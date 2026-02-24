#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static uint16_t s_hid_report_handle = 42; // Placeholder, usually handled via table

void EspidfBleKeyboard::setup() {
    s_instance = this;
    ESP_LOGI(TAG, "Initializing Bluetooth...");
    // initialization logic here...
}

void EspidfBleKeyboard::loop() {}

// --- The missing Linker functions start here ---

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    delay(50);
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    for (char c : str) {
        uint8_t report[8] = {0};
        // Simple ASCII to HID mapping
        if (c >= 'a' && c <= 'z') report[2] = (c - 'a' + 0x04);
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (c - 'A' + 0x04); }
        else if (c == ' ') report[2] = 0x2C;

        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        delay(20);
        memset(report, 0, 8);
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        delay(20);
    }
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    send_key_combo(0x05, 0x4C); // 0x05 = Ctrl + Alt, 0x4C = Delete
}

// --- Button implementation ---

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;
    
    if (action_.find("combo:") == 0) {
        int mod, key;
        if (sscanf(action_.c_str(), "combo:%i:%i", &mod, &key) == 2) {
            parent_->send_key_combo((uint8_t)mod, (uint8_t)key);
            return;
        }
    }
    
    if (action_ == "ctrl_alt_del") {
        parent_->send_ctrl_alt_del();
    } else {
        parent_->send_string(action_);
    }
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome