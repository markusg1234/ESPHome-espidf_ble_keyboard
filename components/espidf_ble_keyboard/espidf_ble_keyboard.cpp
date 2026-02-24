/**
 * BLE HID Keyboard for ESPHome — Dynamic Key Combos with "combo:" prefix.
 */
#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gatt_defs.h"
#include "esp_bt_defs.h"
#include <cstring>
#include <cstdio> 

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
#define GATTS_APP_ID 0x55

// ... [HID Report Map and Advertising Data stay exactly the same] ...
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

static uint8_t raw_adv_data[] = { 0x02, 0x01, 0x06, 0x03, 0x19, 0xC1, 0x03, 0x03, 0x03, 0x12, 0x18 };
static uint8_t raw_scan_rsp_data[] = { 0x13, 0x09, 'E','S','P','3','2',' ','B','L','E',' ','K','e','y','b','o','a','r','d' };

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20, .adv_int_max = 0x40, .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC, .peer_addr = {0}, .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool s_adv_data_set = false;
static bool s_scan_rsp_data_set = false;

static void do_start_advertising() {
    s_adv_data_set = false; s_scan_rsp_data_set = false;
    esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: s_adv_data_set = true; if (s_scan_rsp_data_set) esp_ble_gap_start_advertising(&adv_params); break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: s_scan_rsp_data_set = true; if (s_adv_data_set) esp_ble_gap_start_advertising(&adv_params); break;
        case ESP_GAP_BLE_SEC_REQ_EVT: esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true); break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) ESP_LOGI(TAG, "GAP: Pairing Successful");
            else ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", param->ble_security.auth_cmpl.fail_reason);
            break;
        default: break;
    }
}

static uint16_t hid_handle_table[13];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_report_handle = 0;

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: s_gatts_if = gatts_if; esp_ble_gap_set_device_name("ESP32 BLE Keyboard"); break;
        case ESP_GATTS_START_EVT: do_start_advertising(); break;
        case ESP_GATTS_CONNECT_EVT:
            if (s_instance) s_instance->set_connected(true, param->connect.conn_id);
            if (s_instance && s_instance->has_passkey()) esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;
        case ESP_GATTS_DISCONNECT_EVT: if (s_instance) s_instance->set_connected(false, 0); esp_ble_gap_start_advertising(&adv_params); break;
        default: break;
    }
}

void EspidfBleKeyboard::setup() {
    s_instance = this;
    nvs_flash_init();
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    if (this->has_passkey_) {
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &this->passkey_, sizeof(uint32_t));
        esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
        esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    }
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);
}

void EspidfBleKeyboard::loop() {}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(20));
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    for (char c : str) {
        report[0] = 0; report[2] = 0;
        if      (c >= 'a' && c <= 'z') { report[2] = (uint8_t)(c - 'a' + 0x04); }
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (uint8_t)(c - 'A' + 0x04); }
        else if (c == ' ') { report[2] = 0x2C; }
        else if (c == '\n') { report[2] = 0x28; }
        else continue;
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        memset(report, 0, 8);
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    send_key_combo(0x05, 0x4C);
}

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;
    
    // Check for "combo:modifiers:keycode" format (LOWERCASE)
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