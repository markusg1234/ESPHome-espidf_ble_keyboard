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
#include "nvs.h"
#include <cstring>
#include <cstdio>
#include <vector>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
#define GATTS_APP_ID 0x55

// Forward declarations
static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len);
static bool char_to_keycode(char c, uint8_t &modifiers, uint8_t &keycode);

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

// ── Advertising Data ─────────────────────────────────────────────────────────
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,           // Flags: LE General Discoverable + BR/EDR not supported
    0x03, 0x03, 0x12, 0x18,     // Complete List of 16-bit UUIDs: HID (0x1812)
    0x03, 0x19, 0xC1, 0x03      // Appearance: HID Keyboard (0x03C1)
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
static bool s_use_static_passkey = false;
static bool s_require_mitm = false;

static void maybe_reset_bonds_after_security_config_change() {
    if (s_instance == nullptr) {
        return;
    }

    const bool has_passkey = s_instance->has_passkey();
    const uint8_t current_has_passkey = has_passkey ? 1 : 0;
    const uint32_t current_passkey = has_passkey ? s_instance->passkey() : 0;
    const uint8_t current_sc_mode = s_instance->passkey_secure_connections() ? 1 : 0;

    nvs_handle_t handle;
    esp_err_t open_ret = nvs_open("espidf_ble_kb", NVS_READWRITE, &handle);
    if (open_ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS: Failed to open espidf_ble_kb namespace (%d)", open_ret);
        return;
    }

    uint8_t stored_has_passkey = 0;
    uint32_t stored_passkey = 0;
    uint8_t stored_sc_mode = 0;
    bool has_stored_security_cfg = false;

    esp_err_t has_pk_ret = nvs_get_u8(handle, "has_pk", &stored_has_passkey);
    esp_err_t passkey_ret = nvs_get_u32(handle, "passkey", &stored_passkey);
    esp_err_t sc_ret = nvs_get_u8(handle, "pk_sc", &stored_sc_mode);

    if (has_pk_ret == ESP_OK || passkey_ret == ESP_OK || sc_ret == ESP_OK) {
        has_stored_security_cfg = true;
    }

    bool security_cfg_changed = false;
    if (has_stored_security_cfg) {
        security_cfg_changed = (stored_has_passkey != current_has_passkey) ||
                               (stored_sc_mode != current_sc_mode) ||
                               ((current_has_passkey == 1) && (stored_passkey != current_passkey));
    }

    if (security_cfg_changed) {
        ESP_LOGW(TAG, "Security config changed (passkey/mode). Clearing stored BLE bonds.");
        int dev_num = esp_ble_get_bond_device_num();
        if (dev_num > 0) {
            std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
            int query_num = dev_num;
            esp_err_t list_ret = esp_ble_get_bond_device_list(&query_num, bonded.data());
            if (list_ret == ESP_OK) {
                for (int i = 0; i < query_num; i++) {
                    esp_err_t rm_ret = esp_ble_remove_bond_device(bonded[static_cast<size_t>(i)].bd_addr);
                    if (rm_ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to remove bond #%d (%d)", i, rm_ret);
                    }
                }
            } else {
                ESP_LOGW(TAG, "Failed to read bonded device list (%d)", list_ret);
            }
        }
    }

    nvs_set_u8(handle, "has_pk", current_has_passkey);
    nvs_set_u32(handle, "passkey", current_passkey);
    nvs_set_u8(handle, "pk_sc", current_sc_mode);
    nvs_commit(handle);
    nvs_close(handle);
}

static void request_host_friendly_conn_params(const esp_bd_addr_t bda) {
    esp_ble_conn_update_params_t conn_params = {};
    memcpy(conn_params.bda, bda, sizeof(esp_bd_addr_t));
    conn_params.min_int = 0x10;
    conn_params.max_int = 0x20;
    conn_params.latency = 0;
    conn_params.timeout = 400;

    esp_err_t ret = esp_ble_gap_update_conn_params(&conn_params);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GAP: Conn param update request failed (%d)", ret);
    }
}

static void apply_security_params(bool use_static_passkey) {
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    if (use_static_passkey && s_instance && s_instance->has_passkey()) {
        bool use_sc = s_instance->passkey_secure_connections();
        if (use_sc) {
#if defined(ESP_LE_AUTH_REQ_SC_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
            ESP_LOGI(TAG, "Pairing mode: Static passkey (secure-connections MITM bond)");
#elif defined(ESP_LE_AUTH_REQ_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_MITM_BOND;
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, but SC MITM constant unavailable; using legacy MITM bond");
#elif defined(ESP_LE_AUTH_REQ_MITM)
            auth_req = static_cast<esp_ble_auth_req_t>(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, using MITM fallback");
#else
            auth_req = ESP_LE_AUTH_BOND;
            ESP_LOGW(TAG, "Pairing mode secure_connections requested, but MITM unavailable; using bond-only mode");
#endif
        } else {
#if defined(ESP_LE_AUTH_REQ_MITM_BOND)
            auth_req = ESP_LE_AUTH_REQ_MITM_BOND;
#elif defined(ESP_LE_AUTH_REQ_MITM)
            auth_req = static_cast<esp_ble_auth_req_t>(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
#else
            auth_req = ESP_LE_AUTH_BOND;
#endif
            ESP_LOGI(TAG, "Pairing mode: Static passkey (legacy MITM bond)");
        }
        iocap = ESP_IO_CAP_OUT;
        uint32_t passkey = s_instance->passkey();
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
        s_use_static_passkey = true;
        s_require_mitm = true;
        ESP_LOGI(TAG, "Setting passkey: %06lu", (unsigned long) passkey);
    } else {
#if defined(ESP_LE_AUTH_REQ_SC_BOND)
        auth_req = ESP_LE_AUTH_REQ_SC_BOND;
#else
        auth_req = ESP_LE_AUTH_BOND;
#endif
        s_use_static_passkey = false;
        s_require_mitm = false;
        ESP_LOGI(TAG, "Pairing mode: Just Works / host-selected secure bonding");
    }

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

static void do_start_advertising() {
    s_adv_data_set = false;
    s_scan_rsp_data_set = false;

    // Build scan response dynamically from the configured device name.
    // Format: [length = name_len+1] [0x09 = Complete Local Name] [name bytes...]
    std::string dev_name = (s_instance != nullptr) ? s_instance->device_name() : "ESP32 BLE KB";
    std::vector<uint8_t> scan_rsp;
    scan_rsp.push_back((uint8_t)(dev_name.length() + 1));
    scan_rsp.push_back(0x09);
    for (char c : dev_name) scan_rsp.push_back((uint8_t)c);

    esp_err_t adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    esp_err_t scan_ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp.data(), (uint16_t)scan_rsp.size());

    if (adv_ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP: Failed to config adv data (%d)", adv_ret);
        s_adv_data_set = true;
    }
    if (scan_ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP: Failed to config scan rsp data (%d)", scan_ret);
        s_scan_rsp_data_set = true;
    }
    if (s_adv_data_set && s_scan_rsp_data_set) {
        esp_ble_gap_start_advertising(&adv_params);
    }
}

// ── GAP Event Handler ────────────────────────────────────────────────────────
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            s_adv_data_set = true;
            if (s_scan_rsp_data_set) esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
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
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            if (s_instance && s_instance->has_passkey() && s_use_static_passkey) {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, s_instance->passkey());
            } else {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, false, 0);
            }
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGD(TAG, "GAP: Passkey %06lu", (unsigned long) param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "GAP: Pairing Successful");
                if (s_instance) {
                    s_instance->queue_paired_state(true);
                }
            } else {
                uint8_t fail_reason = param->ble_security.auth_cmpl.fail_reason;
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", fail_reason);
                if (s_instance &&
                    s_instance->has_passkey() &&
                    s_use_static_passkey &&
                    !s_instance->passkey_secure_connections() &&
                    fail_reason == 0x51) {
                    ESP_LOGW(TAG, "GAP: Static passkey rejected by peer (0x51), falling back to Just Works mode");
                    apply_security_params(false);
                    esp_ble_remove_bond_device(param->ble_security.auth_cmpl.bd_addr);
                }
                // Advertising restart is handled in DISCONNECT_EVT to avoid duplicate restarts.
            }
            break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
            // Only update paired state if we're actually connected.
            // During 0x51 passkey fallback, bond removal happens while disconnected
            // and should not briefly flash the paired sensor ON.
            if (s_instance && s_instance->is_connected()) {
                s_instance->queue_paired_state(esp_ble_get_bond_device_num() > 0);
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGD(TAG, "GAP: Conn params updated (status=%d int=%u latency=%u timeout=%u)",
                     param->update_conn_params.status,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

// ── GATT Attribute Tables (one per service — ESP-IDF requires separate tables) ─

// Encrypted permission shorthands — iOS requires these on HID characteristics
#define PERM_R          ESP_GATT_PERM_READ
#define PERM_RW         (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define PERM_R_ENC      ESP_GATT_PERM_READ_ENCRYPTED
#define PERM_W_ENC      ESP_GATT_PERM_WRITE_ENCRYPTED
#define PERM_RW_ENC     (ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED)

static const uint16_t UUID_PRI_SERVICE        = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECLARE       = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_CHAR_CLIENT_CONFIG = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t UUID_RPT_REF_DESCR      = ESP_GATT_UUID_RPT_REF_DESCR;
static const uint16_t UUID_DIS_SVC            = 0x180A;
static const uint16_t UUID_PNP_ID             = 0x2A50;
static const uint16_t UUID_MFR_NAME           = 0x2A29;
static const uint16_t UUID_BAS_SVC            = 0x180F;
static const uint16_t UUID_BATTERY_LEVEL      = 0x2A19;
static const uint16_t UUID_HID_SVC            = ESP_GATT_UUID_HID_SVC;
static const uint16_t UUID_HID_INFORMATION    = ESP_GATT_UUID_HID_INFORMATION;
static const uint16_t UUID_HID_REPORT_MAP     = ESP_GATT_UUID_HID_REPORT_MAP;
static const uint16_t UUID_HID_CONTROL_POINT  = ESP_GATT_UUID_HID_CONTROL_POINT;
static const uint16_t UUID_HID_PROTO_MODE     = ESP_GATT_UUID_HID_PROTO_MODE;
static const uint16_t UUID_HID_REPORT         = ESP_GATT_UUID_HID_REPORT;
static const uint16_t UUID_HID_BOOT_KB_INPUT  = 0x2A22;
static const uint16_t UUID_HID_BOOT_KB_OUTPUT = 0x2A32;

static const uint8_t PROP_READ        = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t PROP_WRITE_NR    = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_RW_NR       = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_READ_WRITE  = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t PROP_READ_NOTIFY = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

// ── DIS (Device Information Service) ─────────────────────────────────────────
static uint8_t  pnp_id_val[7]     = {0x01, 0xE5, 0x02, 0xB2, 0xA1, 0x00, 0x01};
static const char mfr_name_val[]  = "Espressif";

enum { DIS_IDX_SVC, DIS_IDX_PNP_CHAR, DIS_IDX_PNP_VAL, DIS_IDX_MFR_CHAR, DIS_IDX_MFR_VAL, DIS_IDX_NB };
static uint16_t dis_handle_table[DIS_IDX_NB];
static const esp_gatts_attr_db_t dis_attr_db[DIS_IDX_NB] = {
    [DIS_IDX_SVC]      = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, 2, 2, (uint8_t *)&UUID_DIS_SVC}},
    [DIS_IDX_PNP_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [DIS_IDX_PNP_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PNP_ID, PERM_R, sizeof(pnp_id_val), sizeof(pnp_id_val), pnp_id_val}},
    [DIS_IDX_MFR_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [DIS_IDX_MFR_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_MFR_NAME, PERM_R, sizeof(mfr_name_val) - 1, sizeof(mfr_name_val) - 1, (uint8_t *)mfr_name_val}},
};

// ── BAS (Battery Service) ────────────────────────────────────────────────────
static uint8_t  battery_level_val = 100;
static uint16_t battery_ccc_val   = 0;

enum { BAS_IDX_SVC, BAS_IDX_BAT_CHAR, BAS_IDX_BAT_VAL, BAS_IDX_BAT_CCC, BAS_IDX_NB };
static uint16_t bas_handle_table[BAS_IDX_NB];
static const esp_gatts_attr_db_t bas_attr_db[BAS_IDX_NB] = {
    [BAS_IDX_SVC]      = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, 2, 2, (uint8_t *)&UUID_BAS_SVC}},
    [BAS_IDX_BAT_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [BAS_IDX_BAT_VAL]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_BATTERY_LEVEL, PERM_R, 1, 1, &battery_level_val}},
    [BAS_IDX_BAT_CCC]  = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW, 2, 2, (uint8_t *)&battery_ccc_val}},
};

// ── HID Service ──────────────────────────────────────────────────────────────
static uint8_t  hid_info_val[4]       = {0x11, 0x01, 0x00, 0x03};
static uint8_t  hid_ctrl_val          = 0;
static uint8_t  proto_mode_val        = 0x01;
static uint8_t  boot_kb_in_val[8]     = {0};
static uint16_t boot_kb_in_ccc_val    = 0;
static uint8_t  boot_kb_out_val[1]    = {0};
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

enum {
    IDX_SVC,
    IDX_CHAR_HID_INFO,     IDX_CHAR_HID_INFO_VAL,
    IDX_CHAR_REPORT_MAP,   IDX_CHAR_REPORT_MAP_VAL,
    IDX_CHAR_HID_CTRL,     IDX_CHAR_HID_CTRL_VAL,
    IDX_CHAR_PROTO_MODE,   IDX_CHAR_PROTO_MODE_VAL,
    IDX_CHAR_BOOT_KB_IN,   IDX_CHAR_BOOT_KB_IN_VAL,
    IDX_CHAR_BOOT_KB_IN_CCC,
    IDX_CHAR_BOOT_KB_OUT,  IDX_CHAR_BOOT_KB_OUT_VAL,
    IDX_CHAR_REPORT,       IDX_CHAR_REPORT_VAL,
    IDX_CHAR_REPORT_CCC,
    IDX_CHAR_REPORT_REF,
    IDX_CHAR_REPORT_OUT,   IDX_CHAR_REPORT_OUT_VAL,
    IDX_CHAR_REPORT_OUT_REF,
    IDX_CHAR_CONSUMER,     IDX_CHAR_CONSUMER_VAL,
    IDX_CHAR_CONSUMER_CCC,
    IDX_CHAR_CONSUMER_REF,
    IDX_CHAR_SYSTEM,       IDX_CHAR_SYSTEM_VAL,
    IDX_CHAR_SYSTEM_CCC,
    IDX_CHAR_SYSTEM_REF,
    HID_IDX_NB,
};

static uint16_t hid_handle_table[HID_IDX_NB];
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_report_handle = 0;
static uint16_t s_hid_output_report_handle = 0;
static uint16_t s_boot_kb_input_handle = 0;
static uint16_t s_boot_kb_output_handle = 0;
static uint16_t s_proto_mode_handle = 0;
static uint16_t s_boot_kb_input_ccc_handle = 0;
static uint16_t s_hid_report_ccc_handle = 0;
static uint16_t s_consumer_report_handle = 0;
static uint16_t s_consumer_ccc_handle = 0;
static uint16_t s_system_report_handle = 0;
static uint16_t s_system_ccc_handle = 0;

static const esp_gatts_attr_db_t hid_attr_db[HID_IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SERVICE, PERM_R, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&UUID_HID_SVC}},
    [IDX_CHAR_HID_INFO] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_HID_INFO_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_INFORMATION, PERM_R_ENC, sizeof(hid_info_val), sizeof(hid_info_val), hid_info_val}},
    [IDX_CHAR_REPORT_MAP] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ}},
    [IDX_CHAR_REPORT_MAP_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT_MAP, PERM_R_ENC, sizeof(hid_report_map), sizeof(hid_report_map), (uint8_t *)hid_report_map}},
    [IDX_CHAR_HID_CTRL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_WRITE_NR}},
    [IDX_CHAR_HID_CTRL_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_CONTROL_POINT, PERM_W_ENC, 1, 1, &hid_ctrl_val}},
    [IDX_CHAR_PROTO_MODE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_RW_NR}},
    [IDX_CHAR_PROTO_MODE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_PROTO_MODE, PERM_RW_ENC, 1, 1, &proto_mode_val}},
    // Boot keyboard input report
    [IDX_CHAR_BOOT_KB_IN] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_BOOT_KB_IN_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_BOOT_KB_INPUT, PERM_R_ENC, sizeof(boot_kb_in_val), sizeof(boot_kb_in_val), boot_kb_in_val}},
    [IDX_CHAR_BOOT_KB_IN_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(boot_kb_in_ccc_val), sizeof(boot_kb_in_ccc_val), (uint8_t *)&boot_kb_in_ccc_val}},
    // Boot keyboard output report
    [IDX_CHAR_BOOT_KB_OUT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_WRITE}},
    [IDX_CHAR_BOOT_KB_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_BOOT_KB_OUTPUT, PERM_RW_ENC, sizeof(boot_kb_out_val), sizeof(boot_kb_out_val), boot_kb_out_val}},
    [IDX_CHAR_REPORT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_REPORT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(report_val), sizeof(report_val), report_val}},
    [IDX_CHAR_REPORT_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(report_ccc_val), sizeof(report_ccc_val), (uint8_t *)&report_ccc_val}},
    [IDX_CHAR_REPORT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(report_ref_val), sizeof(report_ref_val), report_ref_val}},
    [IDX_CHAR_REPORT_OUT] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_WRITE}},
    [IDX_CHAR_REPORT_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_RW_ENC, sizeof(report_out_val), sizeof(report_out_val), report_out_val}},
    [IDX_CHAR_REPORT_OUT_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(report_out_ref_val), sizeof(report_out_ref_val), report_out_ref_val}},
    // Consumer control report (Report ID 2)
    [IDX_CHAR_CONSUMER] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_CONSUMER_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(consumer_val), sizeof(consumer_val), consumer_val}},
    [IDX_CHAR_CONSUMER_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(consumer_ccc_val), sizeof(consumer_ccc_val), (uint8_t *)&consumer_ccc_val}},
    [IDX_CHAR_CONSUMER_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(consumer_ref_val), sizeof(consumer_ref_val), consumer_ref_val}},
    // System control report (Report ID 3)
    [IDX_CHAR_SYSTEM] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_SYSTEM_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(system_val), sizeof(system_val), &system_val}},
    [IDX_CHAR_SYSTEM_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(system_ccc_val), sizeof(system_ccc_val), (uint8_t *)&system_ccc_val}},
    [IDX_CHAR_SYSTEM_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(system_ref_val), sizeof(system_ref_val), system_ref_val}},
};

// Service instance IDs for create_attr_tab
#define SVC_INST_DIS  0
#define SVC_INST_BAS  1
#define SVC_INST_HID  2

static uint8_t s_services_started = 0;

// ── GATTS Event Handler ──────────────────────────────────────────────────────
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name(s_instance ? s_instance->device_name().c_str() : "ESP32 BLE KB");
            // Create each service as a separate attribute table
            esp_ble_gatts_create_attr_tab(dis_attr_db, gatts_if, DIS_IDX_NB, SVC_INST_DIS);
            esp_ble_gatts_create_attr_tab(bas_attr_db, gatts_if, BAS_IDX_NB, SVC_INST_BAS);
            esp_ble_gatts_create_attr_tab(hid_attr_db, gatts_if, HID_IDX_NB, SVC_INST_HID);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Attr table create failed (svc=%u status=%d)",
                         param->add_attr_tab.svc_inst_id, param->add_attr_tab.status);
                break;
            }
            uint8_t svc_id = param->add_attr_tab.svc_inst_id;
            if (svc_id == SVC_INST_DIS) {
                memcpy(dis_handle_table, param->add_attr_tab.handles, sizeof(dis_handle_table));
                esp_ble_gatts_start_service(dis_handle_table[DIS_IDX_SVC]);
            } else if (svc_id == SVC_INST_BAS) {
                memcpy(bas_handle_table, param->add_attr_tab.handles, sizeof(bas_handle_table));
                esp_ble_gatts_start_service(bas_handle_table[BAS_IDX_SVC]);
            } else if (svc_id == SVC_INST_HID) {
                memcpy(hid_handle_table, param->add_attr_tab.handles, sizeof(hid_handle_table));
                s_proto_mode_handle = hid_handle_table[IDX_CHAR_PROTO_MODE_VAL];
                s_boot_kb_input_handle = hid_handle_table[IDX_CHAR_BOOT_KB_IN_VAL];
                s_boot_kb_input_ccc_handle = hid_handle_table[IDX_CHAR_BOOT_KB_IN_CCC];
                s_boot_kb_output_handle = hid_handle_table[IDX_CHAR_BOOT_KB_OUT_VAL];
                s_hid_report_handle = hid_handle_table[IDX_CHAR_REPORT_VAL];
                s_hid_report_ccc_handle = hid_handle_table[IDX_CHAR_REPORT_CCC];
                s_hid_output_report_handle = hid_handle_table[IDX_CHAR_REPORT_OUT_VAL];
                s_consumer_report_handle = hid_handle_table[IDX_CHAR_CONSUMER_VAL];
                s_consumer_ccc_handle = hid_handle_table[IDX_CHAR_CONSUMER_CCC];
                s_system_report_handle = hid_handle_table[IDX_CHAR_SYSTEM_VAL];
                s_system_ccc_handle = hid_handle_table[IDX_CHAR_SYSTEM_CCC];
                esp_ble_gatts_start_service(hid_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_START_EVT:
            s_services_started++;
            ESP_LOGD(TAG, "GATTS: Service started (%u/3)", s_services_started);
            if (s_services_started < 3) break;
            ESP_LOGI(TAG, "GATTS: All services started (DIS + BAS + HID)");
            do_start_advertising();
            break;
        case ESP_GATTS_CONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS: Connected");
            if (s_instance) s_instance->set_connected(true, param->connect.conn_id);
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            battery_ccc_val = 0;
            request_host_friendly_conn_params(param->connect.remote_bda);
            // Trigger encryption with security level matching configured pairing mode
            esp_ble_sec_act_t sec_act = s_require_mitm ? ESP_BLE_SEC_ENCRYPT_MITM : ESP_BLE_SEC_ENCRYPT_NO_MITM;
            esp_ble_set_encryption(param->connect.remote_bda, sec_act);
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Disconnected");
            ESP_LOGD(TAG, "GATTS: Disconnect reason 0x%02X", param->disconnect.reason);
            if (s_instance) {
                s_instance->set_connected(false, 0);
                // Host-side unpair often appears only as disconnect.
                s_instance->queue_paired_state(false);
            }
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            battery_ccc_val = 0;
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_proto_mode_handle && param->write.len > 0) {
                proto_mode_val = param->write.value[0];
                ESP_LOGD(TAG, "GATTS: Protocol mode set to 0x%02X", proto_mode_val);
            }
            if (param->write.handle == s_hid_report_ccc_handle && param->write.len >= 2) {
                report_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                 (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGD(TAG, "GATTS: Report input CCC=0x%04X", report_ccc_val);
            }
            if (param->write.handle == s_boot_kb_input_ccc_handle && param->write.len >= 2) {
                boot_kb_in_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                     (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGD(TAG, "GATTS: Boot KB input CCC=0x%04X", boot_kb_in_ccc_val);
            }
            if (param->write.handle == s_consumer_ccc_handle && param->write.len >= 2) {
                consumer_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                   (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Consumer CCC=0x%04X (media keys)", consumer_ccc_val);
            }
            if (param->write.handle == s_system_ccc_handle && param->write.len >= 2) {
                system_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                 (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: System CCC=0x%04X (power/sleep)", system_ccc_val);
            }
            if ((param->write.handle == s_hid_output_report_handle || param->write.handle == s_boot_kb_output_handle) &&
                param->write.len > 0) {
                ESP_LOGD(TAG, "GATTS: Keyboard LED report 0x%02X", param->write.value[0]);
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

    maybe_reset_bonds_after_security_config_change();

    // Configure security for BLE HID pairing.
    apply_security_params(this->has_passkey_);

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);

    set_connected(false, 0);
    // Pairing sensor starts OFF and turns ON after successful pairing.
    set_paired(false);
}

void EspidfBleKeyboard::loop() {
    if (pending_paired_update_.exchange(false)) {
        set_paired(pending_paired_state_.load());
    }

    // Non-blocking string typing: one keystroke step per loop() call, paced by timer.
    if (type_queue_.empty() || !is_connected_) return;

    uint32_t now = millis();
    if (now < type_next_ms_) return;

    uint8_t report[8] = {0};

    if (type_key_up_pending_) {
        // Send key-up (all zeros), then advance to next character.
        send_keyboard_input_report(conn_id_, report, 8);
        type_key_up_pending_ = false;
        type_index_++;
        if (type_index_ >= type_queue_.size()) {
            type_queue_.clear();
            type_index_ = 0;
        }
        type_next_ms_ = now + 20;
    } else {
        // Send key-down for the current character.
        uint8_t modifiers = 0, keycode = 0;
        char c = type_queue_[type_index_];
        if (char_to_keycode(c, modifiers, keycode)) {
            report[0] = modifiers;
            report[2] = keycode;
            send_keyboard_input_report(conn_id_, report, 8);
            type_key_up_pending_ = true;
        } else {
            // Unsupported character — skip it.
            type_index_++;
            if (type_index_ >= type_queue_.size()) {
                type_queue_.clear();
                type_index_ = 0;
            }
        }
        type_next_ms_ = now + 20;
    }
}

static uint16_t get_keyboard_input_handle() {
    const bool report_notify_enabled = (report_ccc_val & 0x0001) != 0;
    const bool boot_notify_enabled = (boot_kb_in_ccc_val & 0x0001) != 0;

    if (proto_mode_val == 0x00) {
        if (boot_notify_enabled && s_boot_kb_input_handle != 0) {
            return s_boot_kb_input_handle;
        }
        if (report_notify_enabled && s_hid_report_handle != 0) {
            return s_hid_report_handle;
        }
    } else {
        if (report_notify_enabled && s_hid_report_handle != 0) {
            return s_hid_report_handle;
        }
        if (boot_notify_enabled && s_boot_kb_input_handle != 0) {
            return s_boot_kb_input_handle;
        }
    }

    if (s_hid_report_handle != 0) {
        return s_hid_report_handle;
    }
    return s_boot_kb_input_handle;
}

static uint16_t get_alternate_keyboard_input_handle(uint16_t primary) {
    if (primary == s_hid_report_handle) {
        return s_boot_kb_input_handle;
    }
    if (primary == s_boot_kb_input_handle) {
        return s_hid_report_handle;
    }
    if (s_hid_report_handle != 0) {
        return s_hid_report_handle;
    }
    return s_boot_kb_input_handle;
}

static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len) {
    const uint16_t primary_handle = get_keyboard_input_handle();
    if (primary_handle == 0) {
        ESP_LOGW(TAG, "No keyboard input handle available");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_if, conn_id, primary_handle, len, const_cast<uint8_t *>(report), false);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    const uint16_t fallback_handle = get_alternate_keyboard_input_handle(primary_handle);
    if (fallback_handle == 0 || fallback_handle == primary_handle) {
        ESP_LOGW(TAG, "Keyboard report send failed on handle 0x%04X (%d), no fallback handle", primary_handle, ret);
        return ret;
    }

    ESP_LOGW(TAG,
             "Keyboard report send failed on handle 0x%04X (%d), retrying 0x%04X [proto=0x%02X report_ccc=0x%04X boot_ccc=0x%04X]",
             primary_handle,
             ret,
             fallback_handle,
             proto_mode_val,
             report_ccc_val,
             boot_kb_in_ccc_val);
    esp_err_t fallback_ret = esp_ble_gatts_send_indicate(s_gatts_if, conn_id, fallback_handle, len, const_cast<uint8_t *>(report), false);
    if (fallback_ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallback keyboard report send also failed on handle 0x%04X (%d)", fallback_handle, fallback_ret);
    }
    return fallback_ret;
}

// Returns true if the character is supported, filling modifiers and keycode.
static bool char_to_keycode(char c, uint8_t &modifiers, uint8_t &keycode) {
    modifiers = 0;
    keycode   = 0;
    if      (c >= 'a' && c <= 'z') { keycode = (uint8_t)(c - 'a' + 0x04); }
    else if (c >= 'A' && c <= 'Z') { modifiers = 0x02; keycode = (uint8_t)(c - 'A' + 0x04); }
    else if (c >= '1' && c <= '9') { keycode = (uint8_t)(c - '1' + 0x1E); }
    else if (c == '0')  { keycode = 0x27; }
    else if (c == ' ')  { keycode = 0x2C; }
    else if (c == '\n') { keycode = 0x28; }
    else if (c == '.')  { keycode = 0x37; }
    else if (c == ',')  { keycode = 0x36; }
    else if (c == '/')  { keycode = 0x38; }
    else if (c == '\\') { keycode = 0x31; }
    else if (c == '-')  { keycode = 0x2D; }
    else if (c == '=')  { keycode = 0x2E; }
    else if (c == ';')  { keycode = 0x33; }
    else if (c == '\'') { keycode = 0x34; }
    else if (c == '_')  { modifiers = 0x02; keycode = 0x2D; }
    else if (c == '+')  { modifiers = 0x02; keycode = 0x2E; }
    else if (c == ':')  { modifiers = 0x02; keycode = 0x33; }
    else if (c == '!')  { modifiers = 0x02; keycode = 0x1E; }
    else if (c == '?')  { modifiers = 0x02; keycode = 0x38; }
    else if (c == '"')  { modifiers = 0x02; keycode = 0x34; }
    else if (c == '(')  { modifiers = 0x02; keycode = 0x26; }
    else if (c == ')')  { modifiers = 0x02; keycode = 0x27; }
    else if (c == '@')  { modifiers = 0x02; keycode = 0x1F; }
    else if (c == '#')  { modifiers = 0x02; keycode = 0x20; }
    else if (c == '$')  { modifiers = 0x02; keycode = 0x21; }
    else if (c == '%')  { modifiers = 0x02; keycode = 0x22; }
    else if (c == '^')  { modifiers = 0x02; keycode = 0x23; }
    else if (c == '&')  { modifiers = 0x02; keycode = 0x24; }
    else if (c == '*')  { modifiers = 0x02; keycode = 0x25; }
    else return false;
    return true;
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    // Non-blocking: just append to the queue; loop() will drain it one step at a time.
    type_queue_ += str;
}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = modifiers;
    report[2] = keycode;
    send_keyboard_input_report(conn_id_, report, 8);
    vTaskDelay(pdMS_TO_TICKS(30));
    memset(report, 0, 8);
    send_keyboard_input_report(conn_id_, report, 8);
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    if (!is_connected_) return;
    uint8_t report[8] = {0};
    report[0] = 0x05; report[2] = 0x4C;
    send_keyboard_input_report(conn_id_, report, 8);
    vTaskDelay(pdMS_TO_TICKS(50));
    memset(report, 0, 8);
    send_keyboard_input_report(conn_id_, report, 8);
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