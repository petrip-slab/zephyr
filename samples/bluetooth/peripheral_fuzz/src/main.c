/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>

static bool hrf_ntf_enabled;
static struct k_sem semmari;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(0x2AFF),),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Use atomic variable, 2 bits for connection and disconnection state */
static ATOMIC_DEFINE(state, 2U);

#define STATE_CONNECTED    1U
#define STATE_DISCONNECTED 2U

static const struct bt_le_conn_param conn_parameters = {
	.interval_min = 10, .interval_max = 10, .latency = 0, .timeout = 100};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		printk("Connected\n");

		(void)atomic_set_bit(state, STATE_CONNECTED);
		bt_conn_le_param_update(conn, &conn_parameters);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
	bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	(void)atomic_set_bit(state, STATE_DISCONNECTED);
	k_sem_give(&semmari);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void hrs_ntf_changed(bool enabled)
{
	hrf_ntf_enabled = enabled;
}

static struct bt_hrs_cb hrs_cb = {
	.ntf_changed = hrs_ntf_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static void enter_passkey(struct bt_conn *conn)
{
	printk("enter_passkey\n");
	bt_conn_auth_passkey_entry(conn, 0);
}

static void confirm_passkey(struct bt_conn *conn, unsigned int passkey)
{
	printk("enter_confirm\n");
	bt_conn_auth_passkey_confirm(conn);
}

static void confirm_pairing(struct bt_conn *conn)
{
	printk("pairing_confirm\n");
	bt_conn_auth_pairing_confirm(conn);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	;
}


static struct bt_conn_auth_cb auth_cb = {.cancel = auth_cancel,
					 .passkey_confirm = confirm_passkey,
					 .passkey_entry = enter_passkey,
					 .pairing_confirm = confirm_pairing,
					 .passkey_display = auth_passkey_display};

static char custom_value[512] = "Initial value";


static ssize_t read_custom(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    const char *value = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(custom_value));
}

static ssize_t write_custom(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    uint8_t *value = attr->user_data;
    if (offset == 0) {
	memset(custom_value, 0, sizeof(custom_value));
    }

    if (offset + len > sizeof(custom_value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(value + offset, buf, len);

    return len;
}


static struct bt_gatt_attr fuzz_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x1bff)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x1b00),
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE |
				       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, read_custom, write_custom,
			       custom_value),
};

static struct bt_gatt_service fuzz_service = BT_GATT_SERVICE(fuzz_attrs);

// Service Inclusion (UUID 0x2802)
static struct bt_gatt_attr service_inclusion_attr[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x2802)),
    BT_GATT_INCLUDE_SERVICE((void *)&fuzz_service)
};

static struct bt_gatt_service service_inclusion_service = BT_GATT_SERVICE(service_inclusion_attr);

static void bt_ready(int err)
{
	k_sem_give(&semmari);
}

static uint8_t print_attr(const struct bt_gatt_attr *attr, uint16_t handle, void *user_data)
{
	char uuid_str[BT_UUID_STR_LEN];
	k_msleep(50);
	bt_uuid_to_str(attr->uuid, uuid_str, sizeof(uuid_str));
	printk("H: 0x%04x, U: %s\n", handle, uuid_str);
	return BT_GATT_ITER_CONTINUE;
}

int main(void)
{
	int err;

	k_sem_init(&semmari, 0, 1);

	err = bt_enable(bt_ready);
	if (err) {
		return 0;
	}
	k_sem_take(&semmari, K_FOREVER);

	err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	if (err) {
		printk("Failed to unpair devices (err %d)\n", err);
	}

	size_t count = 1;
	bt_addr_le_t addr;
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
	printk("Addr : %s\n", addr_str);
	k_msleep(100);
	bt_gatt_service_register(&fuzz_service);
	bt_gatt_service_register(&service_inclusion_service);
	
	bt_gatt_foreach_attr(0x0001, 0xffff, print_attr, NULL);

	bt_conn_auth_cb_register(&auth_cb);
	bt_hrs_cb_register(&hrs_cb);
	bt_passkey_set(0);

	printk("Starting Legacy Advertising (connectable and scannable)\n");
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 0;
	}

	printk("Advertising successfully started\n");

	while (1) {
		k_sem_take(&semmari, K_FOREVER);
		if (atomic_test_and_clear_bit(state, STATE_DISCONNECTED)) {
			printk("Starting Legacy Advertising (connectable and scannable)\n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
			if (err) {
				printk("Advertising failed to start (err %d)\n", err);
				return 0;
			}
		}
	}
	return 0;
}
