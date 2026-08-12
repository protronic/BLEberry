/* SPDX-License-Identifier: Apache-2.0
 *
 * Line-oriented terminal I/O over the Bluetooth Nordic UART Service.
 *
 * RX: bytes arrive from the NUS 'received' callback and are queued in a
 * ring buffer; the REPL thread blocks on a semaphore and assembles lines.
 * TX: output is coalesced into a small buffer and flushed on newline,
 * buffer-full or an explicit flush, then sent as GATT notifications in
 * MTU-sized chunks.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>

#include "nus_io.h"

LOG_MODULE_REGISTER(nus_io, CONFIG_LOG_DEFAULT_LEVEL);

RING_BUF_DECLARE(rx_rb, CONFIG_BLEBERRY_RX_RINGBUF_SIZE);
static K_SEM_DEFINE(rx_sem, 0, 1);

static struct bt_conn *cur_conn;
static atomic_t tx_ready;

static char tx_buf[CONFIG_BLEBERRY_TX_BUF_SIZE];
static size_t tx_len;

void nus_io_rx_feed(const void *data, uint16_t len)
{
	uint32_t written = ring_buf_put(&rx_rb, data, len);

	if (written < len) {
		LOG_WRN("RX overflow, dropped %u bytes", len - written);
	}
	k_sem_give(&rx_sem);
}

void nus_io_set_conn(struct bt_conn *conn)
{
	cur_conn = conn;
}

void nus_io_set_ready(bool ready)
{
	atomic_set(&tx_ready, ready ? 1 : 0);
}

bool nus_io_is_ready(void)
{
	return atomic_get(&tx_ready) == 1;
}

static uint16_t tx_chunk_size(void)
{
	struct bt_conn *conn = cur_conn;

	if (conn != NULL) {
		uint16_t mtu = bt_gatt_get_mtu(conn);

		if (mtu > 3) {
			return MIN(mtu - 3, CONFIG_BLEBERRY_TX_BUF_SIZE);
		}
	}
	return 20; /* default ATT MTU 23 - 3 */
}

static void send_chunked(const char *data, size_t len)
{
	if (!nus_io_is_ready()) {
		return;
	}

	while (len > 0) {
		uint16_t chunk = MIN(len, tx_chunk_size());
		int err = bt_nus_send(NULL, data, chunk);

		if (err == -EAGAIN || err == -ENOMEM) {
			/* controller TX buffers full: back off and retry */
			k_sleep(K_MSEC(5));
			continue;
		}
		if (err) {
			/* peer gone or stack error: drop the rest */
			return;
		}
		data += chunk;
		len -= chunk;
	}
}

void nus_io_flush(void)
{
	if (tx_len > 0) {
		send_chunked(tx_buf, tx_len);
		tx_len = 0;
	}
}

void nus_io_write(const char *buf, size_t len)
{
	bool has_nl = false;

	for (size_t i = 0; i < len; i++) {
		tx_buf[tx_len++] = buf[i];
		if (buf[i] == '\n') {
			has_nl = true;
		}
		if (tx_len == sizeof(tx_buf)) {
			send_chunked(tx_buf, tx_len);
			tx_len = 0;
			has_nl = false;
		}
	}
	if (has_nl) {
		nus_io_flush();
	}
}

void nus_io_send_direct(const char *str)
{
	send_chunked(str, strlen(str));
}

static uint8_t rx_getchar(void)
{
	uint8_t c;

	while (ring_buf_get(&rx_rb, &c, 1) == 0) {
		k_sem_take(&rx_sem, K_FOREVER);
	}
	return c;
}

char *nus_io_read_line(char *buf, size_t size)
{
	size_t pos = 0;

	for (;;) {
		uint8_t c = rx_getchar();

		if (c == '\r' || c == '\n') {
			/* swallow the LF of a CRLF pair */
			if (c == '\r') {
				uint8_t peek;

				if (ring_buf_peek(&rx_rb, &peek, 1) == 1 &&
				    peek == '\n') {
					ring_buf_get(&rx_rb, &peek, 1);
				}
			}
			if (IS_ENABLED(CONFIG_BLEBERRY_ECHO)) {
				nus_io_write("\r\n", 2);
				nus_io_flush();
			}
			buf[pos] = '\0';
			return buf;
		}

		if (c == 0x08 || c == 0x7f) { /* backspace / DEL */
			if (pos > 0) {
				pos--;
				if (IS_ENABLED(CONFIG_BLEBERRY_ECHO)) {
					nus_io_write("\b \b", 3);
					nus_io_flush();
				}
			}
			continue;
		}

		if (c < 0x20) { /* other control characters */
			continue;
		}

		if (pos < size - 1) {
			buf[pos++] = c;
			if (IS_ENABLED(CONFIG_BLEBERRY_ECHO)) {
				nus_io_write((char *)&c, 1);
				nus_io_flush();
			}
		}
	}
}
