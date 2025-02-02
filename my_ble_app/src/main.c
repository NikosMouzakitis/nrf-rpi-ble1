#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/settings/settings.h>  // Include for settings_load
#include <zephyr/random/random.h> //random numbers

/*Raspberry Pi's MAC address */
static const bt_addr_le_t rpi_mac = {
    .type = BT_ADDR_LE_RANDOM, // Change to BT_ADDR_LE_PUBLIC if RPi has a public address
    .a = {{0xB8, 0x27, 0xEB, 0xB7, 0xF5, 0x10}}
};

/* Global variable to track connection state */
static struct bt_conn *current_conn = NULL;
/* Custom Service UUID */
static const struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0));

/* Custom Characteristic UUID */
static const struct bt_uuid_128 custom_char_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

/* Characteristic Value */
double custom_value = 1.52;

/* Callback for when the characteristic is read */
static ssize_t read_custom_val(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    const double *value = attr->user_data; //casting user_data to double pointer
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(double));
}

/* GATT Service and Characteristic Definition */
BT_GATT_SERVICE_DEFINE(custom_svc,
    BT_GATT_PRIMARY_SERVICE(&custom_service_uuid),
    BT_GATT_CHARACTERISTIC(&custom_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_custom_val, NULL, &custom_value),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Advertising Data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)),
};

/* Function to generate a random double in the range [0, 3] */
static double generate_random_double(void)
{
    // Generate a random 32-bit integer
    uint32_t rand_val = sys_rand32_get();

    // Scale the integer to the range [0, 1]
    double scaled_value = (double)rand_val / UINT32_MAX;

    // Scale to the range [0, 3]
    return scaled_value * 3.0;
}

/* Function to print the 6-byte Bluetooth address */
static void print_bluetooth_address(void)
{
    struct bt_le_oob oob;
    int err;

    // Retrieve the local Out-of-Band (OOB) data
    err = bt_le_oob_get_local(BT_ID_DEFAULT, &oob);
    if (err) {
        printk("Failed to get local OOB data (err %d)\n", err);
        return;
    }

    // Print the 6-byte Bluetooth address
    printk("Bluetooth Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           oob.addr.a.val[5], oob.addr.a.val[4], oob.addr.a.val[3],
           oob.addr.a.val[2], oob.addr.a.val[1], oob.addr.a.val[0]);
}

static int restart_advertising(void)
{
    int err;

    printk("Stopping advertising...\n");
    err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return err;
    }

    k_sleep(K_MSEC(1000));  // Wait a bit longer to allow cleanup

    printk("Restarting advertising...\n");

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);

    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    }

    printk("Advertising successfully restarted\n");
    print_bluetooth_address();
    return 0;
}

/* Connection Callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
	printk("Authorized device connected\n");
        printk("Connected\n");
	current_conn = bt_conn_ref(conn); //store connection reference.
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    if(current_conn) {
	bt_conn_unref(current_conn);
	current_conn=NULL;
    }

    printk("Restarting advertising\n");
    int err = -1;
    while(err != 0)
    {
	printk("restaring Advertisement..\n");
        k_sleep(K_SECONDS(1));
	err = restart_advertising();
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Start Advertising */
static void bt_ready(void)
{
    int err;

    printk("Bluetooth initialized\n");
	
    // Add the Raspberry Pi to the accept list (formerly whitelist)
    err = bt_le_filter_accept_list_add(&rpi_mac);
    if (err) {
        printk("Failed to add RPi to accept list (err %d)\n", err);
        return;
    }

    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
    printk("Advertising successfully started\n");
    print_bluetooth_address();
}

/* Main Function */
int main(void)
{
    int err;

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
   
    /* Load settings to restore Bluetooth identity address */
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    bt_ready();

    /* Simulate sending data to the RPi after connection */
    while (1) {
        k_sleep(K_SECONDS(2));
	if(current_conn) {
		custom_value = generate_random_double();
		/* Notify the connected device (RPi) with the custom value */
		bt_gatt_notify(NULL, &custom_svc.attrs[1], &custom_value, sizeof(double));
		printk("Sent data to RPi: %f\n", custom_value);
	} else {
		printk("No active connection.\n");
	}
    }

    return 0;
}
