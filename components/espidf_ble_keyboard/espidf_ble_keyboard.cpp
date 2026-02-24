#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;

// HID Report Map
static const uint8_t hid_report_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
};

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        ESP_LOGI(TAG, "GATT Server Registered (if=%d)", gatts_if);
        esp_ble_gap_set_device_name("ESP32 Keyboard");
    }
    // ... rest of event handler logic (advertising start etc)
}

void EspidfBleKeyboard::setup() {
    s_instance = this;
    ESP_LOGI(TAG, "Initializing Bluetooth...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "BT Controller Init Failed");
        return;
    }
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        ESP_LOGE(TAG, "BT Controller Enable Failed");
        return;
    }
    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid Init Failed");
        return;
    }
    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid Enable Failed");
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0x55);
    ESP_LOGI(TAG, "BLE Setup Complete");
}

void EspidfBleKeyboard::loop() {}

// ... (Rest of send_string and send_key_combo functions) ...

void EspidfBleKeyboardButton::press_action() {
    if (!parent_ || !parent_->is_connected()) {
        ESP_LOGW(TAG, "Cannot send: Not connected to Bluetooth");
        return;
    }
    if (action_.find("combo:") == 0) {
        int mod, key;
        if (sscanf(action_.c_str(), "combo:%i:%i", &mod, &key) == 2) {
            parent_->send_key_combo((uint8_t)mod, (uint8_t)key);
            return;
        }
    }
    parent_->send_string(action_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome