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

static void log_bd_addr(const char *prefix, const esp_bd_addr_t bda) {
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X",
             prefix,
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

// ── HID Report Descriptor ────────────────────────────────────────────────────
// Report ID 1: Standard keyboard (8 bytes)
// Report ID 2: Consumer control — media keys (2 bytes)
// Report ID 3: System control — power/sleep (1 byte)
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
    // ---- Consumer Control (Report ID 2) — media keys ----
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
    0xC0,              // End Collection
    // ---- System Control (Report ID 3) — power/sleep ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x80,        // Usage (System Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0xFF,        //   Usage Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0               // End Collection
};

// ── Raw Advertising Data ─────────────────────────────────────────────────────
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
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP: Advertising started");
            } else {
                ESP_LOGE(TAG, "GAP: Advertising start failed (%d)", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "GAP: Security request");
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            ESP_LOGI(TAG, "GAP: Passkey requested");
            if (s_instance && s_instance->has_passkey()) {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, s_instance->passkey());
            } else {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, false, 0);
            }
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGI(TAG, "GAP: Passkey %06lu", (unsigned long) param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                log_bd_addr("GAP: Paired with", param->ble_security.auth_cmpl.bd_addr);
                ESP_LOGI(TAG, "GAP: Pairing Successful");
            } else {
                log_bd_addr("GAP: Pairing failed with", param->ble_security.auth_cmpl.bd_addr);
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", param->ble_security.auth_cmpl.fail_reason);
                esp_ble_remove_bond_device(param->ble_security.auth_cmpl.bd_addr);
                esp_ble_gap_start_advertising(&adv_params);
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
    IDX_CHAR_REPORT_OUT,   IDX_CHAR_REPORT_OUT_VAL,
    IDX_CHAR_REPORT_OUT_REF,
    // Consumer control report (Report ID 2)
    IDX_CHAR_CONSUMER,     IDX_CHAR_CONSUMER_VAL,
    IDX_CHAR_CONSUMER_CCC,
    IDX_CHAR_CONSUMER_REF,
    // System control report (Report ID 3)
    IDX_CHAR_SYSTEM,       IDX_CHAR_SYSTEM_VAL,
    IDX_CHAR_SYSTEM_CCC,
    IDX_CHAR_SYSTEM_REF,
    HID_IDX_NB,
};

static uint16_t hid_handle_table[HID_IDX_NB];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_report_handle = 0;
static uint16_t s_hid_output_report_handle = 0;
static uint16_t s_consumer_report_handle = 0;
static uint16_t s_system_report_handle = 0;

static uint8_t  hid_info_val[4]       = {0x11, 0x01, 0x00, 0x03};
static uint8_t  hid_ctrl_val          = 0;
static uint8_t  proto_mode_val        = 0x01;
static uint8_t  report_val[8]         = {0};
static uint16_t report_ccc_val        = 0;
static uint8_t  report_ref_val[2]     = {0x01, 0x01};
static uint8_t  report_out_val[1]     = {0};
static uint8_t  report_out_ref_val[2] = {0x01, 0x02};
static uint8_t  consumer_val[2]       = {0};
static uint16_t consumer_ccc_val      = 0;
static uint8_t  consumer_ref_val[2]   = {0x02, 0x01};
static uint8_t  system_val            = 0;
static uint16_t system_ccc_val        = 0;
static uint8_t  system_ref_val[2]     = {0x03, 0x01};

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
static const uint8_t PROP_READ_WRITE  = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
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
    [IDX_CHAR_REPORT_OUT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ_WRITE}},
    [IDX_CHAR_REPORT_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(report_out_val), sizeof(report_out_val), report_out_val}},
    [IDX_CHAR_REPORT_OUT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ, sizeof(report_out_ref_val), sizeof(report_out_ref_val), report_out_ref_val}},
    // Consumer control report (Report ID 2)
    [IDX_CHAR_CONSUMER] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_CONSUMER_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, ESP_GATT_PERM_READ, sizeof(consumer_val), sizeof(consumer_val), consumer_val}},
    [IDX_CHAR_CONSUMER_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(consumer_ccc_val), sizeof(consumer_ccc_val), (uint8_t *)&consumer_ccc_val}},
    [IDX_CHAR_CONSUMER_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ, sizeof(consumer_ref_val), sizeof(consumer_ref_val), consumer_ref_val}},
    // System control report (Report ID 3)
    [IDX_CHAR_SYSTEM] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_SYSTEM_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, ESP_GATT_PERM_READ, sizeof(system_val), sizeof(system_val), &system_val}},
    [IDX_CHAR_SYSTEM_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(system_ccc_val), sizeof(system_ccc_val), (uint8_t *)&system_ccc_val}},
    [IDX_CHAR_SYSTEM_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ, sizeof(system_ref_val), sizeof(system_ref_val), system_ref_val}},
};

// ── GATTS Event Handler ──────────────────────────────────────────────────────
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            s_gatts_if = gatts_if;
            ESP_LOGI(TAG, "GATTS: Registered");
            esp_ble_gap_set_device_name("ESP32 BLE Keyboard");
            esp_ble_gatts_create_attr_tab(hid_attr_db, gatts_if, HID_IDX_NB, 0);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            ESP_LOGI(TAG, "GATTS: Attribute table created");
            memcpy(hid_handle_table, param->add_attr_tab.handles, sizeof(hid_handle_table));
            s_hid_report_handle = hid_handle_table[IDX_CHAR_REPORT_VAL];
            s_hid_output_report_handle = hid_handle_table[IDX_CHAR_REPORT_OUT_VAL];
            s_consumer_report_handle = hid_handle_table[IDX_CHAR_CONSUMER_VAL];
            s_system_report_handle = hid_handle_table[IDX_CHAR_SYSTEM_VAL];
            esp_ble_gatts_start_service(hid_handle_table[IDX_SVC]);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "GATTS: Service started");
            do_start_advertising();
            break;
        case ESP_GATTS_CONNECT_EVT: {
            log_bd_addr("GATTS: Connected", param->connect.remote_bda);
            if (s_instance) s_instance->set_connected(true, param->connect.conn_id);
            // Trigger encryption with security level matching configured pairing mode
            esp_ble_sec_act_t sec_act = ESP_BLE_SEC_ENCRYPT_NO_MITM;
            if (s_instance && s_instance->has_passkey()) {
                sec_act = ESP_BLE_SEC_ENCRYPT_MITM;
            }
            esp_ble_set_encryption(param->connect.remote_bda, sec_act);
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT:
            log_bd_addr("GATTS: Disconnected", param->disconnect.remote_bda);
            ESP_LOGI(TAG, "GATTS: Disconnect reason 0x%02X", param->disconnect.reason);
            if (s_instance) s_instance->set_connected(false, 0);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_hid_output_report_handle && param->write.len > 0) {
                ESP_LOGI(TAG, "GATTS: Keyboard LED report 0x%02X", param->write.value[0]);
            }
            break;
        default:
            break;
    }
}

// ── Component Setup ──────────────────────────────────────────────────────────
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

    // Configure security for BLE HID pairing (Android prefers LE Secure Connections)
    {
        esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
        if (this->has_passkey_) {
    #if defined(ESP_LE_AUTH_REQ_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_MITM_BOND;
    #elif defined(ESP_LE_AUTH_REQ_MITM)
            auth_req = static_cast<esp_ble_auth_req_t>(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
    #else
            auth_req = ESP_LE_AUTH_BOND;
    #endif
        }
        esp_ble_io_cap_t iocap = this->has_passkey_ ? ESP_IO_CAP_OUT : ESP_IO_CAP_NONE;
        uint8_t key_size = 16;
        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

        if (this->has_passkey_) {
            ESP_LOGI(TAG, "Setting passkey: %06lu", (unsigned long) this->passkey_);
            esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &this->passkey_, sizeof(uint32_t));
        }

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
    report[0] = modifiers;
    report[2] = keycode;
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_hid_report_handle, 8, report, false);
    vTaskDelay(pdMS_TO_TICKS(30));
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
    uint8_t report[1] = {0x82};  // System Sleep
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    ESP_LOGI(TAG, "System Sleep sent");
}

void EspidfBleKeyboard::send_shutdown() {
    if (!is_connected_) return;
    send_power();
}

void EspidfBleKeyboard::send_consumer(uint16_t usage) {
    if (!is_connected_) return;
    uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[2] = {0, 0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, release, false);
    ESP_LOGI(TAG, "Consumer report sent: 0x%04X", usage);
}

void EspidfBleKeyboard::send_power() {
    if (!is_connected_) return;
    uint8_t report[1] = {0x81};  // System Power Down
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    ESP_LOGI(TAG, "System Power Down sent");
}

void EspidfBleKeyboard::send_media_play_pause() { send_consumer(0x00CD); }
void EspidfBleKeyboard::send_media_next()        { send_consumer(0x00B5); }
void EspidfBleKeyboard::send_media_prev()        { send_consumer(0x00B6); }
void EspidfBleKeyboard::send_media_stop()        { send_consumer(0x00B7); }
void EspidfBleKeyboard::send_volume_up()         { send_consumer(0x00E9); }
void EspidfBleKeyboard::send_volume_down()       { send_consumer(0x00EA); }
void EspidfBleKeyboard::send_mute()              { send_consumer(0x00E2); }

void EspidfBleKeyboard::send_hibernate() {
    if (!is_connected_) return;
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    send_string("shutdown /h");
    vTaskDelay(pdMS_TO_TICKS(200));
    send_key_combo(0x00, 0x28);
}

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;

    // Check if the action string starts with "combo:"
    if (action_.find("combo:") == 0) {
        int mod, key;
        if (sscanf(action_.c_str(), "combo:%i:%i", &mod, &key) == 2) {
            parent_->send_key_combo((uint8_t)mod, (uint8_t)key);
            return;
        }
    }

    // Check if the action string starts with "consumer:"
    if (action_.find("consumer:") == 0) {
        int usage;
        if (sscanf(action_.c_str(), "consumer:%i", &usage) == 1) {
            parent_->send_consumer((uint16_t)usage);
            return;
        }
    }

    // Named actions
    if (action_ == "ctrl_alt_del")      parent_->send_ctrl_alt_del();
    else if (action_ == "sleep")        parent_->send_sleep();
    else if (action_ == "shutdown")     parent_->send_shutdown();
    else if (action_ == "hibernate")    parent_->send_hibernate();
    else if (action_ == "power")        parent_->send_power();
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