/* SPDX-License-Identifier: Apache-2.0
 *
 * BLEberry: Berry language REPL over the Bluetooth Nordic UART Service.
 *
 * Advertises as a connectable peripheral with the NUS UUID; once a
 * central subscribes to the TX characteristic, the Berry REPL banner
 * and prompt are sent and the terminal is live.
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>

#include "nus_io.h"

LOG_MODULE_REGISTER(bleberry, CONFIG_LOG_DEFAULT_LEVEL);

const char *bleberry_banner(void);

#define DEVICE_NAME	CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void adv_restart(struct k_work *work)
{
	ARG_UNUSED(work);

	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
				  ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err && err != -EALREADY) {
		LOG_ERR("Advertising restart failed (err %d)", err);
	}
}
static K_WORK_DEFINE(adv_work, adv_restart);

static void send_banner(struct k_work *work)
{
	ARG_UNUSED(work);

	nus_io_send_direct(bleberry_banner());
}
static K_WORK_DEFINE(banner_work, send_banner);

/* --- NUS callbacks ------------------------------------------------------ */

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	nus_io_set_ready(enabled);
	LOG_INF("NUS notifications %s", enabled ? "enabled" : "disabled");

	if (enabled) {
		k_work_submit(&banner_work);
	}
}

static void received(struct bt_conn *conn, const void *data, uint16_t len,
		     void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	nus_io_rx_feed(data, len);
}

static struct bt_nus_cb nus_listener = {
	.notif_enabled = notif_enabled,
	.received = received,
};

/* --- connection callbacks ----------------------------------------------- */

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
		return;
	}
	nus_io_set_conn(conn);
	LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);

	nus_io_set_conn(NULL);
	nus_io_set_ready(false);
	LOG_INF("Disconnected (reason 0x%02x)", reason);
}

static void recycled(void)
{
	k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

int main(void)
{
	int err;

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		LOG_ERR("Failed to register NUS callbacks (err %d)", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Failed to enable Bluetooth (err %d)", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
			      ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Failed to start advertising (err %d)", err);
		return err;
	}

	LOG_INF("BLEberry up, advertising as \"%s\"", DEVICE_NAME);
	return 0;
}
