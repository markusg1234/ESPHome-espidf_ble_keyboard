#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static uint16_t s_hid_report_handle = 0x2A; // Standard HID Report Handle

static esp_ble_adv_params_t h_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("ESP32 Keyboard");
            
            esp_ble_adv_data_t adv_data = {};
            adv_data.set_scan_rsp = false;
            adv_data.include_name = true;
            adv_data.include_txpower = true;
            adv_data.appearance = 0x03C1; // HID Keyboard
            adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
            
            esp_ble_gap_config_adv_data(&adv_data);
            esp_ble_gap_start_advertising(&h_adv_params);
            ESP_LOGI(TAG, "GATT Registered & Advertising started.");
            break;
        }
        case ESP_GATTS_CONNECT_EVT:
            s_conn_id = param->connect.conn_id;
            if(s_instance) s_instance->set_connected(true, s_conn_id);
            ESP_LOGI(TAG, "Connected to Windows");
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            if(s_instance) s_instance->set_connected(false, 0);
            ESP_LOGI(TAG, "Disconnected. Restarting advertising...");
            esp_ble_gap_start_advertising(&h_adv_params); 
            break;
        default: break;
    }
}

void EspidfBleKeyboard::setup() {
    s_instance = this;
    ESP_LOGI(TAG, "Starting BLE Setup (Rev 1.0 Fix)...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_set_security_param(ESP_BLE_SM_CLEAR_STATIC_LIST, NULL, 0);
    
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
    vTaskDelay(pdMS_TO_TICKS(30));
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_hid_report_handle, 8, report, false);
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    for (char c : str) {
        uint8_t report[8] = {0};
        if (c >= 'a' && c <= 'z') report[2] = (c - 'a' + 0x04);
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (c - 'A' + 0x04); }
        else if (c == ' ') report[2] = 0x2C;
        else if (c == '\n') report[2] = 0x28;

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