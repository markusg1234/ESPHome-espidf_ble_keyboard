/**
 * BLE HID Keyboard for ESPHome — Fixed Raw Advertising & YAML Passkey logic.
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

// ── HID Report Descriptor ────────────────────────────────────────────────────
// Report ID 1: Standard keyboard (8 bytes)
// Report ID 2: Consumer control — power, media keys (2 bytes)
static const uint8_t hid_report_map[] = {
    // ---- Keyboard (Report ID 1) ----
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0,
    // ---- Consumer Control (Report ID 2) ----
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0               // End Collection
};

// ── Raw Advertising Data (Verified Working) ──────────────────────────────────
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,           // Flags
    0x03, 0x19, 0xC1, 0x03,     // Appearance: HID Keyboard (0x03C1)
    0x03, 0x03, 0x12, 0x18      // Complete UUID16: HID service (0x1812)
};

static uint8_t raw_scan_rsp_data[] = {
    0x13, 0x09,                 // Local Name
    'E','S','P','3','2',' ','B','L','E',' ','K','e','y','b','o','a','r','d'
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr         = {0},
    .peer_addr_type    = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool s_adv_data_set = false;
static bool s_scan_rsp_data_set = false;

static void do_start_advertising() {
    s_adv_data_set = false;
    s_scan_rsp_data_set = false;
    esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
}

// ── GAP Event Handler ────────────────────────────────────────────────────────
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            s_adv_data_set = true;
            if (s_scan_rsp_data_set) esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            s_scan_rsp_data_set = true;
            if (s_adv_data_set) esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "GAP: Pairing Successful");
            } else {
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        default:
            break;
    }
}

// ── GATT Attribute Table ─────────────────────────────────────────────────────
enum {
    IDX_SVC,
    IDX_CHAR_HID_INFO,     IDX_CHAR_HID_INFO_VAL,
    IDX_CHAR_REPORT_MAP,   IDX_CHAR_REPORT_MAP_VAL,
    IDX_CHAR_HID_CTRL,     IDX_CHAR_HID_CTRL_VAL,
    IDX_CHAR_PROTO_MODE,   IDX_CHAR_PROTO_MODE_VAL,
    // Keyboard report (Report ID 1)
    IDX_CHAR_REPORT,       IDX_CHAR_REPORT_VAL,
    IDX_CHAR_REPORT_CCC,
    IDX_CHAR_REPORT_REF,
    // Consumer control report (Report ID 2)
    IDX_CHAR_CONSUMER,     IDX_CHAR_CONSUMER_VAL,
    IDX_CHAR_CONSUMER_CCC,
    IDX_CHAR_CONSUMER_REF,
    HID_IDX_NB,
};

static uint16_t hid_handle_table[HID_IDX_NB];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_report_handle = 0;
static uint16_t s_consumer_report_handle = 0;

static uint8_t  hid_info_val[4]   = {0x11, 0x01, 0x00, 0x01};
static uint8_t  hid_ctrl_val      = 0;
static uint8_t  proto_mode_val    = 0x01;
static uint8_t  report_val[8]     = {0};
static uint16_t report_ccc_val    = 0;
static uint8_t  report_ref_val[2]     = {0x01, 0x01};
static uint8_t  consumer_val[2]       = {0};
static uint16_t consumer_ccc_val      = 0;
static uint8_t  consumer_ref_val[2]   = {0x02, 0x01};

static const uint16_t UUID_PRI_SERVICE        = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_HID_SVC            = ESP_GATT_UUID_HID_SVC;
static const uint16_t UUID_CHAR_DECLARE       = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_HID_INFORMATION    = ESP_GATT_UUID_HID_INFORMATION;
static const uint16_t UUID_HID_REPORT_MAP     = ESP_GATT_UUID_HID_REPORT_MAP;
static const uint16_t UUID_HID_CONTROL_POINT  = ESP_GATT_UUID_HID_CONTROL_POINT;
static const uint16_t UUID_HID_PROTO_MODE     = ESP_GATT_UUID_HID_PROTO_MODE;
static const uint16_t UUID_HID_REPORT         = ESP_GATT_UUID_HID_REPORT;
static const uint16_t UUID_CHAR_CLIENT_CONFIG = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t UUID_RPT_REF_DESCR      = ESP_GATT_UUID_RPT_REF_DESCR;

static const uint8_t PROP_READ        = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t PROP_WRITE_NR    = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_RW_NR       = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_READ_NOTIFY = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const esp_gatts_attr_db_t hid_attr_db[HID_IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&UUID_HID_SVC}},
    [IDX_CHAR_HID_INFO] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_HID_INFO_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_INFORMATION, ESP_GATT_PERM_READ, sizeof(hid_info_val), sizeof(hid_info_val), hid_info_val}},
    [IDX_CHAR_REPORT_MAP] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_REPORT_MAP_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT_MAP, ESP_GATT_PERM_READ, sizeof(hid_report_map), sizeof(hid_report_map), (uint8_t *)hid_report_map}},
    [IDX_CHAR_HID_CTRL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_WRITE_NR}},
    [IDX_CHAR_HID_CTRL_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_CONTROL_POINT, ESP_GATT_PERM_WRITE, 1, 1, &hid_ctrl_val}},
    [IDX_CHAR_PROTO_MODE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_RW_NR}},
    [IDX_CHAR_PROTO_MODE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_PROTO_MODE, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 1, 1, &proto_mode_val}},
    [IDX_CHAR_REPORT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_REPORT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, ESP_GATT_PERM_READ, sizeof(report_val), sizeof(report_val), report_val}},
    [IDX_CHAR_REPORT_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(report_ccc_val), sizeof(report_ccc_val), (uint8_t *)&report_ccc_val}},
    [IDX_CHAR_REPORT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ, sizeof(report_ref_val), sizeof(report_ref_val), report_ref_val}},
    // Consumer control report (Report ID 2)
    [IDX_CHAR_CONSUMER] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_CONSUMER_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, ESP_GATT_PERM_READ, sizeof(consumer_val), sizeof(consumer_val), consumer_val}},
    [IDX_CHAR_CONSUMER_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(consumer_ccc_val), sizeof(consumer_ccc_val), (uint8_t *)&consumer_ccc_val}},
    [IDX_CHAR_CONSUMER_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ, sizeof(consumer_ref_val), sizeof(consumer_ref_val), consumer_ref_val}},
};

// ── GATTS Event Handler ──────────────────────────────────────────────────────
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("ESP32 BLE Keyboard");
            esp_ble_gatts_create_attr_tab(hid_attr_db, gatts_if, HID_IDX_NB, 0);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            memcpy(hid_handle_table, param->add_attr_tab.handles, sizeof(hid_handle_table));
            s_hid_report_handle = hid_handle_table[IDX_CHAR_REPORT_VAL];
            s_consumer_report_handle = hid_handle_table[IDX_CHAR_CONSUMER_VAL];
            esp_ble_gatts_start_service(hid_handle_table[IDX_SVC]);
            break;
        case ESP_GATTS_START_EVT:
            do_start_advertising();
            break;
        case ESP_GATTS_CONNECT_EVT:
            if (s_instance) s_instance->set_connected(true, param->connect.conn_id);
            // If passkey is used, trigger security
            if (s_instance && s_instance->has_passkey()) {
                esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            }
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            if (s_instance) s_instance->set_connected(false, 0);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default:
            break;
    }
}

// ── Component Setup ─────────────────────────────────────────────────────────
void EspidfBleKeyboard::setup() {
    s_instance = this;
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

    if (this->has_passkey_) {
        ESP_LOGI(TAG, "Setting passkey in setup: %06d", this->passkey_);
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &this->passkey_, sizeof(uint32_t));

        esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
        esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
        uint8_t key_size = 16;
        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    }

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);
}

void EspidfBleKeyboard::loop() {}

void EspidfBleKeyboard::send_string(const std::string &str) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    for (char c : str) {
        report[0] = 0; report[2] = 0;
        if      (c >= 'a' && c <= 'z') { report[2] = (uint8_t)(c - 'a' + 0x04); }
        else if (c >= 'A' && c <= 'Z') { report[0] = 0x02; report[2] = (uint8_t)(c - 'A' + 0x04); }
        else if (c >= '1' && c <= '9') { report[2] = (uint8_t)(c - '1' + 0x1E); }
        else if (c == '0') { report[2] = 0x27; }
        else if (c == ' ')  { report[2] = 0x2C; }
        else if (c == '\n') { report[2] = 0x28; }
        else if (c == '.')  { report[2] = 0x37; }
        else if (c == ',')  { report[2] = 0x36; }
        else if (c == '/')  { report[2] = 0x38; }
        else if (c == '\\') { report[2] = 0x31; }
        else if (c == '-')  { report[2] = 0x2D; }
        else if (c == '=')  { report[2] = 0x2E; }
        else if (c == ';')  { report[2] = 0x33; }
        else if (c == '\'') { report[2] = 0x34; }
        else if (c == '_')  { report[0] = 0x02; report[2] = 0x2D; }
        else if (c == '+')  { report[0] = 0x02; report[2] = 0x2E; }
        else if (c == ':')  { report[0] = 0x02; report[2] = 0x33; }
        else continue;

        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        memset(report, 0, 8);
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    
    // Set modifier (e.g., Windows Key, Ctrl, Shift) in byte 0
    report[0] = modifiers;
    // Set actual key (e.g., 'R', 'F1') in byte 2
    report[2] = keycode;
    
    // Send press
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(30)); // Brief delay
    
    // Send release (all zeros)
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = 0x05; report[2] = 0x4C;
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    memset(report, 0, 8);
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
}


void EspidfBleKeyboard::send_sleep() {
    if (!is_connected_) return;
    // Win + R to open Run dialog
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    // Type the sleep command
    send_string("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
    vTaskDelay(pdMS_TO_TICKS(200));
    // Press Enter then immediately send all-zeros report to clear key state
    send_key_combo(0x00, 0x28);
    vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t empty[8] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, empty, false);
}

void EspidfBleKeyboard::send_shutdown() {
    if (!is_connected_) return;
    // Win + R to open Run dialog
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    // Type the shutdown command
    send_string("shutdown /s /t 0");
    vTaskDelay(pdMS_TO_TICKS(200));
    // Press Enter then immediately send all-zeros report to clear key state
    send_key_combo(0x00, 0x28);
    vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t empty[8] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, empty, false);
}

void EspidfBleKeyboard::send_consumer(uint16_t usage) {
    if (!is_connected_) return;
    // Send consumer key press
    uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    // Send release
    uint8_t release[2] = {0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, release, false);
}

void EspidfBleKeyboard::send_power() {
    send_consumer(0x0030);  // HID Consumer: Power
}

void EspidfBleKeyboard::send_media_play_pause() {
    send_consumer(0x00CD);  // HID Consumer: Play/Pause
}

void EspidfBleKeyboard::send_media_next() {
    send_consumer(0x00B5);  // HID Consumer: Scan Next Track
}

void EspidfBleKeyboard::send_media_prev() {
    send_consumer(0x00B6);  // HID Consumer: Scan Previous Track
}

void EspidfBleKeyboard::send_media_stop() {
    send_consumer(0x00B7);  // HID Consumer: Stop
}

void EspidfBleKeyboard::send_volume_up() {
    send_consumer(0x00E9);  // HID Consumer: Volume Increment
}

void EspidfBleKeyboard::send_volume_down() {
    send_consumer(0x00EA);  // HID Consumer: Volume Decrement
}

void EspidfBleKeyboard::send_mute() {
    send_consumer(0x00E2);  // HID Consumer: Mute
}


void EspidfBleKeyboard::send_hibernate() {
    if (!is_connected_) return;
    // Win + R to open Run dialog
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    // Type the hibernate command
    send_string("shutdown /h");
    vTaskDelay(pdMS_TO_TICKS(200));
    // Press Enter then immediately send all-zeros report to clear key state
    send_key_combo(0x00, 0x28);
}

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;

    // Check if the action string starts with "combo:"
    if (action_.find("combo:") == 0) {
        int mod, key;
        // %i automatically reads hex values (like 0x08) into integers
        if (sscanf(action_.c_str(), "combo:%i:%i", &mod, &key) == 2) {
            parent_->send_key_combo((uint8_t)mod, (uint8_t)key);
            return;
        }
    }
    
    // Named actions
    if (action_ == "ctrl_alt_del")   parent_->send_ctrl_alt_del();
    else if (action_ == "sleep")     parent_->send_sleep();
    else if (action_ == "shutdown")  parent_->send_shutdown();
    else if (action_ == "hibernate") parent_->send_hibernate();
    else if (action_ == "power")     parent_->send_power();
    else if (action_ == "play_pause")   parent_->send_media_play_pause();
    else if (action_ == "next_track")   parent_->send_media_next();
    else if (action_ == "prev_track")   parent_->send_media_prev();
    else if (action_ == "stop")         parent_->send_media_stop();
    else if (action_ == "volume_up")    parent_->send_volume_up();
    else if (action_ == "volume_down")  parent_->send_volume_down();
    else if (action_ == "mute")         parent_->send_mute();
    else parent_->send_string(action_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome