/**
 * BLE HID Keyboard for ESPHome — Fixed Raw Advertising & YAML Passkey logic.
 */
#include "espidf_ble_keyboard.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gatt_defs.h"
#include "esp_bt_defs.h"
#include "nvs.h"
#include "esp_random.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *TAG = "espidf_ble_keyboard";
static EspidfBleKeyboard *s_instance = nullptr;
#define GATTS_APP_ID 0x55

// Forward declarations
static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len);

// ── HID Report Descriptor ────────────────────────────────────────────────────
// Report ID 1: Standard keyboard (8 bytes)
// Report ID 2: Consumer control — media keys (2 bytes)
// Report ID 3: System control — power/sleep (1 byte)
// Report ID 4: Mouse — buttons + X/Y + scroll (4 bytes)
// Report ID 5: Absolute mouse — buttons + absolute X/Y 0..32767 (5 bytes)
static const uint8_t hid_report_map[] = {
    // ---- Keyboard (Report ID 1) ----
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    // Key array: Logical/Usage Maximum 0x73 (F24) so F13-F24 (0x68-0x73) are in
    // range — hosts silently drop keycodes above the declared maximum. Hosts
    // cache the HID descriptor per bond: re-pair after changing this.
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x73,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x73, 0x81, 0x00,
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
    0xC0,              // End Collection
    // ---- Mouse (Report ID 4) — buttons + X/Y movement + scroll wheel ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1 — Left)
    0x29, 0x03,        //     Usage Maximum (Button 3 — Middle)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x75, 0x05,        //     Report Size (5) — padding
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x01,        //     Input (Constant) — padding to byte boundary
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection (Physical)
    0xC0,              // End Collection (Application)
    // ---- Absolute Mouse (Report ID 5) — exact-position pointer ----
    // Reports absolute X/Y in 0..32767; host maps this range onto the screen
    // (primary monitor or whole virtual desktop, depending on host).
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1 — Left)
    0x29, 0x03,        //     Usage Maximum (Button 3 — Middle)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x75, 0x05,        //     Report Size (5) — padding
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x01,        //     Input (Constant) — padding to byte boundary
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x00, 0x00,  //     Logical Minimum (0)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data, Variable, Absolute) — ABSOLUTE X/Y
    0xC0,              //   End Collection (Physical)
    0xC0               // End Collection (Application)
};

// Keyboard layout ASCII/Unicode tables live in keyboard_layouts.cpp.
// The active layout is selected via active_layout_ (set from YAML and NVS).

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

// Multi-host: when true, next advertising cycle uses directed advertising to target host
static bool s_directed_adv_pending = false;
  static std::atomic<bool> s_directed_adv_active{false};
  static std::atomic<uint32_t> s_directed_adv_start_ms{0};
  static esp_bd_addr_t s_directed_addr = {};
static esp_ble_addr_type_t s_directed_addr_type = BLE_ADDR_TYPE_PUBLIC;


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
    conn_params.min_int = 0x06;  // 7.5ms — BLE spec minimum, ideal for HID
    conn_params.max_int = 0x0C;  // 15ms
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

    // Resolve effective passkey: per-slot overrides global
    bool effective_has_passkey = false;
    uint32_t effective_passkey = 0;
    bool effective_sc = false;
    if (s_instance) {
        s_instance->get_active_slot_passkey(effective_has_passkey, effective_passkey, effective_sc);
    }

    if (use_static_passkey && effective_has_passkey) {
        bool use_sc = effective_sc;
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
        uint32_t passkey = effective_passkey;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
        s_use_static_passkey = true;
        s_require_mitm = true;
        ESP_LOGI(TAG, "Setting passkey: %06lu (slot %u)", (unsigned long) passkey,
                 s_instance ? s_instance->active_host_slot() : 0);
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
    // Set per-slot random address so each slot appears as a different BLE device.
    // This prevents hosts bonded to other slots from auto-reconnecting.
    if (s_instance) {
        uint8_t slot = s_instance->active_host_slot();
        const uint8_t *laddr = s_instance->get_slot_addr(slot);
        esp_ble_gap_set_rand_addr(const_cast<uint8_t *>(laddr));
        adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
        ESP_LOGD(TAG, "ADV: Using slot %u addr %02X:%02X:%02X:%02X:%02X:%02X", slot,
                 laddr[0], laddr[1], laddr[2], laddr[3], laddr[4], laddr[5]);
    }

    // If directed advertising is requested, target the specific bonded host
    if (s_directed_adv_pending) {
        s_directed_adv_pending = false;
        s_directed_adv_active = true;
        s_directed_adv_start_ms = millis();
        ESP_LOGI(TAG, "ADV: Directed advertising to %02X:%02X:%02X:%02X:%02X:%02X",
                 s_directed_addr[0], s_directed_addr[1], s_directed_addr[2],
                 s_directed_addr[3], s_directed_addr[4], s_directed_addr[5]);
        esp_ble_adv_params_t dir_params = adv_params;
        dir_params.adv_type = ADV_TYPE_DIRECT_IND_HIGH;
        memcpy(dir_params.peer_addr, s_directed_addr, sizeof(esp_bd_addr_t));
        dir_params.peer_addr_type = s_directed_addr_type;
        // Set adv data then start (directed low-duty still needs adv data on some stacks)
        s_adv_data_set = false;
        s_scan_rsp_data_set = false;
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        std::string dev_name = (s_instance != nullptr) ? s_instance->device_name() : "ESP32 BLE KB";
        std::vector<uint8_t> scan_rsp;
        scan_rsp.push_back(static_cast<uint8_t>(dev_name.length() + 1));
        scan_rsp.push_back(0x09);
        for (char c : dev_name) scan_rsp.push_back(static_cast<uint8_t>(c));
        esp_ble_gap_config_scan_rsp_data_raw(scan_rsp.data(), static_cast<uint16_t>(scan_rsp.size()));
        // Override adv_params for this cycle — the GAP completion handler will use dir_params
        adv_params.adv_type = ADV_TYPE_DIRECT_IND_HIGH;
        memcpy(adv_params.peer_addr, s_directed_addr, sizeof(esp_bd_addr_t));
        adv_params.peer_addr_type = s_directed_addr_type;
        return;
    }

    // Normal undirected advertising (pairing mode / default)
    adv_params.adv_type = ADV_TYPE_IND;
    memset(adv_params.peer_addr, 0, sizeof(esp_bd_addr_t));
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    s_adv_data_set = false;
    s_scan_rsp_data_set = false;
    esp_err_t adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    std::string dev_name = (s_instance != nullptr) ? s_instance->device_name() : "ESP32 BLE KB";
    std::vector<uint8_t> scan_rsp;
    scan_rsp.push_back(static_cast<uint8_t>(dev_name.length() + 1));
    scan_rsp.push_back(0x09);  // Complete Local Name AD type
    for (char c : dev_name) scan_rsp.push_back(static_cast<uint8_t>(c));
    esp_err_t scan_ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp.data(), static_cast<uint16_t>(scan_rsp.size()));

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
            if (s_instance && s_use_static_passkey) {
                bool pk_has; uint32_t pk_val; bool pk_sc;
                s_instance->get_active_slot_passkey(pk_has, pk_val, pk_sc);
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, pk_val);
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
                    // Matched by identity, so a phone that reconnected on a fresh
                    // resolvable address is still recognised as the slot's owner.
                    int8_t known = s_instance->find_slot_for_peer(param->ble_security.auth_cmpl.bd_addr);
                    if (known >= 0) {
                        ESP_LOGI(TAG, "Reconnected host already in slot %d", known);
                    } else {
                        uint8_t slot = s_instance->active_host_slot();
                        // A bonded slot belongs to its host until it is forgotten.
                        // An unoccupied slot, or one holding an address whose bond
                        // is gone (restored backup, cleared stale bond), is free to
                        // take — refusing those would strand the owner. So is one we
                        // can no longer identify, where the owner returning on a
                        // rotated address looks exactly like a stranger.
                        if (s_instance->get_host_slot(slot).occupied && s_instance->host_slot_bonded(slot) &&
                            s_instance->host_slot_identifiable(slot)) {
                            s_instance->queue_host_reject(param->ble_security.auth_cmpl.bd_addr, slot);
                        } else {
                            // New host — assign to the active slot
                            s_instance->assign_host_slot_(
                                slot,
                                param->ble_security.auth_cmpl.bd_addr,
                                (esp_ble_addr_type_t) param->ble_security.auth_cmpl.addr_type);
                            s_instance->save_host_slots_();
                        }
                    }
                    // Identity is only available once keys have been exchanged, so
                    // the address published at connect time may have been the
                    // rotating one — republish now that it can be resolved.
                    s_instance->queue_host_mac_update();
                }
            } else {
                uint8_t fail_reason = param->ble_security.auth_cmpl.fail_reason;
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%x)", fail_reason);
                bool fb_has, fb_sc; uint32_t fb_pk;
                if (s_instance) s_instance->get_active_slot_passkey(fb_has, fb_pk, fb_sc);
                if (s_instance &&
                    fb_has &&
                    s_use_static_passkey &&
                    !fb_sc &&
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
        case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
            if (s_instance) {
            s_instance->rssi_pending_ = false;
                if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                    s_instance->pending_rssi_value_ = param->read_rssi_cmpl.rssi;
                    s_instance->pending_rssi_update_ = true;
                }
            }
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
static uint8_t  mouse_val[4]          = {0};  // buttons, X, Y, wheel
static uint16_t mouse_ccc_val         = 0;
static uint8_t  mouse_ref_val[2]      = {0x04, 0x01};
static uint8_t  abs_mouse_val[5]      = {0};  // buttons, X_lo, X_hi, Y_lo, Y_hi
static uint16_t abs_mouse_ccc_val     = 0;
static uint8_t  abs_mouse_ref_val[2]  = {0x05, 0x01};

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
    IDX_CHAR_MOUSE,        IDX_CHAR_MOUSE_VAL,
    IDX_CHAR_MOUSE_CCC,
    IDX_CHAR_MOUSE_REF,
    IDX_CHAR_ABS_MOUSE,    IDX_CHAR_ABS_MOUSE_VAL,
    IDX_CHAR_ABS_MOUSE_CCC,
    IDX_CHAR_ABS_MOUSE_REF,
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
static uint16_t s_mouse_report_handle = 0;
static uint16_t s_mouse_ccc_handle = 0;
static uint16_t s_abs_mouse_report_handle = 0;
static uint16_t s_abs_mouse_ccc_handle = 0;

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
    // Mouse report (Report ID 4)
    [IDX_CHAR_MOUSE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_MOUSE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(mouse_val), sizeof(mouse_val), mouse_val}},
    [IDX_CHAR_MOUSE_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(mouse_ccc_val), sizeof(mouse_ccc_val), (uint8_t *)&mouse_ccc_val}},
    [IDX_CHAR_MOUSE_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(mouse_ref_val), sizeof(mouse_ref_val), mouse_ref_val}},
    // Absolute mouse report (Report ID 5)
    [IDX_CHAR_ABS_MOUSE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, PERM_R, 1, 1, (uint8_t *)&PROP_READ_NOTIFY}},
    [IDX_CHAR_ABS_MOUSE_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_HID_REPORT, PERM_R_ENC, sizeof(abs_mouse_val), sizeof(abs_mouse_val), abs_mouse_val}},
    [IDX_CHAR_ABS_MOUSE_CCC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_CLIENT_CONFIG, PERM_RW_ENC, sizeof(abs_mouse_ccc_val), sizeof(abs_mouse_ccc_val), (uint8_t *)&abs_mouse_ccc_val}},
    [IDX_CHAR_ABS_MOUSE_REF] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_RPT_REF_DESCR, PERM_R_ENC, sizeof(abs_mouse_ref_val), sizeof(abs_mouse_ref_val), abs_mouse_ref_val}},
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
                s_mouse_report_handle = hid_handle_table[IDX_CHAR_MOUSE_VAL];
                s_mouse_ccc_handle = hid_handle_table[IDX_CHAR_MOUSE_CCC];
                s_abs_mouse_report_handle = hid_handle_table[IDX_CHAR_ABS_MOUSE_VAL];
                s_abs_mouse_ccc_handle = hid_handle_table[IDX_CHAR_ABS_MOUSE_CCC];
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
            if (s_instance) {
                s_instance->set_connected(true, param->connect.conn_id);
                memcpy(s_instance->peer_addr_, param->connect.remote_bda, sizeof(esp_bd_addr_t));
                s_instance->queue_host_mac_update();
            }
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            mouse_ccc_val = 0;
            abs_mouse_ccc_val = 0;
            battery_ccc_val = 0;
            request_host_friendly_conn_params(param->connect.remote_bda);
            // Trigger encryption with security level matching configured pairing mode
            esp_ble_sec_act_t sec_act = s_require_mitm ? ESP_BLE_SEC_ENCRYPT_MITM : ESP_BLE_SEC_ENCRYPT_NO_MITM;
            esp_ble_set_encryption(param->connect.remote_bda, sec_act);
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS: Disconnected");
            uint8_t dc_reason = param->disconnect.reason;
            ESP_LOGD(TAG, "GATTS: Disconnect reason 0x%02X", dc_reason);

            // Detect stale bond scenario: peer's bond keys are missing/mismatched after a power-cycle.
            // HCI reasons 0x05 (Auth Failure), 0x06 (PIN/Key Missing), 0x3D (MIC Failure) indicate
            // that encryption could not be established with our stored LTK. If this peer is still
            // in our hosts table, drop the stale bond and switch to Just Works so the next
            // connection attempt can rebond automatically without manual re-pairing.
            if (s_instance && (dc_reason == 0x05 || dc_reason == 0x06 || dc_reason == 0x3D)) {
                bool is_zero = true;
                for (int i = 0; i < 6; i++) {
                    if (s_instance->peer_addr_[i] != 0) { is_zero = false; break; }
                }
                if (!is_zero) {
                    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
                        auto &hs = s_instance->get_host_slot(i);
                        if (hs.occupied && memcmp(hs.addr, s_instance->peer_addr_, sizeof(esp_bd_addr_t)) == 0) {
                            ESP_LOGW(TAG, "GATTS: Stale bond detected (reason 0x%02X) for known host slot %u — removing bond and falling back to Just Works", dc_reason, i);
                            esp_ble_remove_bond_device(s_instance->peer_addr_);
                            apply_security_params(false);
                            break;
                        }
                    }
                }
            }

            if (s_instance) {
                s_instance->set_connected(false, 0);
                // Host-side unpair often appears only as disconnect.
                s_instance->queue_paired_state(false);
                s_instance->rssi_pending_ = false;
                s_instance->pending_rssi_nan_ = true;
                s_instance->queue_host_mac_update();
            }
            proto_mode_val = 0x01;
            report_ccc_val = 0;
            boot_kb_in_ccc_val = 0;
            consumer_ccc_val = 0;
            system_ccc_val = 0;
            mouse_ccc_val = 0;
            abs_mouse_ccc_val = 0;
            battery_ccc_val = 0;
            do_start_advertising();
            break;
        }
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
            if (param->write.handle == s_mouse_ccc_handle && param->write.len >= 2) {
                mouse_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Mouse CCC=0x%04X", mouse_ccc_val);
            }
            if (param->write.handle == s_abs_mouse_ccc_handle && param->write.len >= 2) {
                abs_mouse_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                    (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Abs mouse CCC=0x%04X (absolute pointer)", abs_mouse_ccc_val);
            }
            if ((param->write.handle == s_hid_output_report_handle || param->write.handle == s_boot_kb_output_handle) &&
                param->write.len > 0) {
                ESP_LOGD(TAG, "GATTS: Keyboard LED report 0x%02X", param->write.value[0]);
                if (s_instance) s_instance->queue_led_state(param->write.value[0]);
            }
            break;
        default:
            break;
    }
}

// ── Multi-Host Slot Management ──────────────────────────────────────────────

void EspidfBleKeyboard::generate_slot_addrs_() {
    // Generate random static BLE addresses for each slot and persist to NVS.
    // Random static addresses have the two MSBs set to 11.
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "slot%u_laddr", i);
        size_t len = sizeof(esp_bd_addr_t);
        if (nvs_get_blob(handle, key, slot_addrs_[i], &len) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded slot %u local addr: %02X:%02X:%02X:%02X:%02X:%02X", i,
                     slot_addrs_[i][0], slot_addrs_[i][1], slot_addrs_[i][2],
                     slot_addrs_[i][3], slot_addrs_[i][4], slot_addrs_[i][5]);
        } else {
            // Generate new random static address
            esp_fill_random(slot_addrs_[i], 6);
            slot_addrs_[i][0] |= 0xC0;  // MSBs = 11 → random static
            nvs_set_blob(handle, key, slot_addrs_[i], sizeof(esp_bd_addr_t));
            ESP_LOGI(TAG, "Generated slot %u local addr: %02X:%02X:%02X:%02X:%02X:%02X", i,
                     slot_addrs_[i][0], slot_addrs_[i][1], slot_addrs_[i][2],
                     slot_addrs_[i][3], slot_addrs_[i][4], slot_addrs_[i][5]);
        }
    }
    nvs_commit(handle);
    nvs_close(handle);
}

void EspidfBleKeyboard::load_host_slots_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    uint8_t slot_count = 0;
    if (nvs_get_u8(handle, "host_cnt", &slot_count) == ESP_OK) {
        for (uint8_t i = 0; i < slot_count && i < MAX_HOST_SLOTS; i++) {
            char key[16];
            snprintf(key, sizeof(key), "host%u_addr", i);
            size_t len = sizeof(esp_bd_addr_t);
            if (nvs_get_blob(handle, key, hosts_[i].addr, &len) == ESP_OK) {
                hosts_[i].occupied = true;
                snprintf(key, sizeof(key), "host%u_type", i);
                uint8_t addr_type = 0;
                if (nvs_get_u8(handle, key, &addr_type) == ESP_OK) {
                    hosts_[i].addr_type = (esp_ble_addr_type_t) addr_type;
                }
                snprintf(key, sizeof(key), "host%u_id", i);
                size_t id_len = sizeof(esp_bd_addr_t);
                hosts_[i].has_identity =
                    nvs_get_blob(handle, key, hosts_[i].identity, &id_len) == ESP_OK;
                ESP_LOGI(TAG, "Loaded host slot %u: %02X:%02X:%02X:%02X:%02X:%02X", i,
                         hosts_[i].addr[0], hosts_[i].addr[1], hosts_[i].addr[2],
                         hosts_[i].addr[3], hosts_[i].addr[4], hosts_[i].addr[5]);
            }
        }
    }

    uint8_t active = 0;
    if (nvs_get_u8(handle, "host_act", &active) == ESP_OK && active < MAX_HOST_SLOTS) {
        active_slot_ = active;
    }

    nvs_close(handle);
}

bool EspidfBleKeyboard::host_slot_bonded(uint8_t slot) const {
    if (slot >= MAX_HOST_SLOTS || !hosts_[slot].occupied) return false;
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;
    std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
    int query_num = dev_num;
    if (esp_ble_get_bond_device_list(&query_num, bonded.data()) != ESP_OK) return false;
    for (int i = 0; i < query_num; i++) {
        if (memcmp(bonded[static_cast<size_t>(i)].bd_addr, hosts_[slot].addr,
                   sizeof(esp_bd_addr_t)) == 0)
            return true;
    }
    return false;
}

void format_bd_addr(const esp_bd_addr_t addr, char out[18]) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

bool EspidfBleKeyboard::peer_identity_addr(const esp_bd_addr_t addr, esp_bd_addr_t &out) const {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;
    std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
    int query_num = dev_num;
    if (esp_ble_get_bond_device_list(&query_num, bonded.data()) != ESP_OK) return false;

    for (int i = 0; i < query_num; i++) {
        const auto &dev = bonded[static_cast<size_t>(i)];
        // A peer sends its ID key only if it distributed one. Without it there is
        // no stable address to report — say so rather than hand back six zeroes.
        if ((dev.bond_key.key_mask & ESP_BLE_ID_KEY_MASK) == 0) continue;
        bool identity_set = false;
        for (int b = 0; b < 6; b++) {
            if (dev.bond_key.pid_key.static_addr[b] != 0) { identity_set = true; break; }
        }
        if (!identity_set) continue;
        // Match either way round: callers hold the connection address at connect
        // time, but a stored slot may already have been matched to the identity.
        if (memcmp(dev.bd_addr, addr, sizeof(esp_bd_addr_t)) == 0 ||
            memcmp(dev.bond_key.pid_key.static_addr, addr, sizeof(esp_bd_addr_t)) == 0) {
            memcpy(out, dev.bond_key.pid_key.static_addr, sizeof(esp_bd_addr_t));
            return true;
        }
    }
    return false;
}

int8_t EspidfBleKeyboard::find_slot_for_peer(const esp_bd_addr_t addr) const {
    esp_bd_addr_t peer_id;
    bool have_peer_id = peer_identity_addr(addr, peer_id);

    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        if (!hosts_[i].occupied) continue;
        // Raw match first — it is free, and covers every host that uses a public
        // address as well as a slot whose stored address has not rotated yet.
        if (memcmp(hosts_[i].addr, addr, sizeof(esp_bd_addr_t)) == 0) return (int8_t) i;
        if (!have_peer_id) continue;
        // The identity we remembered for this slot is the reliable comparison:
        // it is the one value that does not go stale when the host rotates.
        if (hosts_[i].has_identity &&
            memcmp(hosts_[i].identity, peer_id, sizeof(esp_bd_addr_t)) == 0)
            return (int8_t) i;
        // The slot may also hold the identity itself, if that is what the stack
        // reported when it was assigned.
        if (memcmp(hosts_[i].addr, peer_id, sizeof(esp_bd_addr_t)) == 0) return (int8_t) i;
        // Otherwise compare identities, so a phone that came back on a fresh
        // resolvable address is still recognised as the host that owns the slot.
        esp_bd_addr_t slot_id;
        if (peer_identity_addr(hosts_[i].addr, slot_id) &&
            memcmp(slot_id, peer_id, sizeof(esp_bd_addr_t)) == 0)
            return (int8_t) i;
    }
    return -1;
}

bool EspidfBleKeyboard::host_slot_identifiable(uint8_t slot) const {
    if (slot >= MAX_HOST_SLOTS || !hosts_[slot].occupied) return false;
    // A remembered identity is the strongest case: it cannot go stale.
    if (hosts_[slot].has_identity) return true;
    // A fixed address is its own identity — anything that does not match it is a
    // different device, full stop. Same RPA test as the advertising path uses.
    if ((hosts_[slot].addr[0] >> 6) != 0x01) return true;
    // A rotating one is only meaningful while it still resolves back to an identity.
    esp_bd_addr_t id;
    return peer_identity_addr(hosts_[slot].addr, id);
}

void EspidfBleKeyboard::save_host_slots_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        char key[12];
        if (hosts_[i].occupied) {
            snprintf(key, sizeof(key), "host%u_addr", i);
            nvs_set_blob(handle, key, hosts_[i].addr, sizeof(esp_bd_addr_t));
            snprintf(key, sizeof(key), "host%u_type", i);
            nvs_set_u8(handle, key, (uint8_t) hosts_[i].addr_type);
            snprintf(key, sizeof(key), "host%u_id", i);
            if (hosts_[i].has_identity) {
                nvs_set_blob(handle, key, hosts_[i].identity, sizeof(esp_bd_addr_t));
            } else {
                nvs_erase_key(handle, key);
            }
            count = i + 1;
        } else {
            // Erase stale entries
            snprintf(key, sizeof(key), "host%u_addr", i);
            nvs_erase_key(handle, key);
            snprintf(key, sizeof(key), "host%u_type", i);
            nvs_erase_key(handle, key);
            snprintf(key, sizeof(key), "host%u_id", i);
            nvs_erase_key(handle, key);
        }
    }
    nvs_set_u8(handle, "host_cnt", count);
    nvs_set_u8(handle, "host_act", active_slot_);
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Per-host action overrides (YAML defaults + NVS, web-editable) ─
//
// Stored one NVS entry per slot (key "ovr<slot>"), value the slot's overrides
// serialised as "name=action\n" records. Names reject '=', '|', whitespace and
// newlines so a payload can never break the encoding it is stored in.

bool EspidfBleKeyboard::valid_override_name(const std::string &name) {
    if (name.empty() || name.size() > 31) return false;
    for (char c : name) {
        if (c == '=' || c == '|' || c == '\n' || c == '\r' || c == ' ' || c == '\t')
            return false;
    }
    return true;
}

void EspidfBleKeyboard::set_host_slot_override(uint8_t slot, const std::string &name,
                                               const std::string &action) {
    if (slot >= MAX_HOST_SLOTS || !valid_override_name(name) || action.empty()) return;
    for (auto &o : yaml_overrides_[slot]) {
        if (o.name == name) { o.action = action; return; }
    }
    if (yaml_overrides_[slot].size() < MAX_OVERRIDES)
        yaml_overrides_[slot].push_back({name, action});
}

const std::string *EspidfBleKeyboard::find_override_(uint8_t slot, const std::string &name) const {
    if (slot >= MAX_HOST_SLOTS) return nullptr;
    for (const auto &o : nvs_overrides_[slot])
        if (o.name == name) return &o.action;
    for (const auto &o : yaml_overrides_[slot])
        if (o.name == name) return &o.action;
    return nullptr;
}

bool EspidfBleKeyboard::set_override(uint8_t slot, const std::string &name,
                                     const std::string &action) {
    if (slot >= MAX_HOST_SLOTS || !valid_override_name(name)) return false;
    if (action.empty() || action.size() > 255) return false;
    if (action.find('\n') != std::string::npos || action.find('\r') != std::string::npos)
        return false;

    for (auto &o : nvs_overrides_[slot]) {
        if (o.name == name) {
            o.action = action;
            save_overrides_(slot);
            ESP_LOGI(TAG, "Override slot %u: %s -> %s", (unsigned) slot, name.c_str(), action.c_str());
            return true;
        }
    }
    if (nvs_overrides_[slot].size() >= MAX_OVERRIDES) return false;
    nvs_overrides_[slot].push_back({name, action});
    save_overrides_(slot);
    ESP_LOGI(TAG, "Override slot %u: %s -> %s", (unsigned) slot, name.c_str(), action.c_str());
    return true;
}

bool EspidfBleKeyboard::clear_override(uint8_t slot, const std::string &name) {
    if (slot >= MAX_HOST_SLOTS) return false;
    for (size_t i = 0; i < nvs_overrides_[slot].size(); i++) {
        if (nvs_overrides_[slot][i].name == name) {
            nvs_overrides_[slot].erase(nvs_overrides_[slot].begin() + i);
            save_overrides_(slot);
            ESP_LOGI(TAG, "Cleared override slot %u: %s", (unsigned) slot, name.c_str());
            return true;
        }
    }
    return false;
}

void EspidfBleKeyboard::load_overrides_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "ovr%u", slot);

        size_t len = 0;
        if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0) continue;
        // Cap: MAX_OVERRIDES records of "name=action\n" (31 + 1 + 255 + 1)
        if (len > (size_t) (MAX_OVERRIDES * 288 + 1)) {
            ESP_LOGW(TAG, "Override blob for slot %u is oversized (%u bytes) — ignoring",
                     (unsigned) slot, (unsigned) len);
            continue;
        }

        std::vector<char> buf(len);
        if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;

        std::string blob(buf.data());
        size_t start = 0;
        while (start < blob.size() && nvs_overrides_[slot].size() < MAX_OVERRIDES) {
            size_t end = blob.find('\n', start);
            if (end == std::string::npos) end = blob.size();
            std::string line = blob.substr(start, end - start);
            start = end + 1;

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string name = line.substr(0, eq);
            std::string action = line.substr(eq + 1);
            if (!valid_override_name(name) || action.empty()) continue;

            nvs_overrides_[slot].push_back({name, action});
            ESP_LOGI(TAG, "Loaded override slot %u: %s -> %s", (unsigned) slot, name.c_str(),
                     action.c_str());
        }
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_overrides_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "ovr%u", slot);

    if (nvs_overrides_[slot].empty()) {
        nvs_erase_key(handle, key);
    } else {
        std::string blob;
        for (const auto &o : nvs_overrides_[slot]) {
            blob += o.name;
            blob += '=';
            blob += o.action;
            blob += '\n';
        }
        nvs_set_str(handle, key, blob.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Per-host hidden remote buttons (NVS-persisted) ────────────────
//
// One NVS entry per slot (key "hid<slot>"), value a comma-separated list of
// action names. Presentation only: nothing here touches execute_action(), so a
// hidden button's action still runs from macros, YAML and the API.

std::string EspidfBleKeyboard::hidden_csv(uint8_t slot) const {
    std::string out;
    if (slot >= MAX_HOST_SLOTS) return out;
    for (const auto &n : hidden_[slot]) {
        if (!out.empty()) out += ",";
        out += n;
    }
    return out;
}

bool EspidfBleKeyboard::set_hidden(uint8_t slot, const std::vector<std::string> &names) {
    if (slot >= MAX_HOST_SLOTS || names.size() > MAX_HIDDEN) return false;
    for (const auto &n : names) {
        // valid_override_name() already rejects '=', '|' and whitespace; the
        // comma is this list's own separator.
        if (!valid_override_name(n) || n.find(',') != std::string::npos) return false;
    }
    hidden_[slot] = names;
    save_hidden_(slot);
    if (slot == active_slot_) publish_hidden_();
    ESP_LOGI(TAG, "Hidden buttons for host %u: %u", (unsigned) slot, (unsigned) names.size());
    return true;
}

void EspidfBleKeyboard::publish_hidden_() {
    if (hidden_sensor_ == nullptr) return;
    std::string csv = hidden_csv(active_slot_);
    // Home Assistant rejects state strings over 255 chars. Truncate on a name
    // boundary — a half name would be silently meaningless to the card.
    if (csv.size() > 255) {
        size_t cut = csv.rfind(',', 255);
        csv = (cut == std::string::npos) ? "" : csv.substr(0, cut);
        unsigned kept = 0;
        for (char c : csv) if (c == ',') kept++;
        if (!csv.empty()) kept++;
        ESP_LOGW(TAG, "Hidden list for host %u exceeds the 255-char sensor limit — sent %u name(s)",
                 (unsigned) active_slot_, kept);
    }
    hidden_sensor_->publish_state(csv);
}

void EspidfBleKeyboard::publish_host_mac_() {
    if (host_mac_sensor_ == nullptr) return;
    if (!is_connected_) {
        host_mac_sensor_->publish_state("");
        return;
    }

    char addr_str[18];
    esp_bd_addr_t identity;
    if (peer_identity_addr(peer_addr_, identity)) {
        format_bd_addr(identity, addr_str);
        if (memcmp(identity, peer_addr_, sizeof(esp_bd_addr_t)) != 0) {
            // Worth a log line: this is the Android case, and it is the only place
            // the rotating address and the stable one can be seen side by side.
            char conn_str[18];
            format_bd_addr(peer_addr_, conn_str);
            ESP_LOGD(TAG, "Host identity %s (connected as %s)", addr_str, conn_str);
        }
    } else {
        // Not bonded yet, or the host distributed no ID key. The connection
        // address is all there is — fine for a host with a fixed address, and it
        // will be replaced once pairing completes.
        format_bd_addr(peer_addr_, addr_str);
    }
    host_mac_sensor_->publish_state(addr_str);
}

void EspidfBleKeyboard::remember_host_identity_() {
    if (!is_connected_) return;
    // Only resolvable while the host is connected: the bond table is keyed on the
    // address in use, so once a phone rotates away from the one the slot stored,
    // nothing can map that slot back to an identity. Catching it here is what
    // makes the address survive the rotation.
    esp_bd_addr_t identity;
    if (!peer_identity_addr(peer_addr_, identity)) return;

    int8_t slot = find_slot_for_peer(peer_addr_);
    // A connected peer belongs to the slot that was advertising for it, which is
    // the fallback when its stored address has already gone stale.
    if (slot < 0) slot = (int8_t) active_slot_;
    if (slot < 0 || slot >= (int8_t) MAX_HOST_SLOTS || !hosts_[slot].occupied) return;

    if (hosts_[slot].has_identity &&
        memcmp(hosts_[slot].identity, identity, sizeof(esp_bd_addr_t)) == 0)
        return;  // unchanged — don't churn NVS on every connect

    memcpy(hosts_[slot].identity, identity, sizeof(esp_bd_addr_t));
    hosts_[slot].has_identity = true;
    save_host_slots_();

    char id_str[18];
    format_bd_addr(identity, id_str);
    ESP_LOGI(TAG, "Host slot %d identity recorded: %s", (int) slot, id_str);
}

void EspidfBleKeyboard::reject_host_() {
    char addr_str[18];
    format_bd_addr(reject_addr_, addr_str);
    ESP_LOGW(TAG, "Refused %s: host slot %u is already bonded. Forget the host first to pair a "
                  "different device there.", addr_str, reject_slot_);
    // Removes this peer's bond only — the slot owner's bond is a separate entry.
    esp_ble_remove_bond_device(reject_addr_);
    if (is_connected_) esp_ble_gatts_close(s_gatts_if, conn_id_);
    // The close is asynchronous, so is_connected_ is still true this pass. Drop the
    // publish that pairing queued rather than announce a host we just refused; the
    // disconnect will queue its own and the sensor clears then.
    pending_host_mac_update_.store(false);
}

void EspidfBleKeyboard::load_hidden_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "hid%u", slot);

        size_t len = 0;
        if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0) continue;
        if (len > MAX_HIDDEN * 33 + 1) {
            ESP_LOGW(TAG, "Hidden list for slot %u is oversized (%u bytes) — ignoring",
                     (unsigned) slot, (unsigned) len);
            continue;
        }

        std::vector<char> buf(len);
        if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;

        std::string blob(buf.data());
        size_t start = 0;
        while (start < blob.size() && hidden_[slot].size() < MAX_HIDDEN) {
            size_t end = blob.find(',', start);
            if (end == std::string::npos) end = blob.size();
            std::string name = blob.substr(start, end - start);
            start = end + 1;
            if (valid_override_name(name)) hidden_[slot].push_back(name);
        }
        if (!hidden_[slot].empty())
            ESP_LOGI(TAG, "Loaded %u hidden button(s) for host %u",
                     (unsigned) hidden_[slot].size(), (unsigned) slot);
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_hidden_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "hid%u", slot);
    if (hidden_[slot].empty()) {
        nvs_erase_key(handle, key);
    } else {
        std::string blob = hidden_csv(slot);
        nvs_set_str(handle, key, blob.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Per-host press-and-hold (NVS-persisted) ───────────────────────
//
// Same shape as the hidden list above (one comma-separated NVS entry per slot,
// key "hld<slot>"), but this one changes what a press *does*: a button named
// here is held down on the host for as long as it is held on the remote,
// instead of being tapped. Stored per host because push-to-talk belongs to the
// machine running the voice app, not to the phone holding the remote.

std::string EspidfBleKeyboard::hold_repeat_conflict(uint8_t slot,
                                                    const std::vector<std::string> &names,
                                                    bool checking_hold) const {
    if (slot >= MAX_HOST_SLOTS) return "";
    // A button can only own one meaning of "held": repeating a tap, or staying
    // down. Whichever list is *not* being written is the one to check against.
    const std::vector<std::string> &other = checking_hold ? repeat_[slot].names : hold_[slot];
    if (checking_hold && !repeat_[slot].set) return "";
    for (const auto &n : names)
        if (std::find(other.begin(), other.end(), n) != other.end()) return n;
    return "";
}

bool EspidfBleKeyboard::set_hold(uint8_t slot, const std::vector<std::string> &names) {
    if (slot >= MAX_HOST_SLOTS || names.size() > MAX_HOLD) return false;
    for (const auto &n : names) {
        if (!valid_override_name(n) || n.find(',') != std::string::npos) return false;
    }
    if (!hold_repeat_conflict(slot, names, true).empty()) return false;
    hold_[slot] = names;
    save_hold_(slot);
    ESP_LOGI(TAG, "Hold-to-send for host %u: %u button(s)",
             (unsigned) slot, (unsigned) names.size());
    return true;
}

void EspidfBleKeyboard::load_hold_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "hld%u", slot);

        size_t len = 0;
        if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0) continue;
        if (len > MAX_HOLD * 33 + 1) {
            ESP_LOGW(TAG, "Hold list for slot %u is oversized (%u bytes) — ignoring",
                     (unsigned) slot, (unsigned) len);
            continue;
        }

        std::vector<char> buf(len);
        if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;

        std::string blob(buf.data());
        size_t start = 0;
        while (start < blob.size() && hold_[slot].size() < MAX_HOLD) {
            size_t end = blob.find(',', start);
            if (end == std::string::npos) end = blob.size();
            std::string name = blob.substr(start, end - start);
            start = end + 1;
            if (valid_override_name(name)) hold_[slot].push_back(name);
        }
        if (!hold_[slot].empty())
            ESP_LOGI(TAG, "Loaded %u press-and-hold button(s) for host %u",
                     (unsigned) hold_[slot].size(), (unsigned) slot);
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_hold_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "hld%u", slot);
    if (hold_[slot].empty()) {
        nvs_erase_key(handle, key);
    } else {
        std::string blob;
        for (size_t i = 0; i < hold_[slot].size(); i++) {
            if (i > 0) blob += ",";
            blob += hold_[slot][i];
        }
        nvs_set_str(handle, key, blob.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Per-host hold-to-repeat (NVS-persisted) ───────────────────────
//
// One NVS entry per slot (key "rpt<slot>"), value "<delay>,<rate>" followed by
// the action names that repeat. The first two comma fields are always numeric,
// so the same comma walk that reads the hidden list parses this unambiguously.
//
// Presentation only, like the hidden list: the browser holds the timer and
// re-sends an ordinary press, so nothing here runs an action. The device stores
// it so the setting follows the host rather than the phone that set it.
//
// Storing an empty name list is meaningful — it means "nothing repeats on this
// host" — so unlike save_hidden_() an empty list still writes the key. Erasing
// it (clear_repeat) is what returns the slot to the page's own defaults.

bool EspidfBleKeyboard::set_repeat(uint8_t slot, uint16_t delay, uint16_t rate,
                                   const std::vector<std::string> &names) {
    if (slot >= MAX_HOST_SLOTS || names.size() > MAX_REPEAT_BUTTONS) return false;
    for (const auto &n : names) {
        // valid_override_name() already rejects '=', '|' and whitespace; the
        // comma is this list's own separator.
        if (!valid_override_name(n) || n.find(',') != std::string::npos) return false;
    }
    // The other half of the guard in set_hold(): a button that stays down while
    // held can't also re-fire while held.
    if (!hold_repeat_conflict(slot, names, false).empty()) return false;
    // Clamped rather than rejected: the bounds exist to keep the repeat usable
    // (see REPEAT_RATE_MIN and the dedup guards), not to police the caller.
    if (delay < REPEAT_DELAY_MIN) delay = REPEAT_DELAY_MIN;
    if (delay > REPEAT_DELAY_MAX) delay = REPEAT_DELAY_MAX;
    if (rate < REPEAT_RATE_MIN) rate = REPEAT_RATE_MIN;
    if (rate > REPEAT_RATE_MAX) rate = REPEAT_RATE_MAX;

    repeat_[slot].set = true;
    repeat_[slot].delay = delay;
    repeat_[slot].rate = rate;
    repeat_[slot].names = names;
    save_repeat_(slot);
    ESP_LOGI(TAG, "Repeat for host %u: %u button(s), %ums then every %ums",
             (unsigned) slot, (unsigned) names.size(), (unsigned) delay, (unsigned) rate);
    return true;
}

void EspidfBleKeyboard::clear_repeat(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    repeat_[slot] = RepeatCfg{};
    save_repeat_(slot);  // .set is false now, so this erases the key
    ESP_LOGI(TAG, "Repeat for host %u reset to defaults", (unsigned) slot);
}

void EspidfBleKeyboard::load_repeat_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "rpt%u", slot);

        size_t len = 0;
        if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0) continue;
        if (len > MAX_REPEAT_BUTTONS * 33 + 16) {
            ESP_LOGW(TAG, "Repeat list for slot %u is oversized (%u bytes) — ignoring",
                     (unsigned) slot, (unsigned) len);
            continue;
        }

        std::vector<char> buf(len);
        if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;

        std::string blob(buf.data());
        RepeatCfg cfg;
        size_t start = 0;
        for (int field = 0; start <= blob.size(); field++) {
            size_t end = blob.find(',', start);
            if (end == std::string::npos) end = blob.size();
            std::string tok = blob.substr(start, end - start);
            if (field == 0) {
                cfg.delay = (uint16_t) atoi(tok.c_str());
            } else if (field == 1) {
                cfg.rate = (uint16_t) atoi(tok.c_str());
            } else if (valid_override_name(tok) && cfg.names.size() < MAX_REPEAT_BUTTONS) {
                cfg.names.push_back(tok);
            }
            if (end == blob.size()) break;
            start = end + 1;
        }
        // A blob written by an older or corrupt build could carry out-of-range
        // timings; clamp on the way in so the page never gets a useless rate.
        if (cfg.delay < REPEAT_DELAY_MIN) cfg.delay = REPEAT_DELAY_MIN;
        if (cfg.delay > REPEAT_DELAY_MAX) cfg.delay = REPEAT_DELAY_MAX;
        if (cfg.rate < REPEAT_RATE_MIN) cfg.rate = REPEAT_RATE_MIN;
        if (cfg.rate > REPEAT_RATE_MAX) cfg.rate = REPEAT_RATE_MAX;
        cfg.set = true;
        repeat_[slot] = cfg;
        ESP_LOGI(TAG, "Loaded repeat for host %u: %u button(s), %ums/%ums",
                 (unsigned) slot, (unsigned) cfg.names.size(),
                 (unsigned) cfg.delay, (unsigned) cfg.rate);
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_repeat_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "rpt%u", slot);
    if (!repeat_[slot].set) {
        nvs_erase_key(handle, key);
    } else {
        std::string blob = std::to_string(repeat_[slot].delay) + "," +
                           std::to_string(repeat_[slot].rate);
        for (const auto &n : repeat_[slot].names) blob += "," + n;
        nvs_set_str(handle, key, blob.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// ── User-editable macros (NVS-persisted) ──────────────────────────

void EspidfBleKeyboard::load_macros_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    uint8_t count = 0;
    if (nvs_get_u8(handle, "macro_cnt", &count) == ESP_OK) {
        for (uint8_t i = 0; i < count && i < MAX_MACROS; i++) {
            char key[16];
            char buf[256];
            size_t len;
            std::string name, action;

            snprintf(key, sizeof(key), "macro%u_name", i);
            len = sizeof(buf);
            if (nvs_get_str(handle, key, buf, &len) == ESP_OK)
                name = buf;

            snprintf(key, sizeof(key), "macro%u_act", i);
            len = sizeof(buf);
            if (nvs_get_str(handle, key, buf, &len) == ESP_OK)
                action = buf;

            if (!name.empty() && !action.empty()) {
                macros_.push_back({name, action});
                ESP_LOGI(TAG, "Loaded macro %u: %s -> %s", i, name.c_str(), action.c_str());
            }
        }
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_macros_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    uint8_t count = macros_.size();
    nvs_set_u8(handle, "macro_cnt", count);

    for (uint8_t i = 0; i < MAX_MACROS; i++) {
        char key[16];
        if (i < count) {
            snprintf(key, sizeof(key), "macro%u_name", i);
            nvs_set_str(handle, key, macros_[i].name.c_str());
            snprintf(key, sizeof(key), "macro%u_act", i);
            nvs_set_str(handle, key, macros_[i].action.c_str());
        } else {
            // Erase stale entries
            snprintf(key, sizeof(key), "macro%u_name", i);
            nvs_erase_key(handle, key);
            snprintf(key, sizeof(key), "macro%u_act", i);
            nvs_erase_key(handle, key);
        }
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// Names are referenced by `macro:<name>`, so they have to be unambiguous and
// survive the action parser. A '|' would split the reference into steps (or
// into alternate: branches); a duplicate would make it ambiguous which macro
// is meant. ':' needs no restriction — everything after the first one is the
// name, so "macro:My:Thing" still resolves.
bool EspidfBleKeyboard::macro_name_available(const std::string &name, int skip_index) const {
    if (name.find('|') != std::string::npos) return false;
    for (size_t i = 0; i < macros_.size(); i++) {
        if ((int) i == skip_index) continue;
        if (macros_[i].name == name) return false;
    }
    return true;
}

bool EspidfBleKeyboard::add_macro(const std::string &name, const std::string &action) {
    if (macros_.size() >= MAX_MACROS) return false;
    if (!macro_name_available(name, -1)) return false;
    macros_.push_back({name, action});
    save_macros_();
    ESP_LOGI(TAG, "Added macro: %s -> %s", name.c_str(), action.c_str());
    return true;
}

bool EspidfBleKeyboard::update_macro(uint8_t index, const std::string &name, const std::string &action) {
    if (index >= macros_.size()) return false;
    // skip_index so renaming a macro to the name it already has still saves.
    if (!macro_name_available(name, (int) index)) return false;
    macros_[index].name = name;
    macros_[index].action = action;
    save_macros_();
    ESP_LOGI(TAG, "Updated macro %u: %s -> %s", index, name.c_str(), action.c_str());
    return true;
}

bool EspidfBleKeyboard::delete_macro(uint8_t index) {
    if (index >= macros_.size()) return false;
    ESP_LOGI(TAG, "Deleted macro %u: %s", index, macros_[index].name.c_str());
    macros_.erase(macros_.begin() + index);
    save_macros_();
    return true;
}

void EspidfBleKeyboard::assign_host_slot_(uint8_t slot, const esp_bd_addr_t addr, esp_ble_addr_type_t addr_type) {
    if (slot >= MAX_HOST_SLOTS) return;
    // Check if this address is already in another slot
    for (uint8_t i = 0; i < MAX_HOST_SLOTS; i++) {
        if (hosts_[i].occupied && memcmp(hosts_[i].addr, addr, sizeof(esp_bd_addr_t)) == 0) {
            if (i == slot) return;  // Already in the right slot
            // Move from old slot to new slot, identity included — it describes
            // the host, not the seat it sits in.
            hosts_[slot].has_identity = hosts_[i].has_identity;
            memcpy(hosts_[slot].identity, hosts_[i].identity, sizeof(esp_bd_addr_t));
            hosts_[i].occupied = false;
            hosts_[i].has_identity = false;
            memset(hosts_[i].identity, 0, sizeof(esp_bd_addr_t));
            memcpy(hosts_[slot].addr, addr, sizeof(esp_bd_addr_t));
            hosts_[slot].addr_type = addr_type;
            hosts_[slot].occupied = true;
            ESP_LOGI(TAG, "Host slot %u assigned: %02X:%02X:%02X:%02X:%02X:%02X", slot,
                     addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            return;
        }
    }
    // A different host taking this slot: drop the previous occupant's identity,
    // or the slot would report a machine that no longer owns it.
    hosts_[slot].has_identity = false;
    memset(hosts_[slot].identity, 0, sizeof(esp_bd_addr_t));
    memcpy(hosts_[slot].addr, addr, sizeof(esp_bd_addr_t));
    hosts_[slot].addr_type = addr_type;
    hosts_[slot].occupied = true;
    ESP_LOGI(TAG, "Host slot %u assigned: %02X:%02X:%02X:%02X:%02X:%02X", slot,
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void EspidfBleKeyboard::switch_host(uint8_t slot) {
    if (slot >= host_slots_) {
        ESP_LOGW(TAG, "Invalid host slot %u (max %u)", slot, host_slots_ - 1);
        return;
    }

    // Let go of anything held before the link to the old host drops, or it is
    // left holding the key until it notices the disconnect.
    release_held();

    active_slot_ = slot;
    save_host_slots_();
    if (active_host_sensor_ != nullptr)
        active_host_sensor_->publish_state(slot);
    publish_hidden_();  // the new host may hide a different set of buttons

    // Re-apply security params for the new slot's passkey config
    bool slot_has_pk; uint32_t slot_pk; bool slot_sc;
    get_active_slot_passkey(slot_has_pk, slot_pk, slot_sc);
    apply_security_params(slot_has_pk);

    // Apply per-slot keyboard layout (ephemeral; does not overwrite the web UI's NVS choice).
    if (!slot_layout_id_[slot].empty()) set_runtime_layout(slot_layout_id_[slot], false);

    // Apply this host's saved goto calibration (if any).
    load_goto_scale_for_host(slot);

    ESP_LOGI(TAG, "Switching to host slot %u", slot);

    if (hosts_[slot].occupied) {
        // Check if stored address is a resolvable private address (RPA).
        // RPA has bits [7:6] of first byte = 01. Android rotates these, so
        // directed advertising to a stale RPA will always fail.
        uint8_t addr_top = hosts_[slot].addr[0] >> 6;
        if (addr_top == 0x01) {
            ESP_LOGI(TAG, "Host slot %u has RPA address — using undirected advertising", slot);
            // Skip directed, go straight to undirected so the phone can reconnect
        } else {
            // Static/public address — directed advertising is reliable
            s_directed_adv_pending = true;
            memcpy(s_directed_addr, hosts_[slot].addr, sizeof(esp_bd_addr_t));
            s_directed_addr_type = hosts_[slot].addr_type;
        }
    }
    // else: empty slot — undirected advertising (pairing mode)

    if (is_connected_) {
        // Disconnect current host; DISCONNECT_EVT will trigger advertising
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    } else {
        // Not connected — stop current advertising and restart
        esp_ble_gap_stop_advertising();
        do_start_advertising();
    }
}

void EspidfBleKeyboard::forget_host(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS || !hosts_[slot].occupied) return;

    ESP_LOGI(TAG, "Forgetting host slot %u", slot);

    // Remove the BLE bond
    esp_ble_remove_bond_device(hosts_[slot].addr);

    // Clear the slot
    hosts_[slot].occupied = false;
    memset(hosts_[slot].addr, 0, sizeof(esp_bd_addr_t));
    hosts_[slot].has_identity = false;
    memset(hosts_[slot].identity, 0, sizeof(esp_bd_addr_t));
    hosts_[slot].name.clear();

    save_host_slots_();

    // If this was the active slot and we're connected, disconnect
    if (slot == active_slot_ && is_connected_) {
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    }
}

bool EspidfBleKeyboard::get_active_slot_passkey(bool &has_passkey, uint32_t &passkey, bool &secure_connections) const {
    const auto &cfg = host_slot_configs_[active_slot_];
    if (cfg.has_passkey) {
        has_passkey = true;
        passkey = cfg.passkey;
        secure_connections = cfg.secure_connections;
        return true;
    }
    // Fall back to global config
    has_passkey = has_passkey_;
    passkey = passkey_;
    secure_connections = passkey_secure_connections_;
    return has_passkey_;
}

void EspidfBleKeyboard::update_rssi(int8_t rssi) {
    if (rssi_sensor_ != nullptr) {
        rssi_sensor_->publish_state(static_cast<float>(rssi));
    }
    for (auto &cb : rssi_above_callbacks_) cb(rssi);
    for (auto &cb : rssi_below_callbacks_) cb(rssi);
}

// ── Component Setup ──────────────────────────────────────────────────────────
#if defined(USE_API) && defined(USE_API_CUSTOM_SERVICES)
void EspidfBleKeyboard::register_api_services_() {
    register_service(&EspidfBleKeyboard::on_api_run_action_, "run_action", {"action"});
    register_service(&EspidfBleKeyboard::on_api_run_macro_, "run_macro", {"index"});
    register_service(&EspidfBleKeyboard::on_api_send_string_, "send_string", {"keys"});
    register_service(&EspidfBleKeyboard::on_api_send_key_, "send_key", {"modifier", "keycode"});
    register_service(&EspidfBleKeyboard::on_api_send_consumer_, "send_consumer", {"code"});
    register_service(&EspidfBleKeyboard::on_api_mouse_move_, "mouse_move", {"x", "y"});
    register_service(&EspidfBleKeyboard::on_api_mouse_scroll_, "mouse_scroll", {"amount"});
    register_service(&EspidfBleKeyboard::on_api_mouse_click_, "mouse_click", {"btn"});
    register_service(&EspidfBleKeyboard::on_api_mouse_hold_, "mouse_hold", {"btn"});
    register_service(&EspidfBleKeyboard::on_api_mouse_release_, "mouse_release");
    register_service(&EspidfBleKeyboard::on_api_mouse_abs_, "mouse_abs", {"x", "y"});
    register_service(&EspidfBleKeyboard::on_api_switch_host_, "switch_host", {"slot"});
    register_service(&EspidfBleKeyboard::on_api_forget_host_, "forget_host", {"slot"});
    ESP_LOGI(TAG, "Registered Home Assistant API services (esphome.<node>_run_action, _mouse_move, ...)");
}
#endif

void EspidfBleKeyboard::setup() {
    s_instance = this;
    type_mutex_ = xSemaphoreCreateMutex();
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
    load_host_slots_();
    yaml_goto_scale_x_ = goto_scale_x_;  // snapshot YAML defaults (for Reset) before NVS override
    yaml_goto_scale_y_ = goto_scale_y_;
    load_goto_scale_for_host(active_slot_);  // per-host calibration override (if saved)
    load_macros_();
    load_overrides_();  // all slots, so the web UI can edit an inactive slot
    load_hidden_();
    load_repeat_();
    load_hold_();
    // Apply YAML default if no setter ran (defensive), then let NVS override.
    if (active_layout_ == nullptr) active_layout_ = default_layout();
    load_layout_();
    // Boot slot has a configured per-slot layout — apply ephemerally so it wins over the NVS default
    // until the user manually picks something in the web UI for this slot.
    if (!slot_layout_id_[active_slot_].empty())
        set_runtime_layout(slot_layout_id_[active_slot_], false);
    generate_slot_addrs_();
    if (active_host_sensor_ != nullptr)
        active_host_sensor_->publish_state(active_slot_);
    publish_hidden_();

    // If active slot has a bonded host, use directed advertising on startup
    // Skip directed for RPA addresses (Android rotates these)
    if (hosts_[active_slot_].occupied) {
        uint8_t addr_top = hosts_[active_slot_].addr[0] >> 6;
        if (addr_top == 0x01) {
            ESP_LOGI(TAG, "Startup: host slot %u has RPA address — using undirected advertising", active_slot_);
        } else {
            s_directed_adv_pending = true;
            memcpy(s_directed_addr, hosts_[active_slot_].addr, sizeof(esp_bd_addr_t));
            s_directed_addr_type = hosts_[active_slot_].addr_type;
            ESP_LOGI(TAG, "Startup: will direct-advertise to host slot %u", active_slot_);
        }
    }

    bool startup_has_pk; uint32_t startup_pk; bool startup_sc;
    get_active_slot_passkey(startup_has_pk, startup_pk, startup_sc);
    apply_security_params(startup_has_pk);

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);

    set_connected(false, 0);
    set_paired(false);

#if defined(USE_API) && defined(USE_API_CUSTOM_SERVICES)
    if (api_services_enabled_) register_api_services_();
#else
    if (api_services_enabled_)
        ESP_LOGW(TAG, "api_services: true but the 'api:' component (with custom_services support) is missing — no HA services registered");
#endif

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
    if (web_control_enabled_ && web_server_base_ != nullptr) {
      web_control_ = new BleKeyboardWebControl(web_server_base_, this);
      web_control_->setup();
    }
#endif
}

void EspidfBleKeyboard::update_led_state_(uint8_t led_byte) {
    if (num_lock_binary_sensor_ != nullptr)
        num_lock_binary_sensor_->publish_state(led_byte & 0x01);
    if (caps_lock_binary_sensor_ != nullptr)
        caps_lock_binary_sensor_->publish_state(led_byte & 0x02);
    if (scroll_lock_binary_sensor_ != nullptr)
        scroll_lock_binary_sensor_->publish_state(led_byte & 0x04);
}

void EspidfBleKeyboard::loop() {
    if (pending_paired_update_.exchange(false)) {
        set_paired(pending_paired_state_.load());
    }
    if (pending_led_update_.exchange(false)) {
        update_led_state_(pending_led_value_.load());
    }
    if (pending_rssi_nan_.exchange(false)) {
        if (rssi_sensor_ != nullptr) rssi_sensor_->publish_state(NAN);
    }
    if (pending_rssi_update_.exchange(false)) {
        update_rssi(pending_rssi_value_.load());
    }
    // Must run before the publish below — it withdraws the queued update so a
    // refused device is never announced as the connected host.
    if (pending_host_reject_.exchange(false)) {
        reject_host_();
    }
    if (pending_host_mac_update_.exchange(false)) {
        // Record before publishing, and outside publish_host_mac_ itself, so the
        // slot still learns its host's identity on a device with no MAC sensor.
        remember_host_identity_();
        publish_host_mac_();
    }

    if (is_connected_) {
        s_directed_adv_active = false;
    } else if (s_directed_adv_active) {
        if (millis() - s_directed_adv_start_ms > 2000) {
            s_directed_adv_active = false;
            ESP_LOGW(TAG, "ADV: Directed advertising timeout. Falling back to undirected...");
            esp_ble_gap_stop_advertising();
            do_start_advertising();
        }
    }

    // Stuck-key guard: a hold whose release never arrived (browser closed
    // mid-press, a lost on_release) would otherwise stay down indefinitely.
    // Off by default — a push-to-talk key has no natural maximum.
    if (max_key_hold_ms_ > 0 && has_held() && millis() - hold_start_ms_ > max_key_hold_ms_) {
        ESP_LOGW(TAG, "Held key exceeded max_key_hold_ms (%ums) — releasing",
                 (unsigned) max_key_hold_ms_);
        release_held();
    }

    // Non-blocking string typing: one keystroke step per loop() call, paced by timer.
    if (!is_connected_ || type_mutex_ == nullptr) return;

    uint32_t now = millis();

    // RSSI polling: read signal strength of connected host on configured interval.
    if (rssi_sensor_ != nullptr && !rssi_pending_) {
        if (now - rssi_last_poll_ms_ >= rssi_update_interval_ms_) {
            rssi_last_poll_ms_ = now;
            rssi_pending_ = true;
            esp_ble_gap_read_rssi(peer_addr_);
        }
    }
    if (now < type_next_ms_) return;

    // Snapshot queue state under mutex (non-blocking try-lock).
    if (xSemaphoreTake(type_mutex_, 0) != pdTRUE) return;
    bool queue_empty = type_queue_.empty();
    bool key_up = type_key_up_pending_;
    HidKeyMapping mapping{0, 0};
    if (!queue_empty && !key_up) mapping = type_queue_[type_index_];
    xSemaphoreGive(type_mutex_);

    if (queue_empty) return;

    uint32_t half_delay = key_delay_ms_ / 2;

    if (key_up) {
        // Send key-up. Anything being held stays down (send_kb_report_), so
        // typing while push-to-talk is held doesn't cut the held key.
        // Retry next loop() if the BLE stack queue is full.
        if (send_kb_report_(0, 0) != ESP_OK) return;
        xSemaphoreTake(type_mutex_, portMAX_DELAY);
        type_key_up_pending_ = false;
        type_index_++;
        if (type_index_ >= type_queue_.size()) {
            type_queue_.clear();
            type_index_ = 0;
        }
        type_next_ms_ = now + half_delay;
        xSemaphoreGive(type_mutex_);
    } else {
        // Send key-down for the current keystroke. Layout resolution happened
        // at enqueue time (see send_string), so unmapped chars never reach here.
        if (send_kb_report_(mapping.modifier, mapping.keycode) != ESP_OK) return;
        xSemaphoreTake(type_mutex_, portMAX_DELAY);
        type_key_up_pending_ = true;
        type_next_ms_ = now + half_delay;
        xSemaphoreGive(type_mutex_);
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

static esp_err_t send_keyboard_input_report(uint16_t conn_id, const uint8_t *report, uint16_t len) {
    const uint16_t primary_handle = get_keyboard_input_handle();
    if (primary_handle == 0) {
        ESP_LOGW(TAG, "No keyboard input handle available");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "BLE report -> handle=0x%04X mod=0x%02X key=0x%02X",
             primary_handle, report[0], report[2]);
    esp_err_t ret = esp_ble_gatts_send_indicate(s_gatts_if, conn_id, primary_handle, len, const_cast<uint8_t *>(report), false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Keyboard report send failed on handle 0x%04X (%d), caller will retry",
                 primary_handle, ret);
    }
    // Keep the readable characteristic value in sync for hosts that READ the
    // input report (e.g. on reconnect) instead of relying on notifications.
    esp_ble_gatts_set_attr_value(primary_handle, len, report);
    return ret;
}

// Decode one UTF-8 codepoint starting at `bytes[i]`. Returns codepoint and
// advances `i` past consumed bytes. Returns 0xFFFD on malformed input (and
// advances by 1 byte to recover).
static uint32_t decode_utf8_(const std::string &bytes, size_t &i) {
    uint8_t b0 = static_cast<uint8_t>(bytes[i]);
    if (b0 < 0x80) { i++; return b0; }
    auto cont = [&](size_t off) -> int {
        if (i + off >= bytes.size()) return -1;
        uint8_t b = static_cast<uint8_t>(bytes[i + off]);
        return ((b & 0xC0) == 0x80) ? (b & 0x3F) : -1;
    };
    if ((b0 & 0xE0) == 0xC0) {
        int c1 = cont(1);
        if (c1 < 0) { i++; return 0xFFFD; }
        uint32_t cp = ((b0 & 0x1F) << 6) | c1;
        i += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0) {
        int c1 = cont(1), c2 = cont(2);
        if (c1 < 0 || c2 < 0) { i++; return 0xFFFD; }
        uint32_t cp = ((b0 & 0x0F) << 12) | (c1 << 6) | c2;
        i += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0) {
        int c1 = cont(1), c2 = cont(2), c3 = cont(3);
        if (c1 < 0 || c2 < 0 || c3 < 0) { i++; return 0xFFFD; }
        uint32_t cp = ((b0 & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
        i += 4;
        return cp;
    }
    i++;
    return 0xFFFD;
}

// Resolve a Unicode codepoint to a (modifier, keycode) pair using the active
// layout. Returns {0,0} if unmapped.
static HidKeyMapping resolve_codepoint_(const KeyboardLayout *layout, uint32_t cp) {
    if (layout == nullptr) return {0, 0, 0, 0};
    if (cp < 128) {
        return layout->ascii_map[cp];
    }
    for (size_t i = 0; i < layout->unicode_map_len; i++) {
        if (layout->unicode_map[i].codepoint == cp) {
            return {layout->unicode_map[i].modifier, layout->unicode_map[i].keycode,
                    layout->unicode_map[i].followup_keycode,
                    layout->unicode_map[i].followup_modifier};
        }
    }
    return {0, 0, 0, 0};
}

void EspidfBleKeyboard::send_string(const std::string &str) {
    // Dedup: ESPHome API can deliver the same service call twice within ~5ms
    uint32_t now = millis();
    if (str == last_send_string_ && (now - last_send_string_ms_) < 30) {
        ESP_LOGD(TAG, "send_string dedup: \"%s\" (duplicate after %ums)", str.c_str(), (unsigned) (now - last_send_string_ms_));
        return;
    }
    last_send_string_ = str;
    last_send_string_ms_ = now;

    if (type_mutex_ == nullptr) return;
    if (active_layout_ == nullptr) active_layout_ = default_layout();

    // Pre-resolve keystrokes now so a mid-type layout switch can't garble what
    // was already queued. Skip unmapped codepoints rather than queuing zeros.
    std::vector<HidKeyMapping> strokes;
    strokes.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        uint32_t cp = decode_utf8_(str, i);
        HidKeyMapping m = resolve_codepoint_(active_layout_, cp);
        if (m.keycode != 0x00) {
            strokes.push_back(m);
            // Dead-key compose: emit the followup stroke after the dead key.
            // Used either to emit a bare literal accent (followup = space) or
            // to compose an accented letter (followup = a/e/i/o/u with optional
            // Shift for uppercase variants like Â Ê).
            if (m.followup_keycode != 0x00) {
                strokes.push_back({m.followup_modifier, m.followup_keycode, 0x00, 0x00});
            }
        } else {
            ESP_LOGD(TAG, "send_string: skipped unmapped codepoint U+%04X", (unsigned) cp);
        }
    }

    ESP_LOGD(TAG, "send_string: \"%s\" (len=%u, strokes=%u, queue=%u, layout=%s)",
             str.c_str(), str.size(), strokes.size(), type_queue_.size(),
             active_layout_->id);

    xSemaphoreTake(type_mutex_, portMAX_DELAY);
    type_queue_.insert(type_queue_.end(), strokes.begin(), strokes.end());
    xSemaphoreGive(type_mutex_);
}

// ── Keyboard layout: setters + NVS persistence ──────────────────────────────

void EspidfBleKeyboard::set_keyboard_layout(const std::string &id) {
    yaml_layout_id_ = id;
    const KeyboardLayout *lay = get_layout_by_id(id.c_str());
    active_layout_ = lay != nullptr ? lay : default_layout();
    ESP_LOGI(TAG, "Keyboard layout (YAML default): %s", active_layout_->id);
}

void EspidfBleKeyboard::set_runtime_layout(const std::string &id, bool persist) {
    const KeyboardLayout *lay = get_layout_by_id(id.c_str());
    if (lay == nullptr) {
        ESP_LOGW(TAG, "Unknown keyboard layout '%s' — ignoring", id.c_str());
        return;
    }
    active_layout_ = lay;
    if (persist) save_layout_(id);
    ESP_LOGI(TAG, "Keyboard layout (runtime%s): %s", persist ? "" : ", ephemeral", active_layout_->id);
}

void EspidfBleKeyboard::load_layout_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    // 1) Did the YAML keyboard_layout change since last boot?
    //    "yaml_layout" snapshots the last YAML default we saw. Same idiom as
    //    maybe_reset_bonds_after_security_config_change() does for passkey.
    char saved_yaml[16] = {0};
    size_t yaml_len = sizeof(saved_yaml);
    esp_err_t yaml_get = nvs_get_str(handle, "yaml_layout", saved_yaml, &yaml_len);
    const bool yaml_changed = (yaml_get != ESP_OK) || (yaml_layout_id_ != saved_yaml);

    if (yaml_changed) {
        // Persist the new YAML default and discard any stale runtime override
        // so the user's flash actually takes effect without a factory reset.
        nvs_set_str(handle, "yaml_layout", yaml_layout_id_.c_str());
        nvs_erase_key(handle, "layout");
        nvs_commit(handle);
        ESP_LOGI(TAG, "YAML keyboard_layout changed (%s -> %s); cleared NVS override",
                 (yaml_get == ESP_OK) ? saved_yaml : "<none>", yaml_layout_id_.c_str());
        nvs_close(handle);
        return;  // active_layout_ already points at the new YAML default
    }

    // 2) YAML unchanged — honour the user's runtime web-UI override, if any.
    char buf[16];
    size_t len = sizeof(buf);
    if (nvs_get_str(handle, "layout", buf, &len) == ESP_OK) {
        const KeyboardLayout *lay = get_layout_by_id(buf);
        if (lay != nullptr) {
            active_layout_ = lay;
            ESP_LOGI(TAG, "Keyboard layout from NVS: %s", active_layout_->id);
        } else {
            ESP_LOGW(TAG, "NVS layout '%s' unknown; keeping YAML default '%s'",
                     buf, active_layout_ != nullptr ? active_layout_->id : "us");
        }
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_layout_(const std::string &id) {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_str(handle, "layout", id.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

void EspidfBleKeyboard::save_goto_scale_for_host() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;
    char kx[12], ky[12];
    snprintf(kx, sizeof(kx), "gsx%u", active_slot_);
    snprintf(ky, sizeof(ky), "gsy%u", active_slot_);
    uint32_t bx, by;
    memcpy(&bx, &goto_scale_x_, sizeof(bx));  // store the float bit pattern as u32
    memcpy(&by, &goto_scale_y_, sizeof(by));
    nvs_set_u32(handle, kx, bx);
    nvs_set_u32(handle, ky, by);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved goto scale for host %u: x=%.4f y=%.4f", active_slot_, goto_scale_x_, goto_scale_y_);
}

bool EspidfBleKeyboard::get_saved_goto_scale(uint8_t slot, float &x, float &y) const {
    if (slot >= MAX_HOST_SLOTS) return false;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return false;
    char kx[12], ky[12];
    snprintf(kx, sizeof(kx), "gsx%u", slot);
    snprintf(ky, sizeof(ky), "gsy%u", slot);
    // Both axes must be present and in range — a half-read would otherwise put a
    // zero into a backup, which restore would then reject.
    bool got_x = false, got_y = false;
    uint32_t b;
    if (nvs_get_u32(handle, kx, &b) == ESP_OK) {
        float f; memcpy(&f, &b, sizeof(f));
        if (f >= 0.05f && f <= 20.0f) { x = f; got_x = true; }
    }
    if (nvs_get_u32(handle, ky, &b) == ESP_OK) {
        float f; memcpy(&f, &b, sizeof(f));
        if (f >= 0.05f && f <= 20.0f) { y = f; got_y = true; }
    }
    nvs_close(handle);
    return got_x && got_y;
}

void EspidfBleKeyboard::set_saved_goto_scale(uint8_t slot, float x, float y) {
    if (slot >= MAX_HOST_SLOTS) return;
    if (x < 0.05f || x > 20.0f || y < 0.05f || y > 20.0f) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;
    char kx[12], ky[12];
    snprintf(kx, sizeof(kx), "gsx%u", slot);
    snprintf(ky, sizeof(ky), "gsy%u", slot);
    uint32_t bx, by;
    memcpy(&bx, &x, sizeof(bx));
    memcpy(&by, &y, sizeof(by));
    nvs_set_u32(handle, kx, bx);
    nvs_set_u32(handle, ky, by);
    nvs_commit(handle);
    nvs_close(handle);
    // Apply immediately if this is the slot in use, so a restore doesn't need a
    // host switch to take effect.
    if (slot == active_slot_) { goto_scale_x_ = x; goto_scale_y_ = y; }
    ESP_LOGI(TAG, "Set goto scale for host %u: x=%.4f y=%.4f", (unsigned) slot, x, y);
}

void EspidfBleKeyboard::load_goto_scale_for_host(uint8_t slot) {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;
    char kx[12], ky[12];
    snprintf(kx, sizeof(kx), "gsx%u", slot);
    snprintf(ky, sizeof(ky), "gsy%u", slot);
    uint32_t b;
    if (nvs_get_u32(handle, kx, &b) == ESP_OK) {
        float f; memcpy(&f, &b, sizeof(f));
        if (f >= 0.05f && f <= 20.0f) goto_scale_x_ = f;
    }
    if (nvs_get_u32(handle, ky, &b) == ESP_OK) {
        float f; memcpy(&f, &b, sizeof(f));
        if (f >= 0.05f && f <= 20.0f) goto_scale_y_ = f;
    }
    nvs_close(handle);
    ESP_LOGI(TAG, "Goto scale for host %u: x=%.4f y=%.4f", slot, goto_scale_x_, goto_scale_y_);
}

// Every keyboard report goes through here so held keys survive whatever else is
// being typed. `extra_*` is the transient key of a tap; (0, 0) is its key-up,
// which now means "everything still held, nothing else" rather than "all up".
esp_err_t EspidfBleKeyboard::send_kb_report_(uint8_t extra_mod, uint8_t extra_key) {
    uint8_t report[8] = {0};
    report[0] = held_modifiers_ | extra_mod;
    uint8_t slot = 2;
    for (uint8_t k : held_keys_) {
        if (k == 0 || slot >= 8) continue;
        report[slot++] = k;
    }
    // Skipped when already held: a repeat of the same usage in the array reads
    // as two keys down, and hosts differ on what that means.
    if (extra_key != 0 && slot < 8) {
        bool dup = false;
        for (uint8_t k : held_keys_) if (k == extra_key) dup = true;
        if (!dup) report[slot++] = extra_key;
    }
    return send_keyboard_input_report(conn_id_, report, 8);
}

void EspidfBleKeyboard::send_key_combo(uint8_t modifiers, uint8_t keycode) {
    // Dedup: ESPHome API can deliver the same service call twice within ~5ms
    uint32_t now = millis();
    uint16_t key_id = ((uint16_t) modifiers << 8) | keycode;
    if (key_id == last_send_key_id_ && (now - last_send_key_ms_) < 30) {
        ESP_LOGD(TAG, "send_key_combo dedup: mod=0x%02X key=0x%02X (duplicate after %ums)",
                 modifiers, keycode, (unsigned) (now - last_send_key_ms_));
        return;
    }
    last_send_key_id_ = key_id;
    last_send_key_ms_ = now;

    ESP_LOGD(TAG, "send_key_combo: mod=0x%02X key=0x%02X", modifiers, keycode);
    if (!is_connected_) return;
    send_kb_report_(modifiers, keycode);
    vTaskDelay(pdMS_TO_TICKS(30));
    send_kb_report_(0, 0);
}

void EspidfBleKeyboard::send_ctrl_alt_del() {
    if (!is_connected_) return;
    send_kb_report_(0x05, 0x4C);
    vTaskDelay(pdMS_TO_TICKS(50));
    send_kb_report_(0, 0);
}

void EspidfBleKeyboard::send_sleep() {
    if (!is_connected_) return;
    uint8_t report[1] = {0x82};  // System Sleep
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    esp_ble_gatts_set_attr_value(s_system_report_handle, 1, report);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    esp_ble_gatts_set_attr_value(s_system_report_handle, 1, release);
    ESP_LOGI(TAG, "System Sleep sent");
}

void EspidfBleKeyboard::send_shutdown() {
    if (!is_connected_) return;
    send_power();
}

void EspidfBleKeyboard::send_consumer(uint16_t usage) {
    if (!is_connected_) return;
    uint32_t now = millis();
    if (usage == last_consumer_usage_ && (now - last_consumer_ms_) < 30) {
        ESP_LOGD(TAG, "send_consumer dedup: 0x%04X (duplicate after %ums)", usage, (unsigned) (now - last_consumer_ms_));
        return;
    }
    last_consumer_usage_ = usage;
    last_consumer_ms_ = now;
    uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, report, false);
    esp_ble_gatts_set_attr_value(s_consumer_report_handle, 2, report);
    vTaskDelay(pdMS_TO_TICKS(50));
    // The consumer report has a single usage field, so a held usage cannot stay
    // down *during* another one — it is put back afterwards instead of the zero
    // release. Unlike the keyboard's 6-key array, this is the best the report
    // map allows; the host sees a brief interruption of the held usage.
    uint8_t release[2] = {(uint8_t)(held_consumer_ & 0xFF), (uint8_t)(held_consumer_ >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, release, false);
    esp_ble_gatts_set_attr_value(s_consumer_report_handle, 2, release);
    ESP_LOGI(TAG, "Consumer report sent: 0x%04X", usage);
}

// ── Press and hold (push-to-talk) ─────────────────────────────────
//
// The keyboard/consumer counterpart of send_mouse_click_start(): the report goes
// down and stays down until release_held(). Everything else in this file taps —
// key down, short delay, key up — which is why a physical button held for five
// seconds still reached the host as a brief press before this existed.

void EspidfBleKeyboard::key_hold(uint8_t modifiers, uint8_t keycode) {
    if (!is_connected_) return;
    if (!has_held()) hold_start_ms_ = millis();
    held_modifiers_ |= modifiers;
    // Idempotent: holding an already-held key is a no-op, so a repeated press
    // event (or the API's double-fire) can't fill the 6-key array with copies.
    if (keycode != 0) {
        bool present = false;
        for (uint8_t k : held_keys_) if (k == keycode) present = true;
        if (!present) {
            bool placed = false;
            for (uint8_t &k : held_keys_) {
                if (k != 0) continue;
                k = keycode;
                placed = true;
                break;
            }
            if (!placed) {
                ESP_LOGW(TAG, "key_hold: already holding %u keys — 0x%02X ignored",
                         (unsigned) held_key_count_(), keycode);
            }
        }
    }
    send_kb_report_(0, 0);
    ESP_LOGI(TAG, "Key hold: mod=0x%02X key=0x%02X (%u held)", modifiers, keycode,
             (unsigned) held_key_count_());
}

void EspidfBleKeyboard::consumer_hold(uint16_t usage) {
    if (!is_connected_ || usage == 0) return;
    if (!has_held()) hold_start_ms_ = millis();
    held_consumer_ = usage;
    uint8_t report[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, report, false);
    esp_ble_gatts_set_attr_value(s_consumer_report_handle, 2, report);
    ESP_LOGI(TAG, "Consumer hold: 0x%04X", usage);
}

void EspidfBleKeyboard::release_held() {
    bool had_keys = held_modifiers_ != 0 || held_key_count_() > 0;
    bool had_consumer = held_consumer_ != 0;
    held_modifiers_ = 0;
    memset(held_keys_, 0, sizeof(held_keys_));
    held_consumer_ = 0;
    if (!is_connected_) {
        held_mouse_buttons_ = 0;
        return;
    }
    if (had_keys) send_kb_report_(0, 0);
    if (had_consumer) {
        uint8_t release[2] = {0, 0};
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_consumer_report_handle, 2, release, false);
        esp_ble_gatts_set_attr_value(s_consumer_report_handle, 2, release);
    }
    // Mouse buttons too: "release" means everything this device is holding, and
    // a hold list can name left_click as readily as a key.
    if (held_mouse_buttons_ != 0) send_mouse_click_release();
    if (had_keys || had_consumer) ESP_LOGI(TAG, "Released all held keys");
}

void EspidfBleKeyboard::send_power() {
    if (!is_connected_) return;
    uint8_t report[1] = {0x81};  // System Power Down
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, report, false);
    esp_ble_gatts_set_attr_value(s_system_report_handle, 1, report);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t release[1] = {0};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_system_report_handle, 1, release, false);
    esp_ble_gatts_set_attr_value(s_system_report_handle, 1, release);
    ESP_LOGI(TAG, "System Power Down sent");
}

void EspidfBleKeyboard::send_media_play_pause() { send_consumer(0x00CD); }
void EspidfBleKeyboard::send_media_next()        { send_consumer(0x00B5); }
void EspidfBleKeyboard::send_media_prev()        { send_consumer(0x00B6); }
void EspidfBleKeyboard::send_media_stop()        { send_consumer(0x00B7); }
void EspidfBleKeyboard::send_media_record()      { send_consumer(0x00B2); }

// ── Remote button actions ─────────────────────────────────────────
//
// The remote's buttons used to be hardcoded "consumer:0x...." / "combo:.:.."
// strings in the UIs. They are named here instead so each one can be remapped
// per host slot (parametric forms are dispatched before the override lookup and
// stay literal by design). The codes are exactly what those buttons sent
// before, so the default behaviour is unchanged.
//
// Note "remote_power" is not "power": the latter is a System Power Down report,
// a different thing from the remote's HID consumer Power usage.

struct NamedConsumer {
    const char *name;
    uint16_t usage;
};
static const NamedConsumer NAMED_CONSUMERS[] = {
    {"remote_power", 0x0030}, {"search", 0x0221}, {"info", 0x0209},
    {"home", 0x0223},         {"back", 0x0224},
    {"up", 0x0042},           {"down", 0x0043},   {"left", 0x0044},
    {"right", 0x0045},
    {"rewind", 0x00B4},       {"fast_forward", 0x00B3},
    {"app_explorer", 0x0194}, {"app_browser", 0x0223},
    {"app_email", 0x018A},    {"app_calc", 0x0192},
};

struct NamedCombo {
    const char *name;
    uint8_t modifier;
    uint8_t keycode;
};
static const NamedCombo NAMED_COMBOS[] = {
    // OK sends keyboard Enter, not Consumer Menu Pick (0x0041). Far more hosts
    // accept Enter: TVs and smart monitors commonly implement the Menu
    // Up/Down/Left/Right usages but ignore Menu Pick, leaving OK dead while the
    // D-pad works (seen on a Samsung smart monitor). A host that does want Menu
    // Pick gets it back with a per-host override: ok -> consumer:0x0041.
    {"ok", 0, 0x28},            // Enter
    {"channel_up", 0, 0x4B},    // Page Up
    {"channel_down", 0, 0x4E},  // Page Down
    {"color_red", 0, 0x3A},     // F1
    {"color_green", 0, 0x3B},   // F2
    {"color_yellow", 0, 0x3C},  // F3
    {"color_blue", 0, 0x3D},    // F4
};

bool EspidfBleKeyboard::execute_remote_action_(const std::string &action) {
    for (const auto &e : NAMED_CONSUMERS) {
        if (action == e.name) { send_consumer(e.usage); return true; }
    }
    for (const auto &e : NAMED_COMBOS) {
        if (action == e.name) { send_key_combo(e.modifier, e.keycode); return true; }
    }
    return false;
}

// Hold whatever `action` resolves to, instead of tapping it.
//
// Deliberately a separate dispatcher rather than a "hold mode" flag threaded
// through execute_action(): that one recurses through repeat:, alternate: and
// the '|' split, and a sequence has no single key to leave down. Only actions
// that map to one report can be held, and the caller runs the rest normally.
bool EspidfBleKeyboard::hold_action(const std::string &action) {
    // A sequence has no single key to leave down. Caught before the parsing
    // below, which would otherwise match the first step and quietly drop the
    // rest — better to hand the whole thing back and let it run once.
    if (action.find('|') != std::string::npos) return false;

    // Parametric forms first and overrides after, the same order execute_action
    // uses — so `combo:` and `consumer:` always mean exactly what they say and
    // only bare names are remappable.
    int a = 0, b = 0;
    if (sscanf(action.c_str(), "combo:%i:%i", &a, &b) == 2) {
        key_hold((uint8_t) a, (uint8_t) b);
        return true;
    }
    if (sscanf(action.c_str(), "key_hold:%i:%i", &a, &b) == 2) {
        key_hold((uint8_t) a, (uint8_t) b);
        return true;
    }
    if (sscanf(action.c_str(), "consumer:%i", &a) == 1 ||
        sscanf(action.c_str(), "consumer_hold:%i", &a) == 1) {
        consumer_hold((uint16_t) a);
        return true;
    }
    if (sscanf(action.c_str(), "mouse_click:%i", &a) == 1 ||
        sscanf(action.c_str(), "mouse_hold:%i", &a) == 1) {
        send_mouse_click_start((uint8_t) a);
        return true;
    }
    // Per-host override, so holding `record` on a Windows slot holds whatever
    // that slot remapped it to. Depth 0 only, exactly as in execute_action():
    // an override naming itself, or a pair naming each other, resolves to the
    // built-in rather than recursing.
    if (override_depth_ == 0) {
        const std::string *ovr = find_override_(active_slot_, action);
        if (ovr != nullptr) {
            std::string body = *ovr;
            override_depth_++;
            bool held = hold_action(body);
            override_depth_--;
            return held;
        }
    }

    if (action == "left_click")   { send_mouse_click_start(0x01); return true; }
    if (action == "right_click")  { send_mouse_click_start(0x02); return true; }
    if (action == "middle_click") { send_mouse_click_start(0x04); return true; }

    for (const auto &e : NAMED_CONSUMERS) {
        if (action == e.name) { consumer_hold(e.usage); return true; }
    }
    for (const auto &e : NAMED_COMBOS) {
        if (action == e.name) { key_hold(e.modifier, e.keycode); return true; }
    }
    return false;
}
void EspidfBleKeyboard::send_volume_up()         { send_consumer(0x00E9); }
void EspidfBleKeyboard::send_volume_down()       { send_consumer(0x00EA); }
void EspidfBleKeyboard::send_mute()              { send_consumer(0x00E2); }

void EspidfBleKeyboard::send_mouse_report_(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    uint8_t report[4] = {buttons, static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(wheel)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_mouse_report_handle, sizeof(report), report, false);
    esp_ble_gatts_set_attr_value(s_mouse_report_handle, sizeof(report), report);
}

void EspidfBleKeyboard::send_mouse_click_start(uint8_t buttons) {
    if (!is_connected_) return;
    uint32_t now = millis();
    if (buttons == last_mouse_click_ && (now - last_mouse_click_ms_) < 30) {
        ESP_LOGD(TAG, "send_mouse_click dedup: 0x%02X (duplicate after %ums)", buttons, (unsigned) (now - last_mouse_click_ms_));
        return;
    }
    last_mouse_click_ = buttons;
    last_mouse_click_ms_ = now;
    // Starts the max_key_hold_ms clock like key_hold does. Without it a drag —
    // which has_held() counts — would be measured against a stale timestamp and
    // released the moment the guard next ran.
    if (!has_held()) hold_start_ms_ = now;
    held_mouse_buttons_ = buttons;
    send_mouse_report_(buttons, 0, 0, 0);
    ESP_LOGI(TAG, "Mouse click start sent: buttons=0x%02X", buttons);
}

void EspidfBleKeyboard::send_mouse_click_release() {
    if (!is_connected_) return;
    held_mouse_buttons_ = 0;
    send_mouse_report_(0, 0, 0, 0);
    ESP_LOGI(TAG, "Mouse click release sent");
}

void EspidfBleKeyboard::send_mouse_click(uint8_t buttons) {
    send_mouse_click_start(buttons);
    vTaskDelay(pdMS_TO_TICKS(50));
    send_mouse_click_release();
}

// Moves/scrolls are a single report each — no delay, no trailing idle report.
// Relative reports need no "release" (hosts don't dedupe them; only absolute
// reports get deduped), and blocking here stalls the web server's HTTP task,
// which is the touchpad hot path.
void EspidfBleKeyboard::send_mouse_move(int8_t x, int8_t y) {
    if (!is_connected_) return;
    send_mouse_report_(held_mouse_buttons_, x, y, 0);
    ESP_LOGV(TAG, "Mouse move sent: x=%d y=%d", x, y);
}

void EspidfBleKeyboard::send_mouse_scroll(int8_t wheel) {
    if (!is_connected_) return;
    send_mouse_report_(held_mouse_buttons_, 0, 0, wheel);
    ESP_LOGV(TAG, "Mouse scroll sent: wheel=%d", wheel);
}

void EspidfBleKeyboard::send_mouse_move_abs(uint16_t x, uint16_t y, uint8_t buttons) {
    if (!is_connected_) return;
    if (x > 0x7FFF) x = 0x7FFF;
    if (y > 0x7FFF) y = 0x7FFF;
    // Report ID 5 layout: [buttons, X_lo, X_hi, Y_lo, Y_hi]. No zero-release —
    // a zeroed absolute report would snap the cursor to the top-left corner.
    uint8_t report[5] = {buttons,
                         static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>(x >> 8),
                         static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>(y >> 8)};
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_abs_mouse_report_handle, 5, report, false);
    esp_ble_gatts_set_attr_value(s_abs_mouse_report_handle, 5, report);
    if (buttons != 0) {
        // Click-at-position: release the button while staying at the same coords.
        vTaskDelay(pdMS_TO_TICKS(50));
        uint8_t release[5] = {0, report[1], report[2], report[3], report[4]};
        esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, s_abs_mouse_report_handle, 5, release, false);
        esp_ble_gatts_set_attr_value(s_abs_mouse_report_handle, 5, release);
    }
    // Remember where WE put the cursor (for save/restore). Note: this is only the
    // device-commanded position; HID can't read the host's real cursor.
    cur_abs_x_ = x;
    cur_abs_y_ = y;
    ESP_LOGD(TAG, "Mouse abs move sent: x=%u y=%u buttons=0x%02X", x, y, buttons);
}

void EspidfBleKeyboard::send_mouse_goto(int32_t x, int32_t y) {
    if (!is_connected_) return;
    // Clamp to a sane virtual-desktop range to avoid runaway stepping.
    if (x < -32000) x = -32000; else if (x > 32000) x = 32000;
    if (y < -32000) y = -32000; else if (y > 32000) y = 32000;
    last_goto_x_ = x;
    last_goto_y_ = y;
    // 1) Anchor at ~the desktop origin (primary monitor top-left = Windows 0,0),
    //    which the absolute pointer reliably reaches. Two sends, for two reasons:
    //    (a) hosts ignore an absolute report IDENTICAL to the previous one (the
    //    prior goto also ended near the origin); and (b) Windows ignores an
    //    ALL-ZERO absolute report entirely — so home to (1,1) (a fraction of a
    //    pixel from the corner), not (0,0). Without this the cursor wouldn't
    //    re-home and the relative steps would pile on from the last position,
    //    drifting toward a corner.
    send_mouse_move_abs(64, 64);
    vTaskDelay(pdMS_TO_TICKS(20));
    send_mouse_move_abs(1, 1);
    vTaskDelay(pdMS_TO_TICKS(40));
    // 2) Step there relatively in <=127px chunks. Relative movement crosses
    //    monitor boundaries, so this reaches any monitor (incl. negative coords).
    //    Apply the calibration scale to compensate the host's pointer-speed/DPI
    //    scaling (e.g. ~0.5 if the cursor otherwise travels twice as far).
    //    Steps carry held_mouse_buttons_ so a click-and-hold drag survives the
    //    goto. The abs homing above must NOT carry it: send_mouse_move_abs
    //    treats nonzero buttons as click-at-position and would release the drag.
    float fx = (float) x * goto_scale_x_, fy = (float) y * goto_scale_y_;
    int32_t dx = (int32_t)(fx < 0 ? fx - 0.5f : fx + 0.5f);
    int32_t dy = (int32_t)(fy < 0 ? fy - 0.5f : fy + 0.5f);
    if (dx < -32000) dx = -32000; else if (dx > 32000) dx = 32000;
    if (dy < -32000) dy = -32000; else if (dy > 32000) dy = 32000;
    // Ground-truth diagnostic: shows the actual scales in effect and the relative
    // distance being sent. If scale != your YAML value, the config isn't reaching
    // the firmware; if relative is right but the cursor overshoots, it's host-side.
    ESP_LOGI(TAG, "Mouse goto: target=(%d,%d) scale=(%.4f,%.4f) relative=(%d,%d)",
             (int) x, (int) y, goto_scale_x_, goto_scale_y_, (int) dx, (int) dy);
    // Decoupled move that still crosses monitors. The cursor homes to the top-left
    // corner, so a straight X run travels along the top edge and sticks at a
    // monitor's top-right corner (can't change screens — the "3839" jam). So:
    //   Phase 1: drop Y to MID-screen   Phase 2: cross X there   Phase 3: Y -> target
    // Y is always moved on its own (never bundled with X), so it doesn't inherit
    // X's diagonal speed — that's what caused the location drift.
    int32_t dmid = (int32_t)((float)(screen_h_ / 2u) * goto_scale_y_ + 0.5f);
    int32_t d = dmid;                       // Phase 1: Y -> mid-screen
    while (d != 0) {
        int32_t s = d > 127 ? 127 : (d < -127 ? -127 : d);
        send_mouse_report_(held_mouse_buttons_, 0, static_cast<int8_t>(s), 0);
        d -= s;
        vTaskDelay(pdMS_TO_TICKS(8));
    }
    d = dx;                                 // Phase 2: X across (mid-screen Y)
    while (d != 0) {
        int32_t s = d > 127 ? 127 : (d < -127 ? -127 : d);
        send_mouse_report_(held_mouse_buttons_, static_cast<int8_t>(s), 0, 0);
        d -= s;
        vTaskDelay(pdMS_TO_TICKS(8));
    }
    d = dy - dmid;                          // Phase 3: Y from mid -> target
    while (d != 0) {
        int32_t s = d > 127 ? 127 : (d < -127 ? -127 : d);
        send_mouse_report_(held_mouse_buttons_, 0, static_cast<int8_t>(s), 0);
        d -= s;
        vTaskDelay(pdMS_TO_TICKS(8));
    }
    // Settle nudge: after a burst of injected moves Windows can leave the cursor
    // sprite visually stale (it only redraws on a "real" mouse event), so it looks
    // like nothing happened until you touch a physical mouse. A +1/-1 wiggle nets
    // ~zero displacement but forces the cursor to redraw at the final spot.
    send_mouse_report_(held_mouse_buttons_, 1, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_mouse_report_(held_mouse_buttons_, -1, 0, 0);
}

void EspidfBleKeyboard::send_hibernate() {
    if (!is_connected_) return;
    // Win+R is a single blocking key combo (no watchdog risk).
    send_key_combo(0x08, 0x15);
    vTaskDelay(pdMS_TO_TICKS(600));
    // Queue the command text + Enter through the non-blocking state machine.
    send_string("shutdown /h\n");
}

// ── Centralized action executor ──────────────────────────────────

void EspidfBleKeyboard::execute_action(const std::string &action) {
    // Repeat: run the rest of the action N times. Checked before the '|' split
    // so the count covers the whole remaining sequence. Runs inline/synchronously
    // like other multi-step macros, so the count is capped to bound how long it
    // can block the caller; unbounded/background looping is handled separately.
    if (action.find("repeat:") == 0) {
        int count = 0;
        size_t sep = action.find(':', 7);  // ':' after the count
        if (sep != std::string::npos && sscanf(action.c_str(), "repeat:%i", &count) == 1) {
            std::string body = action.substr(sep + 1);
            const int MAX_REPEAT = 1000;  // inline safety cap
            if (count > MAX_REPEAT) count = MAX_REPEAT;
            for (int i = 0; i < count && !body.empty(); i++)
                execute_action(body);
        }
        return;
    }
    // Alternate: run ONE step per invocation instead of all of them, cycling
    // on each call. Like repeat:, it must be checked before the '|' split
    // because it needs the whole remaining sequence, not a single step.
    //
    // The counter is keyed on the body rather than the caller, so the same
    // alternate string driven from the web remote, the HA card and a macro
    // stays in step — they're all working one physical device.
    //
    // This is assumed state: HID is one-way, so the device cannot know what it
    // actually toggled. Switch the target off by other means and the sequence
    // is inverted until it's pressed through once more.
    if (action.find("alternate:") == 0) {
        std::string body = action.substr(10);
        // Branches are separated by '||'. A single '|' keeps its usual meaning
        // of "next step", so each branch can be a whole sequence — a power-off
        // that needs "code, wait, confirm" is one branch, not three presses.
        std::vector<std::string> branches;
        size_t start = 0;
        while (true) {
            size_t sep = body.find("||", start);
            std::string branch = (sep == std::string::npos) ? body.substr(start)
                                                            : body.substr(start, sep - start);
            while (!branch.empty() && branch.front() == ' ') branch.erase(branch.begin());
            while (!branch.empty() && branch.back() == ' ') branch.pop_back();
            if (!branch.empty()) branches.push_back(branch);
            if (sep == std::string::npos) break;
            start = sep + 2;
        }
        if (branches.empty()) return;

        uint8_t idx = 0;
        auto it = alternate_index_.find(body);
        if (it != alternate_index_.end()) {
            idx = it->second;
            it->second = (uint8_t) ((idx + 1) % branches.size());
        } else if (alternate_index_.size() < MAX_ALTERNATE_COUNTERS) {
            alternate_index_[body] = (uint8_t) (1 % branches.size());
        }
        // Past the cap the counter simply isn't tracked and branch 0 runs every
        // time — a bounded map matters more than toggling an unbounded number
        // of distinct sequences.
        //
        // Recursing into execute_action means a branch is a normal action
        // string: multi-step, repeat:, delays, everything.
        execute_action(branches[idx % branches.size()]);
        return;
    }
    // Multi-step actions: split on '|' and execute each step
    if (action.find('|') != std::string::npos) {
        size_t start = 0;
        while (start < action.size()) {
            size_t end = action.find('|', start);
            if (end == std::string::npos) end = action.size();
            std::string step = action.substr(start, end - start);
            while (!step.empty() && step.front() == ' ') step.erase(step.begin());
            while (!step.empty() && step.back() == ' ') step.pop_back();
            if (!step.empty()) {
                execute_action(step);
            }
            start = end + 1;
            if (start < action.size() && step.find("delay:") != 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        return;
    }
    // Delay action for multi-step macros
    if (action.find("delay:") == 0) {
        int ms = 0;
        if (sscanf(action.c_str(), "delay:%i", &ms) == 1 && ms > 0 && ms <= 10000)
            vTaskDelay(pdMS_TO_TICKS(ms));
        return;
    }
    // Parametric actions
    if (action.find("combo:") == 0) {
        int mod = 0, key = 0;
        if (sscanf(action.c_str(), "combo:%i:%i", &mod, &key) == 2)
            send_key_combo((uint8_t) mod, (uint8_t) key);
        return;
    }
    if (action.find("consumer:") == 0) {
        int usage = 0;
        if (sscanf(action.c_str(), "consumer:%i", &usage) == 1)
            send_consumer((uint16_t) usage);
        return;
    }
    if (action.find("mouse_click:") == 0) {
        int buttons = 0;
        if (sscanf(action.c_str(), "mouse_click:%i", &buttons) == 1)
            send_mouse_click((uint8_t) buttons);
        return;
    }
    if (action.find("mouse_hold:") == 0) {
        int buttons = 0;
        if (sscanf(action.c_str(), "mouse_hold:%i", &buttons) == 1)
            send_mouse_click_start((uint8_t) buttons);
        return;
    }
    // Press and hold — the key stays down on the host until `release`. This is
    // what makes push-to-talk work; every other keyboard action here taps.
    if (action.find("key_hold:") == 0) {
        int mod = 0, key = 0;
        if (sscanf(action.c_str(), "key_hold:%i:%i", &mod, &key) == 2)
            key_hold((uint8_t) mod, (uint8_t) key);
        return;
    }
    if (action.find("consumer_hold:") == 0) {
        int usage = 0;
        if (sscanf(action.c_str(), "consumer_hold:%i", &usage) == 1)
            consumer_hold((uint16_t) usage);
        return;
    }
    // Generic form: hold whatever the body resolves to, including named and
    // per-host-overridden actions. A body that can't be held still does
    // something — running it once beats a button that appears dead.
    if (action.find("hold:") == 0) {
        std::string body = action.substr(5);
        while (!body.empty() && body.front() == ' ') body.erase(body.begin());
        if (body.empty()) return;
        if (!hold_action(body)) {
            ESP_LOGW(TAG, "'%s' cannot be held — running it once instead", body.c_str());
            execute_action(body);
        }
        return;
    }
    if (action.find("mouse_move:") == 0) {
        int x = 0, y = 0;
        if (sscanf(action.c_str(), "mouse_move:%i:%i", &x, &y) == 2)
            send_mouse_move((int8_t) x, (int8_t) y);
        return;
    }
    if (action.find("mouse_scroll:") == 0) {
        int wheel = 0;
        if (sscanf(action.c_str(), "mouse_scroll:%i", &wheel) == 1)
            send_mouse_scroll((int8_t) wheel);
        return;
    }
    // Absolute pointer: percent of the mapped space (0..100).
    if (action.find("mouse_abs:") == 0) {
        float px = 0, py = 0;
        if (sscanf(action.c_str(), "mouse_abs:%f:%f", &px, &py) == 2) {
            px = px < 0 ? 0 : (px > 100 ? 100 : px);
            py = py < 0 ? 0 : (py > 100 ? 100 : py);
            send_mouse_move_abs((uint16_t)(px / 100.0f * 32767.0f),
                                (uint16_t)(py / 100.0f * 32767.0f));
        }
        return;
    }
    // Absolute pointer: pixels within the configured screen/virtual-desktop size.
    if (action.find("mouse_abs_px:") == 0) {
        float px = 0, py = 0;
        if (sscanf(action.c_str(), "mouse_abs_px:%f:%f", &px, &py) == 2 &&
            screen_w_ > 0 && screen_h_ > 0) {
            float fx = px / (float) screen_w_;
            float fy = py / (float) screen_h_;
            fx = fx < 0 ? 0 : (fx > 1 ? 1 : fx);
            fy = fy < 0 ? 0 : (fy > 1 ? 1 : fy);
            send_mouse_move_abs((uint16_t)(fx * 32767.0f), (uint16_t)(fy * 32767.0f));
        }
        return;
    }
    // Absolute pointer: percent within a declared monitor's region.
    if (action.find("mouse_abs_mon:") == 0) {
        int idx = 0; float px = 0, py = 0;
        if (sscanf(action.c_str(), "mouse_abs_mon:%i:%f:%f", &idx, &px, &py) == 3 &&
            idx >= 0 && idx < (int) monitors_.size() && screen_w_ > 0 && screen_h_ > 0) {
            const auto &m = monitors_[idx];
            px = px < 0 ? 0 : (px > 100 ? 100 : px);
            py = py < 0 ? 0 : (py > 100 ? 100 : py);
            float fx = (m.x + px / 100.0f * m.width) / (float) screen_w_;
            float fy = (m.y + py / 100.0f * m.height) / (float) screen_h_;
            fx = fx < 0 ? 0 : (fx > 1 ? 1 : fx);
            fy = fy < 0 ? 0 : (fy > 1 ? 1 : fy);
            send_mouse_move_abs((uint16_t)(fx * 32767.0f), (uint16_t)(fy * 32767.0f));
        }
        return;
    }
    // Absolute desktop pixel via home-then-relative: works across ALL monitors
    // (relative movement spans the virtual desktop, unlike the absolute pointer
    // which Windows confines to the primary monitor). X/Y are Windows virtual-
    // desktop coordinates (the primary monitor's top-left is 0,0; screens left of
    // it are negative). Requires "Enhance pointer precision" OFF and a fixed
    // pointer-speed slider position the per-axis scale is calibrated to (moving
    // it even one notch loses pixel accuracy).
    if (action.find("mouse_goto:") == 0) {
        int tx = 0, ty = 0;
        if (sscanf(action.c_str(), "mouse_goto:%i:%i", &tx, &ty) == 2)
            send_mouse_goto(tx, ty);
        return;
    }
    if (action.find("switch_host:") == 0) {
        int slot = 0;
        if (sscanf(action.c_str(), "switch_host:%i", &slot) == 1)
            switch_host((uint8_t) slot);
        return;
    }
    if (action.find("forget_host:") == 0) {
        int slot = 0;
        if (sscanf(action.c_str(), "forget_host:%i", &slot) == 1)
            forget_host((uint8_t) slot);
        return;
    }

    // Per-host override: a named action can be remapped for the active host slot
    // (e.g. "record" -> Game Bar's Win+Alt+R on a Windows host, while a TV host
    // keeps the HID Record usage). Only bare names reach here — every parametric
    // form above has already returned — so only named actions are remappable.
    // Applied at depth 0 only, so an override body that names itself or chains
    // back to itself runs the built-in action instead of recursing.
    if (override_depth_ == 0) {
        const std::string *ovr = find_override_(active_slot_, action);
        if (ovr != nullptr) {
            // Copy before executing: a web edit runs on the HTTP task and could
            // reallocate the vector, and an override body can block for a long
            // time (delay:/repeat: chains). Same unguarded read/write window as
            // macros — the copy just closes the widest part of it.
            std::string body = *ovr;
            override_depth_++;
            execute_action(body);
            override_depth_--;
            return;
        }
    }

    // Named actions
    if (action == "ctrl_alt_del")      send_ctrl_alt_del();
    else if (action == "sleep")        send_sleep();
    else if (action == "shutdown")     send_shutdown();
    else if (action == "hibernate")    send_hibernate();
    else if (action == "power")        send_power();
    else if (action == "play_pause")   send_media_play_pause();
    else if (action == "next_track")   send_media_next();
    else if (action == "prev_track")   send_media_prev();
    else if (action == "stop")         send_media_stop();
    else if (action == "record")       send_media_record();
    else if (action == "volume_up")    send_volume_up();
    else if (action == "volume_down")  send_volume_down();
    else if (action == "mute")         send_mute();
    else if (action == "left_click")   send_mouse_click(0x01);
    else if (action == "right_click")  send_mouse_click(0x02);
    else if (action == "middle_click") send_mouse_click(0x04);
    else if (action == "left_click_hold")   send_mouse_click_start(0x01);
    else if (action == "right_click_hold")  send_mouse_click_start(0x02);
    else if (action == "middle_click_hold") send_mouse_click_start(0x04);
    else if (action == "mouse_release")     send_mouse_click_release();
    // Ends any hold — keys, consumer usage and mouse buttons. `key_release` is
    // the name the YAML automation action uses, kept as an alias so both read
    // naturally next to `key_hold:`.
    else if (action == "release" || action == "key_release") release_held();
    else if (action == "mouse_abs_save") {
        saved_abs_x_ = cur_abs_x_; saved_abs_y_ = cur_abs_y_; has_saved_abs_ = true;
    }
    else if (action == "mouse_abs_restore") {
        if (has_saved_abs_) send_mouse_move_abs(saved_abs_x_, saved_abs_y_);
    }
#ifdef USE_TEXT
    else if (action == "send_custom_text" || action.find("send_custom_text:") == 0) {
        int idx = 0;
        if (action.find("send_custom_text:") == 0)
            sscanf(action.c_str(), "send_custom_text:%i", &idx);
        if (idx >= 0 && idx < (int) custom_texts_.size() && !custom_texts_[idx]->state.empty())
            send_string(custom_texts_[idx]->state);
    }
#endif
    // Press another ESPHome button (wake_on_lan, template, …) by object id.
    // Must precede the remote/string/typed-text fallbacks below, or the action
    // string gets typed to the host as literal text instead of running.
    else if (action.find("press_button:") == 0) {
        press_external_button_(action.substr(13));
    }
    // Run a stored macro by name. Same placement rule as press_button: above —
    // an unmatched name must not reach the send-as-text fallback.
    else if (action.find("macro:") == 0) {
        run_macro_by_name_(action.substr(6));
    }
    // Remote buttons (D-pad, Power, Channel, colour and app keys) — table-driven
    // so the chain doesn't carry 22 near-identical branches.
    else if (execute_remote_action_(action)) { /* handled */ }
    else if (action.find("string:") == 0) send_string(action.substr(7));
    else send_string(action);  // Fallback: send as typed text
}

bool EspidfBleKeyboard::execute_macro(uint8_t index) {
    if (index >= macros_.size()) return false;
    execute_action(macros_[index].action);
    return true;
}

// Backs the `macro:<name>` action, so a host override can *reference* a macro
// rather than hold a copy of its text and drift when the macro is edited.
//
// By name, not index: delete_macro erases from the middle of the vector, so an
// index would silently repoint at whatever shifted up. Names survive that, and
// a missing one fails loudly instead of running the wrong thing.
void EspidfBleKeyboard::run_macro_by_name_(const std::string &name) {
    // Macros can now call each other, so a cycle is reachable. These run inline
    // (callers depend on the steps completing in order), so the stack is the
    // thing at risk — a depth cap is the guard, not deferral.
    if (macro_depth_ >= MAX_MACRO_DEPTH) {
        ESP_LOGW(TAG, "Macro nesting too deep at '%s' — stopping", name.c_str());
        return;
    }
    for (const auto &m : macros_) {
        if (m.name != name) continue;
        macro_depth_++;
        execute_action(m.action);
        macro_depth_--;
        return;
    }
    ESP_LOGW(TAG, "No macro named '%s'", name.c_str());
}

void EspidfBleKeyboard::press_external_button_(const std::string &object_id) {
    if (!expose_buttons_) return;
    char oid_buf[OBJECT_ID_MAX_LEN];
    for (auto *b : App.get_buttons()) {
        if (b == nullptr) continue;
        // get_object_id_to writes into the caller's buffer and hands back a
        // StringRef; .str() because StringRef::c_str() isn't guaranteed to be
        // null-terminated, only the pointer/length pair is meaningful.
        if (b->get_object_id_to(oid_buf).str() != object_id) continue;
        // Re-checked here, not just in the scan: without it a hidden button
        // stays reachable by typing its action into a macro by hand.
        if (std::find(hidden_buttons_.begin(), hidden_buttons_.end(), b) != hidden_buttons_.end()) {
            ESP_LOGW(TAG, "Button '%s' is in hide_buttons — not pressed", object_id.c_str());
            return;
        }
        // Only a button triggering *itself* is refused. Chaining to a different
        // button is legitimate — a template button deciding between two actions
        // and calling execute_action("press_button:…") is the documented way to
        // build a stateful toggle.
        if (pressing_button_ == b) {
            ESP_LOGW(TAG, "Button '%s' triggers itself — not pressed", object_id.c_str());
            return;
        }
        ESP_LOGI(TAG, "Pressing button '%s'", object_id.c_str());
        // Run it on the main loop, not the caller's task. Web handlers execute
        // on the AsyncTCP task, and unlike every other action here — which are
        // known BLE writes — this runs whatever automation the user attached:
        // network I/O, `delay:` steps needing the scheduler, anything.
        //
        // Deferring also means a nested press schedules another loop iteration
        // rather than growing the stack, so a chain can't overflow it.
        this->defer([this, b]() {
            button::Button *prev = pressing_button_;
            pressing_button_ = b;
            b->press();
            pressing_button_ = prev;
        });
        return;
    }
    ESP_LOGW(TAG, "No button with object id '%s'", object_id.c_str());
}

// Buttons from other ESPHome platforms, discovered once on first use. Built
// lazily rather than in setup() because components register with App in an
// order this component can't rely on — scanning too early misses whatever
// initialises after us.
//
// Actions key off the entity's object id rather than a list index: these
// strings get persisted in NVS by macros and per-host overrides, and an index
// would silently repoint every stored override the moment a button is added to
// the YAML.
const std::vector<EspidfBleKeyboard::ButtonInfo> &EspidfBleKeyboard::get_external_buttons() {
    if (external_scanned_ || !expose_buttons_) return external_buttons_;
    external_scanned_ = true;

    char oid_buf[OBJECT_ID_MAX_LEN];
    for (auto *b : App.get_buttons()) {
        if (b == nullptr || b->is_internal()) continue;
        if (std::find(own_buttons_.begin(), own_buttons_.end(), b) != own_buttons_.end())
            continue;  // ours already — registered with its real action
        if (std::find(hidden_buttons_.begin(), hidden_buttons_.end(), b) != hidden_buttons_.end())
            continue;  // hide_buttons:
        // .str() on both: these are StringRefs into ESPHome's own storage, and
        // c_str() on one isn't guaranteed null-terminated. str() copies using
        // the length, which is what we want anyway since we keep the strings.
        external_buttons_.push_back({b->get_name().str(),
                                     "press_button:" + b->get_object_id_to(oid_buf).str()});
    }
    ESP_LOGI(TAG, "Discovered %u external button(s) for the web page",
             (unsigned) external_buttons_.size());
    return external_buttons_;
}

void EspidfBleKeyboardButton::press_action() {
    if (!parent_) return;
    parent_->execute_action(action_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome




