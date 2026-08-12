/* SPDX-License-Identifier: Apache-2.0
 *
 * Line-oriented terminal I/O over the Bluetooth Nordic UART Service.
 */
#ifndef BLEBERRY_NUS_IO_H_
#define BLEBERRY_NUS_IO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct bt_conn;

/* Feed bytes received from the NUS RX characteristic (any context). */
void nus_io_rx_feed(const void *data, uint16_t len);

/* Track the peer connection (for MTU-sized chunking). */
void nus_io_set_conn(struct bt_conn *conn);

/* Notification subscription state of the TX characteristic. */
void nus_io_set_ready(bool ready);
bool nus_io_is_ready(void);

/* Buffered write; flushes on '\n' or when the buffer fills. */
void nus_io_write(const char *buf, size_t len);

/* Flush the coalescing buffer to the peer. */
void nus_io_flush(void);

/* Send a string immediately, bypassing the coalescing buffer.
 * Safe to call from callbacks/work queue context. */
void nus_io_send_direct(const char *str);

/* Blocking: read one line (without the trailing newline) into buf,
 * handling backspace and optional echo. Returns buf. */
char *nus_io_read_line(char *buf, size_t size);

#endif /* BLEBERRY_NUS_IO_H_ */
