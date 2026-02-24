#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h" // Essential for delay()
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
static uint16_t s_hid_report_handle = 0;

// Proper HID Report Map for a Keyboard
static const uint8_t hid_report_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
};

// ... [The GATT/GAP handlers must be present here to start advertising] ...
// (Omitting full boilerplate for brevity, but ensure your previous GATT logic is included)

void EspidfBleKeyboard::setup() {
    s_instance = this;
    ESP_LOGI(TAG, "Starting BLE Setup for Rev 1.0 ESP32...");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_ble_gatts_register_callback([](esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
        if (event == ESP_GATTS_REG_EVT) {
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("ESP32 Keyboard");
            // Kick off advertising logic here
        }
    });
    esp_ble_gatts_app_register(0x55);
}

void EspidfBleKeyboard::loop() {}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(50)); // Corrected delay for ESP-IDF
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    for (char c : str) {
        uint8_t report[8] = {0};
        if (c >= 'a' && c <= 'z') report[2] = (c - 'a' + 0x04);
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (c - 'A' + 0x04); }
        else if (c == ' ') report[2] = 0x2C;

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