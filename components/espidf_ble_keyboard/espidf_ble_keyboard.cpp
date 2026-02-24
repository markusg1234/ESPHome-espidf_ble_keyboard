#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gatt_defs.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "nvs_flash.h"
#include <cstring>
#include <cstdio>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;

static const uint8_t hid_report_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0
};

static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static uint16_t s_hid_report_handle = 0;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// ── Attributes ──
enum {
    IDX_SVC,
    IDX_CHAR_HID_INFO,     IDX_CHAR_HID_INFO_VAL,
    IDX_CHAR_REPORT_MAP,   IDX_CHAR_REPORT_MAP_VAL,
    IDX_CHAR_HID_CTRL,     IDX_CHAR_HID_CTRL_VAL,
    IDX_CHAR_PROTO_MODE,   IDX_CHAR_PROTO_MODE_VAL,
    IDX_CHAR_REPORT,       IDX_CHAR_REPORT_VAL,
    IDX_CHAR_REPORT_CCC,
    IDX_CHAR_REPORT_REF,
    HID_IDX_NB,
};

static uint16_t hid_handle_table[HID_IDX_NB];
static uint8_t hid_info_val[4] = {0x11, 0x01, 0x00, 0x01};
static uint8_t hid_ctrl_val = 0;
static uint8_t proto_mode_val = 0x01;
static uint8_t report_val[8] = {0};
static uint16_t report_ccc_val = 0;
static uint8_t report_ref_val[2] = {0x01, 0x01};

static const esp_gatts_attr_db_t hid_attr_db[HID_IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_PRI_SERVICE}, ESP_GATT_PERM_READ, 2, 2, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_SVC}}},
    [IDX_CHAR_HID_INFO] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ}}},
    [IDX_CHAR_HID_INFO_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_INFORMATION}, ESP_GATT_PERM_READ, 4, 4, hid_info_val}},
    [IDX_CHAR_REPORT_MAP] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ}}},
    [IDX_CHAR_REPORT_MAP_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_REPORT_MAP}, ESP_GATT_PERM_READ, sizeof(hid_report_map), sizeof(hid_report_map), (uint8_t *)hid_report_map}},
    [IDX_CHAR_HID_CTRL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_WRITE_NR}}},
    [IDX_CHAR_HID_CTRL_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_CONTROL_POINT}, ESP_GATT_PERM_WRITE, 1, 1, &hid_ctrl_val}},
    [IDX_CHAR_PROTO_MODE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR}}},
    [IDX_CHAR_PROTO_MODE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_PROTO_MODE}, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 1, 1, &proto_mode_val}},
    [IDX_CHAR_REPORT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&(uint8_t){ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY}}},
    [IDX_CHAR_REPORT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_HID_REPORT}, ESP_GATT_PERM_READ, 8, 8, report_val}},
    [IDX_CHAR_REPORT_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_CLIENT_CONFIG}, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, (uint8_t *)&report_ccc_val}},
    [IDX_CHAR_REPORT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_RPT_REF_DESCR}, ESP_GATT_PERM_READ, 2, 2, report_ref_val}},
};

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("ESP32 Keyboard");
            esp_ble_gatts_create_attr_tab(hid_attr_db, gatts_if, HID_IDX_NB, 0);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            memcpy(hid_handle_table, param->add_attr_tab.handles, sizeof(hid_handle_table));
            s_hid_report_handle = hid_handle_table[IDX_CHAR_REPORT_VAL];
            esp_ble_gatts_start_service(hid_handle_table[IDX_SVC]);
            break;
        case ESP_GATTS_START_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CONNECT_EVT:
            s_conn_id = param->connect.conn_id;
            if (s_instance) s_instance->set_connected(true, s_conn_id);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            if (s_instance) s_instance->set_connected(false, 0);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default: break;
    }
}

void EspidfBleKeyboard::setup() {
    s_instance = this;
    nvs_flash_init();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0x55);
}

void EspidfBleKeyboard::loop() {}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_hid_report_handle, 8, report, false);
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    for (char c : str) {
        uint8_t report[8] = {0};
        if (c >= 'a' && c <= 'z') report[2] = (uint8_t)(c - 'a' + 0x04);
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (uint8_t)(c - 'A' + 0x04); }
        else if (c == ' ') report[2] = 0x2C;
        else if (c == '\n') report[2] = 0x28;
        else continue;

        esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        memset(report, 0, 8);
        esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void EspidfBleKeyboard::send_ctrl_alt_del() { send_key_combo(0x05, 0x4C); }

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;
    if (action_.find("combo:") == 0) {
        int mod, key;
        if (sscanf(action_.c_str(), "combo:%i:%i", &mod, &key) == 2) {
            parent_->send_key_combo((uint8_t)mod, (uint8_t)key);
            return;
        }
    }
    if (action_ == "ctrl_alt_del") parent_->send_ctrl_alt_del();
    else parent_->send_string(action_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome