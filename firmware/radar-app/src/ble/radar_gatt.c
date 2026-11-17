/* Application-owned database. GAP/stack parameters come from design.cybt. */
#include "wiced_bt_gatt.h"
#include "cycfg_gatt_db.h"
#include "radar_ble.h"

const uint8_t gatt_database[] = {
    PRIMARY_SERVICE_UUID16(1, 0x1800),
    CHARACTERISTIC_UUID16(2, 3, 0x2a00, GATTDB_CHAR_PROP_READ, GATTDB_PERM_READABLE),
    CHARACTERISTIC_UUID16(4, 5, 0x2a01, GATTDB_CHAR_PROP_READ, GATTDB_PERM_READABLE),
    PRIMARY_SERVICE_UUID16(6, 0x1801),
    CHARACTERISTIC_UUID16(7, 8, 0x2a05, GATTDB_CHAR_PROP_INDICATE, GATTDB_PERM_NONE),
    CHAR_DESCRIPTOR_UUID16_WRITABLE(9, 0x2902, GATTDB_PERM_READABLE | GATTDB_PERM_WRITE_REQ),
    PRIMARY_SERVICE_UUID128(0x20, RADAR_SERVICE_UUID),
    CHARACTERISTIC_UUID128_WRITABLE(0x21, RADAR_RX_HANDLE, RADAR_RX_UUID,
        GATTDB_CHAR_PROP_WRITE, GATTDB_PERM_WRITE_REQ),
    CHARACTERISTIC_UUID128(0x23, RADAR_TX_HANDLE, RADAR_TX_UUID,
        GATTDB_CHAR_PROP_NOTIFY, GATTDB_PERM_NONE),
    CHAR_DESCRIPTOR_UUID16_WRITABLE(RADAR_CCCD_HANDLE, 0x2902,
        GATTDB_PERM_READABLE | GATTDB_PERM_WRITE_REQ),
};
const uint16_t gatt_database_len = sizeof(gatt_database);
uint8_t app_gap_device_name[] = "Radar-CSK";
static uint8_t appearance[2], changed[4], changed_cccd[2];
uint8_t radar_cccd[2];
gatt_db_lookup_table_t app_gatt_db_ext_attr_tbl[] = {
    {3, sizeof(app_gap_device_name)-1, sizeof(app_gap_device_name)-1, app_gap_device_name},
    {5, 2, 2, appearance}, {8, 4, 4, changed}, {9, 2, 2, changed_cccd},
    {RADAR_CCCD_HANDLE, 2, 2, radar_cccd}
};
const uint16_t app_gatt_db_ext_attr_tbl_size = sizeof(app_gatt_db_ext_attr_tbl)/sizeof(app_gatt_db_ext_attr_tbl[0]);
