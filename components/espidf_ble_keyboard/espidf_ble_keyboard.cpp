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
#include <cmath>
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

// Whether the controller is believed to be advertising, and when a cycle was
// last asked for. Nothing retried a cycle that never got going: a start that
// fails is logged and dropped, and advertising is only ever started from the
// adv-data completion events, so one of those going missing leaves the keyboard
// silent until the next host switch — which from the outside is indisting-
// uishable from a host that simply has not come back.
static std::atomic<bool> s_adv_running{false};
static std::atomic<uint32_t> s_adv_attempt_ms{0};
// Long enough that a slow stack is not talked over, short enough that a host
// looking for this keyboard finds it before anyone gives up and reaches for the
// phone's Bluetooth page.
static constexpr uint32_t ADV_RETRY_MS = 10000;


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
                    } else {
                        s_instance->bond_log_record(BOND_LOSS_CONFIG, 0, 0xFF,
                                                    bonded[static_cast<size_t>(i)].bd_addr);
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
    // Each cycle owns this: set for a directed one, cleared for an undirected
    // one. It used to be set where directed advertising began and cleared only
    // by a connect or by its own 2 s timeout, so switching host again inside
    // those two seconds carried it into the next slot's undirected cycle — and
    // loop()'s timeout then stopped and restarted the advertising that slot's
    // host was already answering. A phone that has its advertising pulled away
    // mid-reconnect waits to be told to connect by hand.
    s_directed_adv_active = s_directed_adv_pending;
    s_adv_attempt_ms = millis();
    // A slot can be marked as never advertising — a remote page whose keys drive
    // Home Assistant rather than a connected host. Gated here rather than at each
    // caller: advertising starts from four places (services up at boot, after a
    // disconnect, a host switch, and the directed-advertising timeout), and a
    // fifth added later is covered by being here.
    if (s_instance != nullptr && !s_instance->slot_broadcasts(s_instance->active_host_slot())) {
        ESP_LOGD(TAG, "ADV: slot %u does not advertise; staying quiet",
                 s_instance->active_host_slot());
        return;
    }
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
                s_adv_running = true;
                ESP_LOGI(TAG, "GAP: Advertising started");
            } else {
                // Left false on purpose: loop()'s watchdog is what tries again.
                s_adv_running = false;
                ESP_LOGE(TAG, "GAP: Advertising start failed (%d)", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            s_adv_running = false;
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
                    s_instance->mark_link_secure();
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
                ESP_LOGE(TAG, "GAP: Pairing Failed (0x%02X)", fail_reason);

                // The bond is very probably already gone, and not because anything
                // here removed it. Bluedroid's btc_dm_ble_auth_cmpl_evt()
                // (btc_dm.c, the `default:` branch) calls
                // btc_dm_remove_ble_bonding_keys() on *every* pairing failure
                // except SMP_PAIR_NOT_SUPPORT — so a procedure that merely timed
                // out because the link went away takes the bond with it. The codes
                // that reach here that way are all offset by
                // BTA_DM_AUTH_FAIL_BASE (HCI_ERR_MAX_ERR + 10 = 0x4D):
                //   0x66 SMP_CONN_TOUT   — link dropped mid-pairing
                //   0x63 SMP_RSP_TIMEOUT — peer stopped answering
                //   0x61 SMP_ENC_FAIL, 0x65 SMP_FAIL
                // The host keeps its own copy of the key, so it does not re-pair on
                // its own and has to be paired again by hand. This component cannot
                // veto the removal, so record it — that record is the only trace
                // the event leaves behind.
                if (s_instance && fail_reason != 0x52) {
                    int8_t fslot = s_instance->find_slot_for_peer(param->ble_security.auth_cmpl.bd_addr);
                    ESP_LOGW(TAG, "GAP: The stack drops the bond after a failed pairing — "
                                  "this host must be paired again from its own side");
                    s_instance->queue_bond_log(BOND_LOSS_STACK, fail_reason,
                                               fslot >= 0 ? (uint8_t) fslot : 0xFF,
                                               param->ble_security.auth_cmpl.bd_addr);
                }

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
            s_adv_running = false;   // the controller stops advertising on connect
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
            // Ask for encryption only when this peer has no bond yet.
            //
            // On a peripheral, esp_ble_set_encryption() does not mean "encrypt with
            // the key we already have". btm_ble_set_encryption() (btm_ble.c) falls
            // through every sec_act to SMP_Pair(), which as slave sends a security
            // request and parks the device record in BTM_SEC_STATE_AUTHENTICATING —
            // it notices the peer already holds an LTK and does it anyway. That
            // opened a timeable SMP procedure on every single reconnect.
            //
            // It cost real bonds. If the link drops while that procedure is open the
            // stack completes it as SMP_CONN_TOUT, and Bluedroid deletes the bond
            // from flash on almost any pairing failure (btc_dm.c,
            // btc_dm_ble_auth_cmpl_evt). The host keeps its own copy of the key, so
            // it never re-pairs itself and has to be paired again by hand. A TV that
            // sleeps hits that window often enough to lose its bond every few days.
            //
            // Skipping it for a bonded peer is safe: every HID attribute is
            // encryption-gated (PERM_R_ENC / PERM_W_ENC / PERM_RW_ENC), so a host
            // that does not encrypt on its own is refused the moment it touches the
            // HID service and encrypts then. First-time pairing still asks, which is
            // the case that actually needs the request.
            if (s_instance != nullptr && s_instance->peer_is_bonded(param->connect.remote_bda)) {
                ESP_LOGD(TAG, "GATTS: Peer is already bonded — leaving encryption to the host");
            } else {
                esp_ble_sec_act_t sec_act = s_require_mitm ? ESP_BLE_SEC_ENCRYPT_MITM : ESP_BLE_SEC_ENCRYPT_NO_MITM;
                esp_ble_set_encryption(param->connect.remote_bda, sec_act);
            }
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            ESP_LOGI(TAG, "GATTS: Disconnected");
            uint8_t dc_reason = param->disconnect.reason;
            {
                char peer_str[18] = "??:??:??:??:??:??";
                if (s_instance) format_bd_addr(s_instance->peer_addr_, peer_str);
                ESP_LOGD(TAG, "GATTS: Disconnect reason 0x%02X (peer %s)", dc_reason, peer_str);
            }

            // An encryption-related disconnect from a host we know may mean its bond
            // has gone stale — the peer no longer holds a key that matches ours.
            //
            // Only 0x06 (PIN or Key Missing) actually says that. 0x05 (Auth Failure)
            // is what a peer sends when encryption could not complete for any reason,
            // including a security-procedure collision, and 0x3D (MIC Failure) is a
            // radio-layer symptom — one corrupt-but-CRC-valid packet, or Wi-Fi/BLE
            // coexistence pressure, on a device that also serves a web page. Treating
            // either as proof of a stale key throws away a working bond that would
            // have re-encrypted fine on the next attempt, and the host then has to be
            // paired again by hand. So the bond is kept and only the log says so.
            //
            // Deliberately not written to the bond log: a host in a bad spot can drop
            // this way every few seconds, and each record is an NVS write. That is an
            // unbounded write path driven by a peer's behaviour rather than the user's,
            // and it would churn the eight-slot ring until the record worth keeping
            // scrolled out of it.
            if (s_instance && (dc_reason == 0x05 || dc_reason == 0x06 || dc_reason == 0x3D)) {
                bool is_zero = true;
                for (int i = 0; i < 6; i++) {
                    if (s_instance->peer_addr_[i] != 0) { is_zero = false; break; }
                }
                // Matched through find_slot_for_peer rather than a raw compare, so a
                // host that came back on a rotated address cannot aim this at the
                // wrong slot.
                int8_t hit = is_zero ? -1 : s_instance->find_slot_for_peer(s_instance->peer_addr_);
                if (hit >= 0) {
                    uint8_t i = (uint8_t) hit;
                    if (dc_reason == 0x06) {
                        ESP_LOGW(TAG, "GATTS: Host slot %u reports our key is missing "
                                      "(reason 0x06) — removing the stale bond", i);
                        esp_ble_remove_bond_device(s_instance->peer_addr_);
                        s_instance->queue_bond_log(BOND_LOSS_DISCONNECT, dc_reason, i,
                                                   s_instance->peer_addr_);
                    } else {
                        ESP_LOGW(TAG, "GATTS: Encryption-related disconnect (reason 0x%02X) from "
                                      "host slot %u — keeping the bond; it should re-encrypt on "
                                      "the next connection", dc_reason, i);
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
                // Nothing may act on this address until the next connect fills it
                // in. Left standing, it would let a disconnect that arrives without
                // a fresh connect point bond removal at whoever was here last.
                // Safe to clear: both readers (publish_host_mac_,
                // remember_host_identity_) return early once disconnected.
                memset(s_instance->peer_addr_, 0, sizeof(esp_bd_addr_t));
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
            if (param->write.handle == bas_handle_table[BAS_IDX_BAT_CCC] && param->write.len >= 2) {
                battery_ccc_val = static_cast<uint16_t>(param->write.value[0]) |
                                  (static_cast<uint16_t>(param->write.value[1]) << 8);
                ESP_LOGI(TAG, "GATTS: Battery CCC=0x%04X", battery_ccc_val);
                // A fresh subscriber holds only whatever the attribute read gave
                // it, which for a host that connected before the first reading is
                // the startup default. Push the current level now instead of
                // waiting for it to change.
                if ((battery_ccc_val & 0x0001) != 0 && s_instance) s_instance->queue_battery_notify();
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

bool EspidfBleKeyboard::peer_is_bonded(const esp_bd_addr_t addr) const {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;
    std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
    int query_num = dev_num;
    if (esp_ble_get_bond_device_list(&query_num, bonded.data()) != ESP_OK) return false;
    for (int i = 0; i < query_num; i++) {
        const auto &dev = bonded[static_cast<size_t>(i)];
        // The connection address, and the identity the record was filed under —
        // a host that rotates its address is bonded under the latter, so a peer
        // arriving on a fresh RPA matches only the second test.
        if (memcmp(dev.bd_addr, addr, sizeof(esp_bd_addr_t)) == 0) return true;
        if ((dev.bond_key.key_mask & ESP_BLE_ID_KEY_MASK) != 0 &&
            memcmp(dev.bond_key.pid_key.static_addr, addr, sizeof(esp_bd_addr_t)) == 0)
            return true;
    }
    return false;
}

bool EspidfBleKeyboard::host_slot_bonded(uint8_t slot) const {
    if (slot >= MAX_HOST_SLOTS || !hosts_[slot].occupied) return false;
    // The address the slot stores is only what the host last connected with, and
    // a phone rotates that every ~15 minutes while the stack keeps filing the bond
    // under the identity address. Checking `addr` alone reported every phone here
    // as unbonded — two healthy hosts were flagged by the boot census before this
    // was fixed. So try the identity too, the same order find_slot_for_peer uses.
    if (peer_is_bonded(hosts_[slot].addr)) return true;
    if (hosts_[slot].has_identity && peer_is_bonded(hosts_[slot].identity)) return true;
    esp_bd_addr_t identity;
    if (peer_identity_addr(hosts_[slot].addr, identity) && peer_is_bonded(identity)) return true;
    return false;
}

void format_bd_addr(const esp_bd_addr_t addr, char out[18]) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

// ── Bond-loss recorder ───────────────────────────────────────────────────────
// A bond can vanish days before anyone tries the host again, by which time the
// log that would have explained it is gone. NVS is the only place a reason
// survives that gap, so every removal writes one record here.

void EspidfBleKeyboard::bond_log_init_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    size_t len = sizeof(bond_log_);
    // A blob of a different size came from a different build of this struct.
    // Start clean rather than reinterpret bytes that no longer mean what they did.
    if (nvs_get_blob(handle, "bondlog", &bond_log_, &len) != ESP_OK || len != sizeof(bond_log_))
        bond_log_ = BondLogBlob{};

    bond_log_.boot_seq++;
    nvs_set_blob(handle, "bondlog", &bond_log_, sizeof(bond_log_));
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Bond log: boot #%lu, %u past event(s) on record",
             (unsigned long) bond_log_.boot_seq, (unsigned) bond_log_.count);
}

void EspidfBleKeyboard::bond_log_save_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_blob(handle, "bondlog", &bond_log_, sizeof(bond_log_));
    nvs_commit(handle);
    nvs_close(handle);
}

void EspidfBleKeyboard::bond_log_record(uint8_t cause, uint8_t reason, uint8_t slot,
                                        const esp_bd_addr_t addr) {
    esp_bd_addr_t peer{};
    if (addr != nullptr) memcpy(peer, addr, sizeof(esp_bd_addr_t));
    char peer_str[18];
    format_bd_addr(peer, peer_str);

    // One record per cause per peer per boot. Several of these are driven by what
    // a peer does rather than by the user — a device that keeps failing to pair,
    // or one being turned away from a taken slot, retries as fast as it likes —
    // and every record is an NVS write. Without this, such a device could write
    // flash in a loop and push the record worth keeping out of the ring. The
    // first occurrence is the one that carries the information; the repeats only
    // say it is still happening, which the log already does.
    if (bond_log_.count > 0) {
        uint8_t newest = (uint8_t) ((bond_log_.head + BOND_LOG_SLOTS - 1) % BOND_LOG_SLOTS);
        const BondLossRecord &p = bond_log_.rec[newest];
        if (p.cause == cause && p.boot_seq == bond_log_.boot_seq &&
            memcmp(p.addr, peer, sizeof(esp_bd_addr_t)) == 0) {
            ESP_LOGW(TAG, "Bond log: cause=%u reason=0x%02X addr=%s again this boot — not re-recorded",
                     (unsigned) cause, (unsigned) reason, peer_str);
            return;
        }
    }

    BondLossRecord &r = bond_log_.rec[bond_log_.head];
    r.cause = cause;
    r.reason = reason;
    r.slot = slot;
    memcpy(r.addr, peer, sizeof(esp_bd_addr_t));
    r.uptime_s = millis() / 1000;
    r.boot_seq = bond_log_.boot_seq;

    bond_log_.head = (uint8_t) ((bond_log_.head + 1) % BOND_LOG_SLOTS);
    if (bond_log_.count < BOND_LOG_SLOTS) bond_log_.count++;
    bond_log_save_();

    ESP_LOGW(TAG, "Bond log: cause=%u reason=0x%02X slot=%u addr=%s (boot #%lu, +%lus)",
             (unsigned) cause, (unsigned) reason, (unsigned) slot, peer_str,
             (unsigned long) r.boot_seq, (unsigned long) r.uptime_s);
}

std::string EspidfBleKeyboard::bond_log_json() const {
    std::string json = "{\"boot\":";
    json += std::to_string(bond_log_.boot_seq);
    json += ",\"events\":[";
    // Newest first. Whoever opens this has just found a host unpaired and wants
    // the last thing that happened, not the oldest thing still remembered.
    for (uint8_t i = 0; i < bond_log_.count; i++) {
        uint8_t idx = (uint8_t) ((bond_log_.head + BOND_LOG_SLOTS - 1 - i) % BOND_LOG_SLOTS);
        const BondLossRecord &r = bond_log_.rec[idx];
        char addr_str[18];
        format_bd_addr(r.addr, addr_str);
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "%s{\"cause\":%u,\"reason\":%u,\"slot\":%d,\"addr\":\"%s\","
                 "\"uptime\":%lu,\"boot\":%lu}",
                 i ? "," : "", (unsigned) r.cause, (unsigned) r.reason,
                 r.slot == 0xFF ? -1 : (int) r.slot, addr_str,
                 (unsigned long) r.uptime_s, (unsigned long) r.boot_seq);
        json += buf;
    }
    json += "]}";
    return json;
}

void EspidfBleKeyboard::bond_census_() {
    int dev_num = esp_ble_get_bond_device_num();
    ESP_LOGI(TAG, "Bond census: the stack holds %d bond(s)", dev_num);
    if (dev_num > 0) {
        std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
        int query_num = dev_num;
        if (esp_ble_get_bond_device_list(&query_num, bonded.data()) == ESP_OK) {
            for (int i = 0; i < query_num; i++) {
                char addr_str[18];
                format_bd_addr(bonded[static_cast<size_t>(i)].bd_addr, addr_str);
                ESP_LOGI(TAG, "Bond census: bonded %s", addr_str);
            }
        }
    }

    // An occupied slot with no bond is the signature of a bond that went away
    // while nothing was watching. If this component removed it, a record from
    // that moment is already here saying why; if not, this is the only trace
    // there will ever be.
    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        if (!hosts_[slot].occupied || host_slot_bonded(slot)) continue;
        char addr_str[18];
        format_bd_addr(hosts_[slot].addr, addr_str);
        ESP_LOGW(TAG, "Bond census: host slot %u (%s) is occupied but has no bond — "
                      "that host must pair again", slot, addr_str);

        // The condition persists across reboots, so recording it every boot would
        // fill the ring with copies of itself and push the event that caused it
        // out the far end. Once per address is the whole value.
        bool already = false;
        for (uint8_t i = 0; i < bond_log_.count && !already; i++) {
            const BondLossRecord &r = bond_log_.rec[i];
            already = r.cause == BOND_LOSS_MISSING &&
                      memcmp(r.addr, hosts_[slot].addr, sizeof(esp_bd_addr_t)) == 0;
        }
        if (!already) bond_log_record(BOND_LOSS_MISSING, 0, slot, hosts_[slot].addr);
    }
}

bool EspidfBleKeyboard::peer_id_keys_(const esp_bd_addr_t addr, esp_ble_pid_keys_t &out) const {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;
    std::vector<esp_ble_bond_dev_t> bonded(static_cast<size_t>(dev_num));
    int query_num = dev_num;
    if (esp_ble_get_bond_device_list(&query_num, bonded.data()) != ESP_OK) return false;

    for (int i = 0; i < query_num; i++) {
        const auto &dev = bonded[static_cast<size_t>(i)];
        // A peer sends its ID key only if it distributed one, and everything this
        // resolves — the identity address, the IRK — lives inside that key.
        if ((dev.bond_key.key_mask & ESP_BLE_ID_KEY_MASK) == 0) continue;
        // Match either way round: callers hold the connection address at connect
        // time, but a stored slot may already have been matched to the identity.
        if (memcmp(dev.bd_addr, addr, sizeof(esp_bd_addr_t)) == 0 ||
            memcmp(dev.bond_key.pid_key.static_addr, addr, sizeof(esp_bd_addr_t)) == 0) {
            memcpy(&out, &dev.bond_key.pid_key, sizeof(esp_ble_pid_keys_t));
            return true;
        }
    }
    return false;
}

bool EspidfBleKeyboard::peer_identity_addr(const esp_bd_addr_t addr, esp_bd_addr_t &out) const {
    esp_ble_pid_keys_t keys;
    if (!peer_id_keys_(addr, keys)) return false;
    // An ID key with no address in it leaves nothing stable to report — say so
    // rather than hand back six zeroes.
    bool identity_set = false;
    for (int b = 0; b < 6; b++) {
        if (keys.static_addr[b] != 0) { identity_set = true; break; }
    }
    if (!identity_set) return false;
    memcpy(out, keys.static_addr, sizeof(esp_bd_addr_t));
    return true;
}

bool EspidfBleKeyboard::peer_irk(const esp_bd_addr_t addr, uint8_t out[16]) const {
    esp_ble_pid_keys_t keys;
    if (!peer_id_keys_(addr, keys)) return false;
    // Same reasoning as the identity address above: an all-zero key is not one, and
    // handing back sixteen zeroes would look like a real answer to whatever is
    // being fed with it.
    bool irk_set = false;
    for (int b = 0; b < 16; b++) {
        if (keys.irk[b] != 0) { irk_set = true; break; }
    }
    if (!irk_set) return false;
    // Reversed, deliberately. Bluedroid keeps the IRK in the order it arrived over
    // the air, which is least-significant byte first — see SMP_Encrypt() in
    // smp_keys.c, which does REVERSE_ARRAY_TO_STREAM on the key before handing it
    // to AES. Everything that consumes an IRK as text wants the other order:
    // ESPHome's resolve_irk() memcpy's the configured hex straight in as the AES
    // key, and Home Assistant's private_ble_device does the same. Handing over the
    // stored order produces a key that looks perfectly valid and matches nothing.
    for (int b = 0; b < 16; b++) out[b] = keys.irk[15 - b];
    return true;
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
// Stored one NVS entry per slot, value the slot's overrides serialised as
// "name=action\n" records. Names reject '=', '|', whitespace and newlines so a
// payload can never break the encoding it is stored in.
//
// The entry is a blob under "ovb<slot>". It used to be a string under
// "ovr<slot>", but NVS caps a string at 4000 bytes and a full slot of 32 is up
// to 9216 — so the old key is still read, and erased on the slot's next save.

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
    // Compile-time configuration, so it is kept either way — but saying so beats
    // finding out when the web page refuses every save.
    if (override_text() > MAX_OVERRIDE_TEXT)
        ESP_LOGW(TAG, "YAML overrides use %u characters, over the %u shared by all hosts — "
                 "overrides saved from the web page will be refused",
                 (unsigned) override_text(), (unsigned) MAX_OVERRIDE_TEXT);
}

size_t EspidfBleKeyboard::override_text() const {
    size_t n = 0;
    for (uint8_t s = 0; s < MAX_HOST_SLOTS; s++) {
        for (const auto &o : nvs_overrides_[s]) n += o.name.size() + o.action.size();
        for (const auto &o : yaml_overrides_[s]) n += o.name.size() + o.action.size();
    }
    return n;
}

const std::string *EspidfBleKeyboard::find_override_(uint8_t slot, const std::string &name) const {
    if (slot >= MAX_HOST_SLOTS) return nullptr;
    for (const auto &o : nvs_overrides_[slot])
        if (o.name == name) return &o.action;
    for (const auto &o : yaml_overrides_[slot])
        if (o.name == name) return &o.action;
    return nullptr;
}

EspidfBleKeyboard::OverrideSave EspidfBleKeyboard::set_override(uint8_t slot, const std::string &name,
                                                               const std::string &action) {
    if (slot >= MAX_HOST_SLOTS || !valid_override_name(name)) return OverrideSave::BAD;
    if (action.empty() || action.size() > 255) return OverrideSave::BAD;
    if (action.find('\n') != std::string::npos || action.find('\r') != std::string::npos)
        return OverrideSave::BAD;

    auto &list = nvs_overrides_[slot];
    for (auto &o : list) {
        if (o.name != name) continue;
        // Replacing one counts only what it adds, so an edit that shortens an
        // action is never refused.
        if (action.size() > o.action.size() &&
            override_text() + (action.size() - o.action.size()) > MAX_OVERRIDE_TEXT)
            return OverrideSave::TEXT_FULL;
        std::string was = o.action;
        o.action = action;
        if (!save_overrides_(slot)) {
            o.action = was;  // RAM must not claim what storage lost
            return OverrideSave::WRITE_FAILED;
        }
        ESP_LOGI(TAG, "Override slot %u: %s -> %s", (unsigned) slot, name.c_str(), action.c_str());
        return OverrideSave::OK;
    }
    if (list.size() >= MAX_OVERRIDES) return OverrideSave::HOST_FULL;
    if (override_text() + name.size() + action.size() > MAX_OVERRIDE_TEXT) return OverrideSave::TEXT_FULL;
    list.push_back({name, action});
    if (!save_overrides_(slot)) {
        list.pop_back();
        return OverrideSave::WRITE_FAILED;
    }
    ESP_LOGI(TAG, "Override slot %u: %s -> %s", (unsigned) slot, name.c_str(), action.c_str());
    // A new long-press key: the card learns it from the hold sensor.
    if (is_long_name(name) && slot == style_slot()) publish_hold_();
    return OverrideSave::OK;
}

bool EspidfBleKeyboard::clear_override(uint8_t slot, const std::string &name) {
    if (slot >= MAX_HOST_SLOTS) return false;
    for (size_t i = 0; i < nvs_overrides_[slot].size(); i++) {
        if (nvs_overrides_[slot][i].name == name) {
            nvs_overrides_[slot].erase(nvs_overrides_[slot].begin() + i);
            save_overrides_(slot);
            ESP_LOGI(TAG, "Cleared override slot %u: %s", (unsigned) slot, name.c_str());
            if (is_long_name(name) && slot == style_slot()) publish_hold_();
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
        // Cap: MAX_OVERRIDES records of "name=action\n" (31 + 1 + 255 + 1)
        const size_t cap = (size_t) MAX_OVERRIDES * 288 + 1;
        std::string blob;
        size_t len = 0;
        snprintf(key, sizeof(key), "ovb%u", slot);
        if (nvs_get_blob(handle, key, nullptr, &len) == ESP_OK && len > 0) {
            if (len > cap) {
                ESP_LOGW(TAG, "Override blob for slot %u is oversized (%u bytes) — ignoring",
                         (unsigned) slot, (unsigned) len);
                continue;
            }
            blob.resize(len);
            if (nvs_get_blob(handle, key, &blob[0], &len) != ESP_OK) continue;
        } else {
            // Written by firmware from before the blob, which stored a string.
            snprintf(key, sizeof(key), "ovr%u", slot);
            if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0 || len > cap) continue;
            std::vector<char> buf(len);
            if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;
            blob = buf.data();
        }

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
            ESP_LOGD(TAG, "Loaded override slot %u: %s -> %s", (unsigned) slot, name.c_str(),
                     action.c_str());
        }
        // One line a slot rather than one an override: a full set is 32 of them
        // on each of several slots, every boot.
        if (!nvs_overrides_[slot].empty())
            ESP_LOGI(TAG, "Loaded %u override(s) for slot %u", (unsigned) nvs_overrides_[slot].size(),
                     (unsigned) slot);
    }
    nvs_close(handle);
}

bool EspidfBleKeyboard::save_overrides_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return false;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return false;

    char key[12], legacy[12];
    snprintf(key, sizeof(key), "ovb%u", slot);
    snprintf(legacy, sizeof(legacy), "ovr%u", slot);

    bool ok = true;
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
        ok = nvs_set_blob(handle, key, blob.data(), blob.size()) == ESP_OK;
    }
    // Only once the blob is safely down: a failed write leaves the old string
    // for the next boot to read, rather than nothing at all.
    if (ok) nvs_erase_key(handle, legacy);
    ok = nvs_commit(handle) == ESP_OK && ok;
    nvs_close(handle);
    if (!ok) ESP_LOGW(TAG, "Could not save the overrides for slot %u", (unsigned) slot);
    return ok;
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
    if (slot == style_slot()) publish_hidden_();
    ESP_LOGI(TAG, "Hidden buttons for host %u: %u", (unsigned) slot, (unsigned) names.size());
    return true;
}

void EspidfBleKeyboard::publish_hidden_() {
    if (hidden_sensor_ == nullptr) return;
    std::string csv = hidden_csv(style_slot());
    // Home Assistant rejects state strings over 255 chars. Truncate on a name
    // boundary — a half name would be silently meaningless to the card.
    if (csv.size() > 255) {
        size_t cut = csv.rfind(',', 255);
        csv = (cut == std::string::npos) ? "" : csv.substr(0, cut);
        unsigned kept = 0;
        for (char c : csv) if (c == ',') kept++;
        if (!csv.empty()) kept++;
        ESP_LOGW(TAG, "Hidden list for host %u exceeds the 255-char sensor limit — sent %u name(s)",
                 (unsigned) style_slot(), kept);
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
    bond_log_record(BOND_LOSS_REJECT, 0, reject_slot_, reject_addr_);
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

// ── Per-host remote style + custom styles (NVS-persisted) ─────────
//
// Presentation that lives entirely in the browser: the device remembers an id
// per host and a handful of user-authored style documents, and never looks
// inside either. Keeping the firmware out of the layout is what lets a new
// built-in style ship as a page change alone, and what lets a style the page
// invents outlive a firmware that has never heard of it.

bool EspidfBleKeyboard::valid_style_id(const std::string &id) {
    if (id.empty() || id.size() > MAX_STYLE_LEN) return false;
    for (char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

const std::string &EspidfBleKeyboard::get_remote_style(uint8_t slot) const {
    static const std::string none;
    return slot < MAX_HOST_SLOTS ? remote_style_[slot] : none;
}

bool EspidfBleKeyboard::set_remote_style(uint8_t slot, const std::string &id) {
    if (slot >= MAX_HOST_SLOTS) return false;
    if (!id.empty() && !valid_style_id(id)) return false;
    remote_style_[slot] = id;
    save_remote_style_(slot);
    if (slot == style_slot()) publish_remote_style_();
    ESP_LOGI(TAG, "Remote style for host %u: %s", (unsigned) slot,
             id.empty() ? "(default)" : id.c_str());
    return true;
}

// Empty for the default style, which is what the card reads as "draw the full
// remote". No length guard needed — a style id is 15 characters at most, far
// inside the 255 that truncates the button lists.
void EspidfBleKeyboard::publish_remote_style_() {
    if (remote_style_sensor_ == nullptr) return;
    remote_style_sensor_->publish_state(remote_style_[style_slot()]);
}

// ── LCD panel values ─────────────────────────────────────────────────────────

// A reading as the panel should show it. The unit and the decimals come from
// the sensor unless the YAML overrode them, so "21.4 °C" needs no format string
// anywhere — which is the point: neither renderer knows what kind of entity is
// behind a key, and neither should have to.
static std::string format_lcd_number(float value, int8_t decimals, const std::string &unit) {
    if (std::isnan(value)) return "";
    int d = decimals < 0 ? 1 : (decimals > 4 ? 4 : decimals);
    // Deliberately not snprintf("%.*f"). printf's float conversion is the
    // hungriest frame in this whole chain, and the chain runs on the main task,
    // which has 3584 bytes — less than the web task it used to run on. Scaling
    // to an integer and printing that keeps the formatter out of it entirely.
    static const int32_t POW10[5] = {1, 10, 100, 1000, 10000};
    const int32_t scale = POW10[d];
    float scaled = value * (float) scale;
    // Round half away from zero, as %f does.
    int64_t fixed = (int64_t) (scaled < 0 ? scaled - 0.5f : scaled + 0.5f);
    const bool neg = fixed < 0;
    if (neg) fixed = -fixed;
    char buf[32];
    if (d == 0) {
        snprintf(buf, sizeof(buf), "%s%lld", neg ? "-" : "", (long long) fixed);
    } else {
        // The fraction is zero-padded by hand rather than with "%0*lld": the
        // runtime width is something the compiler cannot bound, so it warns the
        // output may be truncated however large the buffer is. d is 1-4 here.
        char frac[6];
        int64_t f = fixed % scale;
        for (int i = d - 1; i >= 0; i--) { frac[i] = (char) ('0' + (f % 10)); f /= 10; }
        frac[d] = 0;
        snprintf(buf, sizeof(buf), "%s%lld.%s", neg ? "-" : "",
                 (long long) (fixed / scale), frac);
    }
    std::string out = buf;
    if (!unit.empty()) {
        out += " ";
        out += unit;
    }
    return out;
}

void EspidfBleKeyboard::add_source_sensor(const std::string &key, sensor::Sensor *s,
                                       const std::string &unit, int8_t decimals) {
    if (s == nullptr || sources_.size() >= MAX_SOURCES) return;
    Source src;
    src.key = key;
    src.num = s;
    src.unit = unit;
    src.decimals = decimals;
    sources_.push_back(src);
    // Flag only — the callback can fire from any task, and publishing an API
    // state from there is not this component's to do. loop() picks it up.
    s->add_on_state_callback([this](float) { this->pending_lcd_publish_.store(true); });
}

void EspidfBleKeyboard::add_source_text_sensor(const std::string &key, text_sensor::TextSensor *s) {
    if (s == nullptr || sources_.size() >= MAX_SOURCES) return;
    Source src;
    src.key = key;
    src.txt = s;
    sources_.push_back(src);
    s->add_on_state_callback([this](std::string) { this->pending_lcd_publish_.store(true); });
}

void EspidfBleKeyboard::add_source_binary_sensor(const std::string &key, binary_sensor::BinarySensor *s) {
    if (s == nullptr || sources_.size() >= MAX_SOURCES) return;
    Source src;
    src.key = key;
    src.flag = s;
    sources_.push_back(src);
    s->add_on_state_callback([this](bool) { this->pending_lcd_publish_.store(true); });
}

bool EspidfBleKeyboard::source_bool(const std::string &key, bool &out) const {
    for (const auto &src : sources_) {
        if (src.key != key || src.flag == nullptr) continue;
        if (!src.flag->has_state()) return false;   // nothing heard yet
        out = src.flag->state;
        return true;
    }
    return false;
}

#ifdef USE_TEXT
void EspidfBleKeyboard::add_source_text(const std::string &key, text::Text *t) {
    if (t == nullptr || sources_.size() >= MAX_SOURCES) return;
    Source src;
    src.key = key;
    src.fld = t;
    sources_.push_back(src);
    t->add_on_state_callback([this](std::string) { this->pending_lcd_publish_.store(true); });
}
#endif

std::string EspidfBleKeyboard::host_label(uint8_t slot) const {
    // The switch_host button's own name, which is what the host bar and the
    // /hosts response already use — so a panel never disagrees with them.
    char want[24];
    snprintf(want, sizeof(want), "switch_host:%u", (unsigned) slot);
    for (const auto &btn : buttons_) {
        if (btn.action == want) return btn.name;
    }
    return "Host " + std::to_string((unsigned) slot + 1);
}

std::vector<std::pair<std::string, std::string>> EspidfBleKeyboard::lcd_values() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(sources_.size() + 7);

    // The built-ins first: they cost nothing to produce and need no YAML, which
    // is what lets an imported style say something useful straight away.
    out.emplace_back("@host", host_label(active_slot_));
    out.emplace_back("@slot", std::to_string((unsigned) active_slot_));
    // A slot with no radio is not disconnected — there is nothing to connect to,
    // and "Disconnected" on a deliberate IR page reads as a fault.
    out.emplace_back("@state", !slot_broadcasts(active_slot_) ? "No BLE"
                               : (is_paired_ ? "Paired" : (is_connected_ ? "Connected" : "Disconnected")));
    out.emplace_back("@layout", active_layout_id());
    out.emplace_back("@battery", std::to_string((unsigned) battery_level()) + "%");
    if (has_rssi_) out.emplace_back("@rssi", std::to_string((int) last_rssi_) + " dBm");
    // Left out entirely until something has happened, so a panel shows its own
    // dashes rather than an empty line pretending to be a reading.
    if (!last_action_.empty()) out.emplace_back("@last", last_action_);
    if (!last_spare_.empty()) out.emplace_back("@station", last_spare_);
    if (!lcd_msg_.empty()) out.emplace_back("@msg", lcd_msg_);
    {
        const HostSlot &h = hosts_[active_slot_];
        if (h.occupied) {
            char addr[18];
            format_bd_addr(h.has_identity ? h.identity : h.addr, addr);
            out.emplace_back("@mac", addr);
        }
    }

    for (const auto &src : sources_) {
        std::string value;
        if (src.num != nullptr) {
            if (src.num->has_state()) {
                // get_accuracy_decimals() is not const upstream; the pointer is
                // what this const method holds const, not the sensor behind it.
                sensor::Sensor *s = src.num;
                int8_t d = src.decimals >= 0 ? src.decimals : s->get_accuracy_decimals();
                std::string unit = src.unit.empty() ? s->get_unit_of_measurement_ref().str() : src.unit;
                value = format_lcd_number(s->state, d, unit);
            }
        } else if (src.flag != nullptr) {
            // The literal on/off a style's lit: token and the if: action test.
            if (src.flag->has_state()) value = src.flag->state ? "on" : "off";
        } else if (src.txt != nullptr) {
            if (src.txt->has_state()) value = src.txt->state;
#ifdef USE_TEXT
        } else if (src.fld != nullptr) {
            if (src.fld->has_state()) value = src.fld->state;
#endif
        }
        // A source with no state yet is left out rather than sent as empty, so
        // the panel keeps its dashes instead of going blank.
        if (!value.empty()) out.emplace_back(src.key, value);
    }
    return out;
}

// Escapes only what JSON demands. Values are entity states and user-set names,
// so a quote or a backslash is unremarkable and a control character is not
// worth a second code path — it simply doesn't travel.
static void lcd_json_escape(const std::string &in, std::string &out) {
    for (char c : in) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if ((unsigned char) c >= 0x20) out += c;
    }
}

// Which values keep their place when the whole map will not fit the 255
// characters a Home Assistant state holds. Whatever came last used to be the one
// to go, which meant a source the user declared losing out to a built-in they
// never put on a panel. Order of loss, last to go first:
//
//   0  a declared source — the reason `sources:` was written at all
//   1  @state @last @station @msg — changing state nothing else reports
//   2  @battery @rssi @layout — device facts, rarely on a panel
//   3  @host @slot @mac — the card already derives these from its /hosts poll,
//      so losing them costs nothing while it can reach the device
//
// The device's own page is unaffected either way: /status serves the full map.
static uint8_t lcd_sensor_rank(const std::string &key) {
    if (key.empty() || key[0] != '@') return 0;
    if (key == "@state" || key == "@last" || key == "@station" || key == "@msg") return 1;
    if (key == "@battery" || key == "@rssi" || key == "@layout") return 2;
    return 3;
}

// Built here, on the main loop, for whoever asks. See lcd_status_json().
// One walk of the sources produces both strings: the full object /status
// serves, and the clamped one the text sensor can carry. It used to be two
// walks a second, each with its own chain of frames.
void EspidfBleKeyboard::rebuild_lcd_status_() {
    static const size_t HA_STATE_MAX = 255;
    const auto values = lcd_values();
    // Formatted once and read twice — the two strings differ only in which
    // entries they take and in what order, not in how one is written.
    std::vector<std::string> entries;
    entries.reserve(values.size());
    std::string full = "{";
    full.reserve(240);
    for (const auto &kv : values) {
        std::string entry;
        entry.reserve(kv.first.size() + kv.second.size() + 8);
        entry += "\"";
        lcd_json_escape(kv.first, entry);
        entry += "\":\"";
        lcd_json_escape(kv.second, entry);
        entry += "\"";
        if (full.size() > 1) full += ",";
        full += entry;
        entries.push_back(std::move(entry));
    }
    full += "}";

    // Whole entries or nothing, so what the sensor carries always parses; and by
    // rank, so what is lost is what matters least. A rank that does not fit does
    // not stop a later, shorter one from getting in.
    // `lost` is what nothing else can tell the card; `derived` is @host, @slot
    // and @mac, which it reads off its own host list — losing those is the
    // design working, not a problem to report.
    std::string clamped = "{", lost, derived;
    clamped.reserve(HA_STATE_MAX + 2);
    for (uint8_t rank = 0; rank <= 3; rank++) {
        for (size_t i = 0; i < entries.size(); i++) {
            if (lcd_sensor_rank(values[i].first) != rank) continue;
            const size_t sep = clamped.size() > 1 ? 1 : 0;
            if (clamped.size() + sep + entries[i].size() + 1 > HA_STATE_MAX) {
                std::string &bucket = rank == 3 ? derived : lost;
                if (!bucket.empty()) bucket += ", ";
                bucket += values[i].first;
                continue;
            }
            if (sep) clamped += ",";
            clamped += entries[i];
        }
    }
    clamped += "}";

    // Named, and only when the set changes: this runs up to once a second, and a
    // warning repeating at that rate is what a user actually notices. Silent
    // without the sensor — nothing is being sent to Home Assistant to clamp —
    // and silent for the derived three, which cost the card nothing. Any set of
    // sources at all pushes those out, so warning about them would mean warning
    // nearly everybody about nothing.
    if (lost != last_lcd_drop_) {
        last_lcd_drop_ = lost;
        if (!lost.empty() && lcd_sensor_ != nullptr)
            ESP_LOGW(TAG, "No room in the Home Assistant state (255 chars) for: %s. Shorten those "
                          "source keys, or give the card its own entity for them.", lost.c_str());
    }
    // Braced because ESP_LOGV compiles to nothing below VERBOSE, which leaves an
    // empty if-body and a -Wempty-body warning in every normal build.
    if (!derived.empty()) {
        ESP_LOGV(TAG, "Left out of the Home Assistant state, and read from /hosts instead: %s",
                 derived.c_str());
    }
    lcd_status_json_.swap(full);
    lcd_sensor_json_.swap(clamped);
}

void EspidfBleKeyboard::publish_lcd_() {
    if (lcd_sensor_ == nullptr) return;
    if (lcd_sensor_json_ == last_lcd_json_) return;   // states stream; publish on change
    last_lcd_json_ = lcd_sensor_json_;
    lcd_sensor_->publish_state(lcd_sensor_json_);
}

void EspidfBleKeyboard::load_broadcast_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;
    uint16_t mask = 0xFFFF;
    // Absent key leaves every slot advertising, which is what this firmware did
    // before the setting existed.
    if (nvs_get_u16(handle, "bcast", &mask) == ESP_OK) broadcast_mask_ = mask;
    nvs_close(handle);
    for (uint8_t s = 0; s < MAX_HOST_SLOTS; s++)
        if (!slot_broadcasts(s)) ESP_LOGI(TAG, "Host slot %u does not advertise", s);
}

void EspidfBleKeyboard::save_broadcast_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u16(handle, "bcast", broadcast_mask_);
    nvs_commit(handle);
    nvs_close(handle);
}

bool EspidfBleKeyboard::set_slot_broadcast(uint8_t slot, bool on) {
    if (slot >= MAX_HOST_SLOTS) return false;
    const uint16_t bit = (uint16_t) (1u << slot);
    const bool was = (broadcast_mask_ & bit) != 0;
    if (was == on) return true;
    if (on) broadcast_mask_ |= bit; else broadcast_mask_ &= (uint16_t) ~bit;
    save_broadcast_();
    ESP_LOGI(TAG, "Host slot %u %s advertise", slot, on ? "will" : "will not");

    // Only the active slot's radio state is live; any other slot takes effect
    // when it is switched to.
    if (slot != active_slot_) return true;
    pending_lcd_publish_.store(true);   // @state changes with it
    if (on) {
        // Same path switch_host() takes when it is not connected.
        esp_ble_gap_stop_advertising();
        do_start_advertising();
    } else if (is_connected_) {
        // Dropping the link is the point: otherwise HID keeps reaching the host
        // that is still connected while this slot claims to have no radio. The
        // disconnect handler's restart is gated by the same flag.
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    } else {
        esp_ble_gap_stop_advertising();
    }
    return true;
}

void EspidfBleKeyboard::load_remote_style_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "rst%u", slot);
        // A stored value too long for this buffer comes back as an error and is
        // skipped, which is the right answer: the page could not have written it.
        char buf[MAX_STYLE_LEN + 1];
        size_t len = sizeof(buf);
        if (nvs_get_str(handle, key, buf, &len) != ESP_OK) continue;
        std::string id(buf);
        if (valid_style_id(id)) {
            remote_style_[slot] = id;
            ESP_LOGI(TAG, "Host %u uses remote style \"%s\"", (unsigned) slot, id.c_str());
        }
    }
    nvs_close(handle);
}

const std::string &EspidfBleKeyboard::get_on_connect(uint8_t slot) const {
    static const std::string none;
    return slot < MAX_HOST_SLOTS ? on_connect_[slot] : none;
}

// Same rules as an override's action — a line break would end the stored value
// early — and written before it is kept, so a failed write is reported rather
// than lost at the next reboot.
bool EspidfBleKeyboard::set_on_connect(uint8_t slot, const std::string &action) {
    if (slot >= MAX_HOST_SLOTS || action.size() > MAX_ACTION_LEN ||
        action.find_first_of("\r\n") != std::string::npos)
        return false;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return false;
    char key[12];
    snprintf(key, sizeof(key), "onc%u", slot);
    esp_err_t err = action.empty() ? nvs_erase_key(handle, key) : nvs_set_str(handle, key, action.c_str());
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;   // clearing one that was never set
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "On-connect action for host %u not saved: %s", (unsigned) slot, esp_err_to_name(err));
        return false;
    }
    on_connect_[slot] = action;
    ESP_LOGI(TAG, "On-connect action for host %u: %s", (unsigned) slot,
             action.empty() ? "(none)" : action.c_str());
    return true;
}

void EspidfBleKeyboard::load_on_connect_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;
    for (uint8_t slot = 0; slot < MAX_HOST_SLOTS; slot++) {
        char key[12];
        snprintf(key, sizeof(key), "onc%u", slot);
        char buf[MAX_ACTION_LEN + 1];
        size_t len = sizeof(buf);
        if (nvs_get_str(handle, key, buf, &len) == ESP_OK) on_connect_[slot] = buf;
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_remote_style_(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "rst%u", slot);
    if (remote_style_[slot].empty()) {
        nvs_erase_key(handle, key);
    } else {
        nvs_set_str(handle, key, remote_style_[slot].c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

const std::string &EspidfBleKeyboard::get_custom_template(uint8_t index) const {
    static const std::string none;
    return index < MAX_CUSTOM_TEMPLATES ? custom_templates_[index] : none;
}

bool EspidfBleKeyboard::stage_chunk_(uint16_t seq, const std::string &data, uint16_t cap,
                                     uint8_t kind) {
    // seq 0 restarts, so an upload that died halfway needs no timeout to clean
    // up after it — the next one simply overwrites what it left behind.
    if (seq == 0) {
        tpl_staging_.clear();
        tpl_next_seq_ = 0;
        staging_kind_ = kind;
    }
    if (kind != staging_kind_ || seq != tpl_next_seq_) return false;
    if (tpl_staging_.size() + data.size() > cap) return false;
    // Control characters would break the JSON this is handed back inside, and
    // the page only ever sends compacted JSON, which contains none.
    for (char c : data) {
        if ((unsigned char) c < 0x20) return false;
    }
    tpl_staging_ += data;
    tpl_next_seq_++;
    return true;
}

void EspidfBleKeyboard::clear_staging_() {
    tpl_staging_.clear();
    tpl_staging_.shrink_to_fit();  // the staging copy is dead weight until the next upload
    tpl_next_seq_ = 0;
    staging_kind_ = STAGED_NONE;
}

bool EspidfBleKeyboard::stage_template_chunk(uint16_t seq, const std::string &data) {
    return stage_chunk_(seq, data, MAX_TEMPLATE_LEN, STAGED_STYLE);
}

bool EspidfBleKeyboard::commit_template(uint8_t index) {
    if (index >= MAX_CUSTOM_TEMPLATES || staging_kind_ != STAGED_STYLE || tpl_staging_.empty())
        return false;
    custom_templates_[index] = tpl_staging_;
    clear_staging_();
    save_template_(index);
    ESP_LOGI(TAG, "Saved custom remote style %u (%u bytes)", (unsigned) index,
             (unsigned) custom_templates_[index].size());
    return true;
}

bool EspidfBleKeyboard::delete_template(uint8_t index) {
    if (index >= MAX_CUSTOM_TEMPLATES) return false;
    custom_templates_[index].clear();
    custom_templates_[index].shrink_to_fit();
    save_template_(index);
    return true;
}

void EspidfBleKeyboard::load_templates_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;

    for (uint8_t i = 0; i < MAX_CUSTOM_TEMPLATES; i++) {
        char key[12];
        snprintf(key, sizeof(key), "ctpl%u", i);

        size_t len = 0;
        if (nvs_get_str(handle, key, nullptr, &len) != ESP_OK || len == 0) continue;
        if (len > MAX_TEMPLATE_LEN + 1) {
            ESP_LOGW(TAG, "Custom remote style %u is oversized (%u bytes) — ignoring",
                     (unsigned) i, (unsigned) len);
            continue;
        }
        std::vector<char> buf(len);
        if (nvs_get_str(handle, key, buf.data(), &len) != ESP_OK) continue;
        custom_templates_[i] = buf.data();
        ESP_LOGI(TAG, "Loaded custom remote style %u (%u bytes)", (unsigned) i,
                 (unsigned) custom_templates_[i].size());
    }
    nvs_close(handle);
}

void EspidfBleKeyboard::save_template_(uint8_t index) {
    if (index >= MAX_CUSTOM_TEMPLATES) return;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return;

    char key[12];
    snprintf(key, sizeof(key), "ctpl%u", index);
    if (custom_templates_[index].empty()) {
        nvs_erase_key(handle, key);
    } else {
        nvs_set_str(handle, key, custom_templates_[index].c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Imported icons (NVS-persisted, names only in RAM) ──────────────
//
// Bodies are blobs, not strings: NVS caps a string at 4000 bytes including its
// terminator, which is under MAX_ICON_LEN, while a blob may span pages.

const std::string &EspidfBleKeyboard::get_icon_name(uint8_t index) const {
    static const std::string none;
    return index < MAX_ICONS ? icon_names_[index] : none;
}

uint16_t EspidfBleKeyboard::get_icon_size(uint8_t index) const {
    return index < MAX_ICONS ? icon_sizes_[index] : 0;
}

int EspidfBleKeyboard::find_icon_(const std::string &name) const {
    if (name.empty()) return -1;
    for (uint8_t i = 0; i < MAX_ICONS; i++) {
        if (icon_names_[i] == name) return i;
    }
    return -1;
}

bool EspidfBleKeyboard::read_icon(const std::string &name, std::string &out) const {
    int index = find_icon_(name);
    if (index < 0) return false;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return false;
    char key[12];
    snprintf(key, sizeof(key), "icn%d", index);
    size_t len = 0;
    bool ok = false;
    if (nvs_get_blob(handle, key, nullptr, &len) == ESP_OK && len > 0 && len <= MAX_ICON_LEN) {
        out.resize(len);
        ok = nvs_get_blob(handle, key, &out[0], &len) == ESP_OK;
        if (!ok) out.clear();
    }
    nvs_close(handle);
    return ok;
}

bool EspidfBleKeyboard::stage_icon_chunk(uint16_t seq, const std::string &data) {
    return stage_chunk_(seq, data, MAX_ICON_LEN, STAGED_ICON);
}

EspidfBleKeyboard::IconSave EspidfBleKeyboard::commit_icon(const std::string &name) {
    // Same rule as a style id, so a name can ride in a style's option string
    // and in a URL without either needing escaping.
    if (!valid_style_id(name)) return IconSave::BAD_NAME;
    if (staging_kind_ != STAGED_ICON || tpl_staging_.empty()) return IconSave::NOTHING_STAGED;
    int index = find_icon_(name);
    for (uint8_t i = 0; index < 0 && i < MAX_ICONS; i++) {
        if (icon_names_[i].empty()) index = i;
    }
    if (index < 0) return IconSave::FULL;

    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return IconSave::WRITE_FAILED;
    char bkey[12], nkey[12];
    snprintf(bkey, sizeof(bkey), "icn%d", index);
    snprintf(nkey, sizeof(nkey), "icnm%d", index);
    // Body before name. The name is what makes a slot count as taken at boot, so
    // a write that dies between the two leaves a free slot rather than a name
    // pointing at nothing.
    bool ok = nvs_set_blob(handle, bkey, tpl_staging_.data(), tpl_staging_.size()) == ESP_OK &&
              nvs_set_str(handle, nkey, name.c_str()) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    if (!ok) {
        clear_staging_();
        ESP_LOGW(TAG, "Could not save icon \"%s\" — NVS write failed", name.c_str());
        return IconSave::WRITE_FAILED;
    }
    icon_names_[index] = name;
    icon_sizes_[index] = (uint16_t) tpl_staging_.size();
    clear_staging_();
    ESP_LOGI(TAG, "Saved icon \"%s\" in slot %d (%u bytes)", name.c_str(), index,
             (unsigned) icon_sizes_[index]);
    return IconSave::OK;
}

bool EspidfBleKeyboard::delete_icon(const std::string &name) {
    int index = find_icon_(name);
    if (index < 0) return false;
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READWRITE, &handle) != ESP_OK) return false;
    char bkey[12], nkey[12];
    snprintf(bkey, sizeof(bkey), "icn%d", index);
    snprintf(nkey, sizeof(nkey), "icnm%d", index);
    // Name first, for the same reason commit_icon writes it last.
    nvs_erase_key(handle, nkey);
    nvs_erase_key(handle, bkey);
    nvs_commit(handle);
    nvs_close(handle);
    icon_names_[index].clear();
    icon_names_[index].shrink_to_fit();
    icon_sizes_[index] = 0;
    return true;
}

void EspidfBleKeyboard::load_icon_names_() {
    nvs_handle_t handle;
    if (nvs_open("espidf_ble_kb", NVS_READONLY, &handle) != ESP_OK) return;
    unsigned loaded = 0;
    for (uint8_t i = 0; i < MAX_ICONS; i++) {
        char bkey[12], nkey[12];
        snprintf(bkey, sizeof(bkey), "icn%u", i);
        snprintf(nkey, sizeof(nkey), "icnm%u", i);
        char name[MAX_STYLE_LEN + 1];
        size_t nlen = sizeof(name);
        if (nvs_get_str(handle, nkey, name, &nlen) != ESP_OK || !valid_style_id(name)) continue;
        // A null buffer asks for the length alone, so the body is never read.
        size_t blen = 0;
        if (nvs_get_blob(handle, bkey, nullptr, &blen) != ESP_OK || blen == 0 || blen > MAX_ICON_LEN)
            continue;
        icon_names_[i] = name;
        icon_sizes_[i] = (uint16_t) blen;
        loaded++;
    }
    nvs_close(handle);
    if (loaded) ESP_LOGI(TAG, "Found %u imported icon(s)", loaded);
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

std::string EspidfBleKeyboard::hold_csv(uint8_t slot) const {
    if (slot >= MAX_HOST_SLOTS) return "";
    std::string csv;
    for (size_t i = 0; i < hold_[slot].size(); i++) {
        if (i > 0) csv += ",";
        csv += hold_[slot][i];
    }
    // The keys with a long-press action ride along as <key>@long, after the
    // holds so the sensor's 255-character cut takes them first. A card from
    // before long presses matches no key to them and ignores them.
    for (const auto &k : long_keys(slot)) {
        if (!csv.empty()) csv += ",";
        csv += k + "@long";
    }
    return csv;
}

std::vector<std::string> EspidfBleKeyboard::long_keys(uint8_t slot) const {
    std::vector<std::string> out;
    if (slot >= MAX_HOST_SLOTS) return out;
    auto add = [&out](const std::string &name) {
        if (!is_long_name(name)) return;
        std::string key = name.substr(0, name.size() - 5);
        if (std::find(out.begin(), out.end(), key) == out.end()) out.push_back(key);
    };
    for (const auto &o : nvs_overrides_[slot]) add(o.name);
    for (const auto &o : yaml_overrides_[slot]) add(o.name);
    return out;
}

// Home Assistant rejects state strings over 255 chars. Truncating on a name
// boundary keeps every name the card does receive usable — a half name would
// just be a button that mysteriously doesn't hold.
static std::string clamp_csv_for_sensor(const char *what, unsigned slot, std::string csv) {
    if (csv.size() <= 255) return csv;
    size_t cut = csv.rfind(',', 255);
    csv = (cut == std::string::npos) ? "" : csv.substr(0, cut);
    ESP_LOGW(TAG, "%s for host %u exceeds the 255-char sensor limit — truncated", what, slot);
    return csv;
}

void EspidfBleKeyboard::publish_hold_() {
    if (hold_sensor_ == nullptr) return;
    hold_sensor_->publish_state(
        clamp_csv_for_sensor("Hold list", style_slot(), hold_csv(style_slot())));
}

void EspidfBleKeyboard::publish_repeat_() {
    if (repeat_sensor_ == nullptr) return;
    const RepeatCfg &r = repeat_[style_slot()];
    // Empty = never configured, which is the card's cue to keep its own
    // defaults — distinct from a configured-but-empty list ("<delay>,<rate>"
    // with no names), which means nothing repeats on this host.
    if (!r.set) {
        repeat_sensor_->publish_state("");
        return;
    }
    std::string csv = std::to_string(r.delay) + "," + std::to_string(r.rate);
    for (const auto &n : r.names) csv += "," + n;
    repeat_sensor_->publish_state(clamp_csv_for_sensor("Repeat list", style_slot(), csv));
}

bool EspidfBleKeyboard::set_hold(uint8_t slot, const std::vector<std::string> &names) {
    if (slot >= MAX_HOST_SLOTS || names.size() > MAX_HOLD) return false;
    for (const auto &n : names) {
        if (!valid_override_name(n) || n.find(',') != std::string::npos) return false;
    }
    if (!hold_repeat_conflict(slot, names, true).empty()) return false;
    hold_[slot] = names;
    save_hold_(slot);
    if (slot == style_slot()) publish_hold_();
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
    if (slot == style_slot()) publish_repeat_();
    ESP_LOGI(TAG, "Repeat for host %u: %u button(s), %ums then every %ums",
             (unsigned) slot, (unsigned) names.size(), (unsigned) delay, (unsigned) rate);
    return true;
}

void EspidfBleKeyboard::clear_repeat(uint8_t slot) {
    if (slot >= MAX_HOST_SLOTS) return;
    repeat_[slot] = RepeatCfg{};
    save_repeat_(slot);  // .set is false now, so this erases the key
    if (slot == style_slot()) publish_repeat_();
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
    // Erasing from the middle renumbers everything after it, so any automation
    // holding one of those indices now points at a different macro. Say so here
    // — it's the moment the drift happens, and nothing else would show it.
    if (index < macros_.size())
        ESP_LOGW(TAG, "Macros %u+ shifted down — index-based run_macro/execute_macro callers now "
                      "point at a different macro; macro:<name> references are unaffected", index);
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

// Everything a host slot lends the remote: which buttons it hides, which it
// holds, which it repeats, and the style it is drawn in. All four read
// style_slot(), so they move together or not at all.
void EspidfBleKeyboard::publish_remote_lists_() {
    publish_hidden_();
    publish_hold_();
    publish_repeat_();
    publish_remote_style_();
}

// The end of an action string that switched host: the remote catches up with
// wherever the action left the keyboard. A visit that came home has nothing to
// publish, which is the whole point of holding it.
void EspidfBleKeyboard::release_style_hold_() {
    const int8_t held = style_hold_.exchange(-1);
    if (held >= 0 && (uint8_t) held != active_slot_) publish_remote_lists_();
}

bool EspidfBleKeyboard::is_advertising() const { return s_adv_running.load(); }

void EspidfBleKeyboard::switch_host(uint8_t slot, bool from_action) {
    if (slot >= host_slots_) {
        ESP_LOGW(TAG, "Invalid host slot %u (max %u)", slot, host_slots_ - 1);
        return;
    }

    // Let go of anything held before the link to the old host drops, or it is
    // left holding the key until it notices the disconnect.
    release_held();

    // A switch made inside an action string leaves the remote drawn for the slot
    // it was showing until that whole action has run (release_style_hold_), so a
    // macro that visits another host and comes back never re-skins it on the way
    // through. Any other switch — the web page's host bar, Home Assistant's
    // service — re-skins now, and clears a hold as it goes, which is also what
    // recovers one whose action never unwound.
    if (!from_action)
        style_hold_.store(-1);
    else if (style_hold_.load() < 0)
        style_hold_.store((int8_t) active_slot_);

    // Remembered whichever way the switch was asked for — a verb, a button, the
    // web page or Home Assistant — so switch_host:back always means "where it
    // was before this".
    const bool same_slot = (slot == active_slot_);
    if (!same_slot) previous_slot_ = (int8_t) active_slot_;
    active_slot_ = slot;
    save_host_slots_();
    if (active_host_sensor_ != nullptr)
        active_host_sensor_->publish_state(slot);
    // The new host may hide, hold and repeat a different set of buttons, and
    // may draw its remote in a different style — unless the remote is being
    // held on the slot it started this action on.
    if (style_hold_.load() < 0) publish_remote_lists_();
    publish_lcd_();   // @host, @slot and @mac all just changed

    // Re-apply security params for the new slot's passkey config
    bool slot_has_pk; uint32_t slot_pk; bool slot_sc;
    get_active_slot_passkey(slot_has_pk, slot_pk, slot_sc);
    apply_security_params(slot_has_pk);

    // Apply per-slot keyboard layout (ephemeral; does not overwrite the web UI's NVS choice).
    if (!slot_layout_id_[slot].empty()) set_runtime_layout(slot_layout_id_[slot], false);

    // Apply this host's saved goto calibration (if any).
    load_goto_scale_for_host(slot);

    // A resolvable private address — bits [7:6] of its first byte are 01 — is a
    // phone's, and has rotated since it was stored, so directed advertising at
    // it would never be answered. Everything else can be invited directly.
    const bool rpa = hosts_[slot].occupied && (hosts_[slot].addr[0] >> 6) == 0x01;
    if (hosts_[slot].occupied && slot_broadcasts(slot) && !rpa) {
        s_directed_adv_pending = true;
        memcpy(s_directed_addr, hosts_[slot].addr, sizeof(esp_bd_addr_t));
        s_directed_addr_type = hosts_[slot].addr_type;
    }
    // else: empty slot, or a phone — undirected advertising, and the host comes
    // back in its own time.

    // One line saying what this slot is and what is about to be done about it:
    // when a host does not come back, whether it was ever invited and whether
    // this keyboard still holds its pairing key is the whole question.
    ESP_LOGI(TAG, "Switching to host slot %u (%s, bond %s) — %s advertising", slot,
             !hosts_[slot].occupied ? "empty" : rpa ? "phone address, rotates" : "fixed address",
             host_slot_bonded(slot) ? "yes" : "no",
             !slot_broadcasts(slot) ? "no" : s_directed_adv_pending ? "directed" : "undirected");

    if (is_connected_) {
        // Disconnect current host; DISCONNECT_EVT will trigger advertising
        esp_ble_gatts_close(s_gatts_if, conn_id_);
    } else if (same_slot && !s_directed_adv_pending && s_adv_running.load()) {
        // Already advertising for this very slot, and an undirected cycle has
        // nothing to re-aim. Switching to the host that is already selected —
        // a card tapped twice, an automation firing again — would otherwise
        // stop and restart the advertising that host may be part-way through
        // answering.
        ESP_LOGD(TAG, "ADV: already advertising for slot %u", slot);
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
    bond_log_record(BOND_LOSS_FORGET, 0, slot, hosts_[slot].addr);

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
    // Kept as well as published: the @rssi panel value needs a number even when
    // no rssi sensor is declared, and there is nowhere else it survives.
    last_rssi_ = rssi;
    has_rssi_ = true;
    pending_lcd_publish_.store(true);
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
    register_service(&EspidfBleKeyboard::on_api_run_macro_name_, "run_macro_name", {"name"});
    register_service(&EspidfBleKeyboard::on_api_send_string_, "send_string", {"keys"});
    register_service(&EspidfBleKeyboard::on_api_send_key_, "send_key", {"modifier", "keycode"});
    register_service(&EspidfBleKeyboard::on_api_send_consumer_, "send_consumer", {"code"});
    register_service(&EspidfBleKeyboard::on_api_mouse_move_, "mouse_move", {"x", "y"});
    register_service(&EspidfBleKeyboard::on_api_mouse_scroll_, "mouse_scroll", {"amount"});
    register_service(&EspidfBleKeyboard::on_api_mouse_click_, "mouse_click", {"btn"});
    register_service(&EspidfBleKeyboard::on_api_mouse_hold_, "mouse_hold", {"btn"});
    register_service(&EspidfBleKeyboard::on_api_mouse_release_, "mouse_release");
    register_service(&EspidfBleKeyboard::on_api_mouse_abs_, "mouse_abs", {"x", "y"});
    register_service(&EspidfBleKeyboard::on_api_set_battery_level_, "set_battery_level", {"level"});
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

    // This component drives Bluedroid directly rather than going through ESPHome's
    // esp32_ble, so it needs the stack to itself. Anything else that brings it up —
    // esp32_ble, esp32_ble_tracker, bluetooth_proxy — sets up at priority BLUETOOTH,
    // well ahead of this component's -200, and gets there first.
    //
    // Caught here because the failure downstream is silent, which is worse than
    // loud. The four calls below each return a status that used to be discarded, so
    // they would all fail unnoticed; then the GAP callback registered further down
    // would succeed and *replace* the other component's, Bluedroid having room for
    // one. The result was a device that compiled, booted, logged nothing wrong, ran
    // the keyboard, and quietly starved the other component of every BLE event it
    // was waiting on. Refuse the configuration instead, and name the way out.
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE ||
        esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        ESP_LOGE(TAG, "Bluetooth is already running — another component (esp32_ble, "
                      "esp32_ble_tracker or bluetooth_proxy) started it first.");
        ESP_LOGE(TAG, "This component talks to the BLE stack directly and cannot share it. "
                      "Remove that component from this device and run it on a separate ESP32.");
        mark_failed();
        return;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t bt_ret = esp_bt_controller_init(&bt_cfg);
    if (bt_ret == ESP_OK) bt_ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (bt_ret == ESP_OK) bt_ret = esp_bluedroid_init();
    if (bt_ret == ESP_OK) bt_ret = esp_bluedroid_enable();
    if (bt_ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE stack failed to start: %s", esp_err_to_name(bt_ret));
        mark_failed();
        return;
    }

    // Before anything that can remove a bond, so the wipe below can be recorded.
    bond_log_init_();
    maybe_reset_bonds_after_security_config_change();
    load_host_slots_();
    bond_census_();  // needs the slots loaded to know what should be bonded
    yaml_goto_scale_x_ = goto_scale_x_;  // snapshot YAML defaults (for Reset) before NVS override
    yaml_goto_scale_y_ = goto_scale_y_;
    load_goto_scale_for_host(active_slot_);  // per-host calibration override (if saved)
    load_macros_();
    load_overrides_();  // all slots, so the web UI can edit an inactive slot
    load_hidden_();
    load_repeat_();
    load_hold_();
    load_broadcast_();
    load_remote_style_();
    load_on_connect_();
    load_templates_();
    load_icon_names_();
    // Publish after loading, not just when the sensors are attached: the
    // set_*_sensor() calls run at registration, which is before this setup()
    // reads NVS, so their initial publish always described an empty list. Until
    // the first host switch the cards were told this host hides/holds/repeats
    // nothing, whatever Host Actions actually said.
    publish_hidden_();
    publish_repeat_();
    publish_hold_();
    publish_remote_style_();
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
    publish_remote_style_();

    // If active slot has a bonded host, use directed advertising on startup
    // Skip directed for RPA addresses (Android rotates these)
    if (hosts_[active_slot_].occupied && slot_broadcasts(active_slot_)) {
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

    // The task that runs action strings handed over by the web server. Created
    // last so nothing it might touch is still uninitialised.
    action_queue_ = xQueueCreate(ACTION_QUEUE_DEPTH, sizeof(std::string *));
    if (action_queue_ == nullptr ||
        xTaskCreate(&EspidfBleKeyboard::action_task_entry_, "ble_kb_act", ACTION_TASK_STACK,
                    this, 2, &action_task_) != pdPASS) {
        // Not fatal: queue_action() falls back to running inline, which is what
        // it did before this task existed.
        ESP_LOGW(TAG, "Could not start the action task; web actions will run on the web task");
        if (action_queue_ != nullptr) { vQueueDelete(action_queue_); action_queue_ = nullptr; }
    }
}

void EspidfBleKeyboard::update_led_state_(uint8_t led_byte) {
    if (num_lock_binary_sensor_ != nullptr)
        num_lock_binary_sensor_->publish_state(led_byte & 0x01);
    if (caps_lock_binary_sensor_ != nullptr)
        caps_lock_binary_sensor_->publish_state(led_byte & 0x02);
    if (scroll_lock_binary_sensor_ != nullptr)
        scroll_lock_binary_sensor_->publish_state(led_byte & 0x04);
}

// ── Battery Service ─────────────────────────────────────────────────────────
// The attribute is the value a host reads; the notification is what makes a
// host that subscribed update without re-reading. Both are kept in step here so
// no caller has to remember the pair.

uint8_t EspidfBleKeyboard::battery_level() const { return battery_level_val; }

void EspidfBleKeyboard::set_battery_level(uint8_t percent) {
    if (percent > 100) percent = 100;
    // A battery sensor publishes on its own interval whether or not the reading
    // moved, and every notification is a BLE transmission — so only a genuine
    // change is worth one.
    if (percent == battery_level_val) return;
    battery_level_val = percent;
    ESP_LOGD(TAG, "Battery level %u%%", (unsigned) battery_level_val);
    if (bas_handle_table[BAS_IDX_BAT_VAL] != 0)
        esp_ble_gatts_set_attr_value(bas_handle_table[BAS_IDX_BAT_VAL], 1, &battery_level_val);
    send_battery_notify_();
}

// Parameter deliberately not named `sensor`: that would shadow the namespace
// this class already uses for its own sensor members.
void EspidfBleKeyboard::set_battery_sensor(sensor::Sensor *battery) {
    if (battery == nullptr) return;
    battery->add_on_state_callback([this](float value) {
        // NAN means the sensor has no reading — reporting that as 0% would tell
        // the host the battery is flat.
        if (std::isnan(value)) return;
        if (value < 0.0f) value = 0.0f;
        if (value > 100.0f) value = 100.0f;
        this->set_battery_level(static_cast<uint8_t>(std::lroundf(value)));
    });
    // A sensor with a value already (a restored state, or one that published
    // during setup) would otherwise not reach us until its next update.
    if (battery->has_state() && !std::isnan(battery->state)) {
        float v = battery->state;
        if (v < 0.0f) v = 0.0f;
        if (v > 100.0f) v = 100.0f;
        set_battery_level(static_cast<uint8_t>(std::lroundf(v)));
    }
}

// Notify only an actual subscriber. An unsubscribed host reads the attribute
// instead, which set_battery_level has already updated.
void EspidfBleKeyboard::send_battery_notify_() {
    if (!is_connected_ || (battery_ccc_val & 0x0001) == 0) return;
    if (bas_handle_table[BAS_IDX_BAT_VAL] == 0) return;
    esp_ble_gatts_send_indicate(s_gatts_if, conn_id_, bas_handle_table[BAS_IDX_BAT_VAL],
                                1, &battery_level_val, false);
}

void EspidfBleKeyboard::loop() {
    if (pending_paired_update_.exchange(false)) {
        set_paired(pending_paired_state_.load());
    }
    if (pending_led_update_.exchange(false)) {
        update_led_state_(pending_led_value_.load());
    }
    if (pending_battery_notify_.exchange(false)) {
        send_battery_notify_();
    }
    // LCD values, coalesced. Several sources can move within the same second and
    // each publish is an API state update; nothing here is urgent — the web
    // page's own poll is 3 s — so once a second is as often as it can matter.
    if (pending_lcd_publish_.load()) {
        uint32_t now = millis();
        if (now - lcd_last_publish_ms_ >= 1000) {
            lcd_last_publish_ms_ = now;
            pending_lcd_publish_.store(false);
            rebuild_lcd_status_();
            publish_lcd_();   // no-ops without the sensor
            // What that pass actually cost this task. The main task gets 3584
            // bytes — less than the web task — and a stack overflow here is what
            // sent the device into a boot loop, so the figure is worth having
            // rather than guessing at. Logged once per boot at its lowest, and
            // loudly if it is getting close.
            {
                UBaseType_t left = uxTaskGetStackHighWaterMark(nullptr);
                if (left < lcd_stack_low_ || lcd_stack_low_ == 0) {
                    lcd_stack_low_ = left;
                    // Only when it is getting close. A healthy device says
                    // nothing; measured here it sits around 5.6 KB free.
                    if (left < 768)
                        ESP_LOGW(TAG, "Main task stack down to %u bytes after an LCD rebuild — "
                                      "raise CONFIG_ESP_MAIN_TASK_STACK_SIZE", (unsigned) left);
                }
            }
        }
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
    // Bond-loss records handed over by the BLE callbacks; each one commits to NVS,
    // which is why it happens here rather than on Bluedroid's task.
    if (pending_bond_log_count_.load() > 0) {
        uint8_t n = pending_bond_log_count_.exchange(0);
        if (n > PENDING_BOND_LOG) n = PENDING_BOND_LOG;
        for (uint8_t i = 0; i < n; i++)
            bond_log_record(pending_bond_log_[i].cause, pending_bond_log_[i].reason,
                            pending_bond_log_[i].slot, pending_bond_log_[i].addr);
    }
    check_on_connect_();

    if (is_connected_) {
        s_directed_adv_active = false;
    } else if (s_directed_adv_active) {
        if (millis() - s_directed_adv_start_ms > 2000) {
            s_directed_adv_active = false;
            ESP_LOGW(TAG, "ADV: Directed advertising timeout. Falling back to undirected...");
            esp_ble_gap_stop_advertising();
            do_start_advertising();
        }
    } else if (s_services_started >= 3 && !s_adv_running.load() && slot_broadcasts(active_slot_) &&
               millis() - s_adv_attempt_ms.load() > ADV_RETRY_MS) {
        // Meant to be connectable and not advertising: the start failed, or the
        // completion event that would have started it never arrived. Nothing
        // else tries again, and a silent keyboard looks exactly like a host that
        // has not come back — so it is said out loud and tried again.
        ESP_LOGW(TAG, "ADV: not advertising %ums after the last attempt — starting again",
                 (unsigned) (millis() - s_adv_attempt_ms.load()));
        esp_ble_gap_stop_advertising();
        do_start_advertising();
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
    TypedStroke stroke{0, 0, false};
    if (!queue_empty && !key_up) stroke = type_queue_[type_index_];
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
        // Caps Lock is read now rather than then: it is whatever the host says
        // at the moment the key goes out.
        if (send_kb_report_(caps_corrected_(stroke.modifier, stroke.letter), stroke.keycode) != ESP_OK) return;
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

// The other case of a letter, for the scripts the layouts cover (ASCII and
// Latin-1); 0 when it has none a layout could type (ß, µ, ÿ).
static uint32_t case_partner_(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 0x20;
    if (cp >= 'A' && cp <= 'Z') return cp + 0x20;
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;
    if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) return cp + 0x20;
    return 0;
}

// Whether Caps Lock changes what `cp`'s key types on this layout: it has a case
// partner, and the partner is the same key with only Shift different. That is a
// property of the key rather than of the character — German ä's key gives Ä
// with Shift, so Caps Lock reaches it, while Belgian é's gives 2 and it doesn't.
// For a dead-key compose the question is about the letter that follows the
// accent, so the whole stroke pair has to match apart from that letter's Shift.
static bool caps_affects_(const KeyboardLayout *layout, uint32_t cp, const HidKeyMapping &m) {
    const uint32_t other = case_partner_(cp);
    if (other == 0) return false;
    const HidKeyMapping o = resolve_codepoint_(layout, other);
    if (m.followup_keycode != 0x00)
        return o.modifier == m.modifier && o.keycode == m.keycode && o.followup_keycode == m.followup_keycode &&
               (o.followup_modifier ^ m.followup_modifier) == 0x02;
    return o.followup_keycode == 0x00 && o.keycode == m.keycode && (o.modifier ^ m.modifier) == 0x02;
}

// A letter typed while the host has Caps Lock on comes out in the other case on
// Windows, Android, Linux and ChromeOS, where Caps Lock inverts what Shift does
// to a letter key, so the opposite Shift gives the character that was asked
// for. macOS and iPadOS ignore Shift under Caps Lock and type capitals either
// way; there caps_lock:off before the text is the answer.
uint8_t EspidfBleKeyboard::caps_corrected_(uint8_t modifier, bool letter) const {
    return (letter && host_caps() == 1) ? (uint8_t) (modifier ^ 0x02) : modifier;
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
    std::vector<TypedStroke> strokes;
    strokes.reserve(str.size());
    size_t i = 0;
    while (i < str.size()) {
        uint32_t cp = decode_utf8_(str, i);
        HidKeyMapping m = resolve_codepoint_(active_layout_, cp);
        if (m.keycode != 0x00) {
            const bool compose = m.followup_keycode != 0x00;
            strokes.push_back({m.modifier, m.keycode, !compose && caps_affects_(active_layout_, cp, m)});
            // Dead-key compose: emit the followup stroke after the dead key.
            // Used either to emit a bare literal accent (followup = space) or
            // to compose an accented letter (followup = a/e/i/o/u with optional
            // Shift for uppercase variants like Â Ê). The letter is the stroke
            // Caps Lock acts on, not the accent.
            if (compose) {
                strokes.push_back({m.followup_modifier, m.followup_keycode, caps_affects_(active_layout_, cp, m)});
            }
        } else {
            ESP_LOGD(TAG, "send_string: skipped unmapped codepoint U+%04X", (unsigned) cp);
        }
    }

    ESP_LOGD(TAG, "send_string: \"%s\" (len=%u, strokes=%u, queue=%u, layout=%s)",
             str.c_str(), str.size(), strokes.size(), type_queue_.size(),
             active_layout_->id);

    // One character that is already held down types nothing at all: the report
    // builder drops a keycode it is already holding, so a key whose release went
    // missing swallows every later tap of itself while every other key works.
    // Lifting it first is the only way that press can be seen. Scoped to a
    // single character — the on-screen keyboard's taps — so that pasted text,
    // which can legitimately contain the letter a push-to-talk key is holding,
    // is left alone.
    if (strokes.size() == 1 && is_key_held(strokes[0].keycode)) {
        ESP_LOGD(TAG, "send_string: 0x%02X was still held — releasing so this press registers",
                 strokes[0].keycode);
        release_held();
        // The gap key_repress explains, without blocking: the queue below is
        // drained by loop(), so pushing its next step out is enough to keep the
        // lift from being batched away with the keystroke that follows.
        type_next_ms_ = millis() + 30;
    }

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
    // Same reason as in send_string: this exact key still being held means its
    // release was lost, and the report builder would drop it, so the press would
    // do nothing at all. The gap is so the lift is not batched away with the
    // press below — see key_repress.
    if (is_key_held(keycode)) {
        release_held();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
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

bool EspidfBleKeyboard::hold_char(const std::string &utf8) {
    // The web keyboard's character keys carry the character, not a keycode —
    // which key produces it depends on the layout, and that table lives here.
    // So the same resolution send_string does is done once, and the key is left
    // down instead of tapped: the host then repeats it at whatever rate its own
    // keyboard settings say, which is what makes holding a key on that page feel
    // like holding one on a real keyboard.
    if (utf8.empty()) return false;
    size_t i = 0;
    uint32_t cp = decode_utf8_(utf8, i);
    // One character, or nothing. decode_utf8_ leaves i past the first codepoint,
    // so anything left over means a body like "hello" — which has no single key
    // to leave down, and would otherwise have quietly held its first letter and
    // dropped the rest. Refused, so the caller types the whole thing instead.
    if (i != utf8.size()) return false;
    HidKeyMapping m = resolve_codepoint_(active_layout_, cp);
    if (m.keycode == 0x00) return false;   // nothing on this layout types it
    // A dead-key compose is two strokes — the accent, then the letter or a space
    // — so there is no single key to leave down. Refused rather than half-held,
    // and the caller types it once instead.
    if (m.followup_keycode != 0x00) return false;
    key_repress(caps_corrected_(m.modifier, caps_affects_(active_layout_, cp, m)), m.keycode);
    return true;
}

void EspidfBleKeyboard::key_repress(uint8_t modifiers, uint8_t keycode) {
    // Anything still down means a release that never arrived — a browser closed
    // mid-press, a second key pressed before the first came up, a request that
    // died on the way back. This is the on-screen keyboard's press, and that is
    // driven by one pointer, so exactly one of its keys is ever meant to be down:
    // whatever is left over is stale and goes.
    //
    // Not just this same keycode, which is what it used to check. A left-over
    // *different* key is worse, not better: the report carries both, so the host
    // has the old key down as well as the new one and goes on repeating it. Hold
    // an arrow to move the cursor, then press Space, and the space arrives with
    // the arrow still down underneath it.
    if (held_key_count_() > 0 || held_modifiers_ != 0) {
        // The lift needs to land as a report of its own. Sent back to back the
        // two arrive in the same batch on a host that groups input per frame,
        // the key never looks like it came up, and the press that follows does
        // nothing — which is how recovering a stuck key came to work in one
        // notes app and not another on the same phone. The same 30ms
        // send_key_combo already puts between its own down and up.
        release_held();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    key_hold(modifiers, keycode);
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
    // One-way, unlike play_pause, so a macro can pause without risking a resume.
    {"play", 0x00B0},         {"pause", 0x00B1},
    // Screens the host drives itself (phones, tablets, laptops); an external
    // monitor ignores it.
    {"brightness_up", 0x006F}, {"brightness_down", 0x0070},
    {"app_explorer", 0x0194}, {"app_browser", 0x0223},
    {"app_email", 0x018A},    {"app_calc", 0x0192},
    // Keys a TV remote has and a keyboard doesn't. Every one is a standard
    // Consumer Page usage, but that page is widely under-implemented — a host
    // that ignores one leaves the button dead, which is what per-host overrides
    // are for (same reasoning as "ok" below, which sends Enter for exactly this
    // reason). Prefer a spare over a guessed code: "input", "settings" and
    // "replay" have no standard usage and are deliberately absent.
    {"menu", 0x0040},         // Menu
    {"exit", 0x0046},         // Menu Escape
    {"captions", 0x0061},     // Closed Caption
    {"tv", 0x0089},           // Media Select TV
    {"guide", 0x008D},        // Media Select Program Guide
    {"voice", 0x00CF},        // Voice Command — the mic key
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
    // The keypad, as plain keyboard digits — what a TV wants for direct channel
    // entry and what a PC reads as typing a number. There is no consumer usage
    // for these; the keyboard row is the whole mechanism, so a host that takes
    // digits at all takes these.
    {"num1", 0, 0x1E}, {"num2", 0, 0x1F}, {"num3", 0, 0x20},
    {"num4", 0, 0x21}, {"num5", 0, 0x22}, {"num6", 0, 0x23},
    {"num7", 0, 0x24}, {"num8", 0, 0x25}, {"num9", 0, 0x26},
    {"num0", 0, 0x27},
    {"backspace", 0, 0x2A},     // fixing a digit or a TV search box
    {"caps_lock", 0, 0x39},     // a tap toggles it; caps_lock:on / :off look at the host first
};

bool EspidfBleKeyboard::execute_remote_action_(const std::string &action) {
    for (const auto &e : NAMED_CONSUMERS) {
        if (action == e.name) { send_consumer(e.usage); return true; }
    }
    for (const auto &e : NAMED_COMBOS) {
        if (action == e.name) { send_key_combo(e.modifier, e.keycode); return true; }
    }
    // Spare buttons: a name to hang a per-host override on, and nothing else.
    // They exist because a remote layout often needs a key this device has no
    // business guessing a HID code for — an app launcher, a set-top box's
    // "input", a vendor's own menu. Overrides resolve before this function, so
    // reaching here means the button is genuinely unmapped on this host. That
    // is a normal state, not a fault, hence the hint rather than a warning.
    if (is_spare_action(action)) {
        ESP_LOGI(TAG, "%s has no action on host %u — set one for it under Host Actions",
                 action.c_str(), (unsigned) active_slot_);
        return true;  // handled: swallow it, so it can't fall through to "unknown action"
    }
    return false;
}

// Cycles exactly as the Lovelace cards do: plain wrap-around over every
// configured slot, empty ones included (landing on an empty slot advertises for
// new pairing, same as switch_host:N). The >1 test guards the modulo and stops a
// one-slot config from "switching" to the slot it is already on — switch_host()
// tears the link down unconditionally, so that would drop the host for nothing.
void EspidfBleKeyboard::cycle_host_(int delta) {
    if (host_slots_ > 1)
        switch_host((uint8_t) ((active_slot_ + host_slots_ + delta) % host_slots_), true);
}

// Where the keyboard was before the last switch. Pressed again it goes back
// again, so two hosts can be toggled.
void EspidfBleKeyboard::return_to_last_host_() {
    if (previous_slot_ >= 0 && previous_slot_ < host_slots_ && previous_slot_ != (int8_t) active_slot_)
        switch_host((uint8_t) previous_slot_, true);
    else
        ESP_LOGW(TAG, "No earlier host to return to");
}

// "spare" followed by 1..MAX_SPARES. A rule rather than a table so the count is
// one constant to change — and it parses the number rather than testing a single
// digit, which stopped working the moment MAX_SPARES went past nine.
bool EspidfBleKeyboard::is_spare_action(const std::string &action) {
    if (action.size() < 6 || action.size() > 7) return false;
    if (action.compare(0, 5, "spare") != 0) return false;
    int n = 0;
    for (size_t i = 5; i < action.size(); i++) {
        if (action[i] < '0' || action[i] > '9') return false;
        n = n * 10 + (action[i] - '0');
    }
    return n >= 1 && n <= MAX_SPARES;
}

// Splits "host_action:<slot>:<name>". False for anything malformed — a slot that
// isn't all digits (so "abc" is refused rather than read as slot 0) or no name —
// which the callers report rather than guess at. The range check against the
// configured slots is theirs, since this has no instance to ask.
static bool split_host_action(const std::string &action, int &slot, std::string &name) {
    size_t sep = action.find(':', 12);
    if (sep == std::string::npos || sep == 12 || sep - 12 > 2 || sep + 1 >= action.size()) return false;
    slot = 0;
    for (size_t i = 12; i < sep; i++) {
        if (action[i] < '0' || action[i] > '9') return false;
        slot = slot * 10 + (action[i] - '0');
    }
    name = action.substr(sep + 1);
    return true;
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

    // Another host's Host Action for this key, as in execute_action(): held from
    // that slot's override when it has one, otherwise held as an ordinary key.
    if (action.rfind("host_action:", 0) == 0) {
        int slot = 0;
        std::string name;
        if (!split_host_action(action, slot, name) || slot >= host_slots_) return false;
        if (override_depth_ == 0) {
            const std::string *ovr = find_override_((uint8_t) slot, name);
            if (ovr != nullptr) {
                std::string body = *ovr;
                override_depth_++;
                bool held = hold_action(body);
                override_depth_--;
                return held;
            }
        }
        return hold_action(name);
    }

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
    // Hold a character rather than type it, resolved through the active layout —
    // which key produces it is the layout's business, and only this side knows.
    // hold_char refuses anything with no single key to leave down: a character
    // the layout cannot type, a dead-key compose (two strokes), and by way of
    // decode_utf8_ a body of more than one character. Every refusal comes back
    // here as false, and the caller types it once instead.
    if (action.rfind("string:", 0) == 0) return hold_char(action.substr(7));
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

// Split out of execute_action, and never inlined back into it. That function
// recurses up to MAX_ACTION_DEPTH deep, and a frame reserves the locals of every
// branch it contains — so a vector of branch strings declared here used to cost
// its space on every nested frame, including ones that only send a keycode.
// Measured on device: ~700 bytes a frame, 484 bytes of the web task's 4352 left
// at four deep. Each heavy branch now pays only where it is actually taken.
__attribute__((noinline)) void EspidfBleKeyboard::run_alternate_(const std::string &body) {
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
        }

// Not inlined, for the reason run_alternate_ gives.
__attribute__((noinline)) void EspidfBleKeyboard::run_if_(const std::string &action) {
        size_t colon = action.find(':', 3);
        if (colon == std::string::npos) return;
        const std::string key = action.substr(3, colon - 3);
        const std::string body = action.substr(colon + 1);

        std::vector<std::string> branches;
        size_t start = 0;
        while (true) {
            size_t sep = body.find("||", start);
            std::string branch = (sep == std::string::npos) ? body.substr(start)
                                                            : body.substr(start, sep - start);
            while (!branch.empty() && branch.front() == ' ') branch.erase(branch.begin());
            while (!branch.empty() && branch.back() == ' ') branch.pop_back();
            branches.push_back(branch);
            if (sep == std::string::npos) break;
            start = sep + 2;
        }

        bool on = false;
        if (!source_bool(key, on)) {
            // No such source, or none heard from yet. Deliberately does nothing:
            // the off-branch of a power button sends Wake-on-LAN, and guessing
            // "off" every time the device reboots before Home Assistant connects
            // would send it for no reason. Debug, not warn — this is per press.
            ESP_LOGD(TAG, "if:%s has no state yet; doing nothing", key.c_str());
            return;
        }
        const size_t want = on ? 0 : 1;
        // One branch means "when on, otherwise nothing".
        if (want < branches.size() && !branches[want].empty())
            execute_action(branches[want]);
        return;
    }

// Blocks the calling task until the active slot's host is connected and ready
// for keys, or `timeout_ms` passes. Ready means the link was made for the active
// slot and is encrypted, or the host has subscribed to reports. It is not
// is_connected_: switch_host() closes the old link asynchronously, so for a
// moment after a switch the previous host still reads as connected — and a wait
// that trusted that would let the next keys go to the wrong machine.
//
// On timeout the macro carries on, so a switch_host:back at its end still
// brings the keyboard home. Both readiness flags are written on the Bluetooth
// task, so this works on the action task and, for the YAML run_action that runs
// inline, on the loop too — where the watchdog is fed while it waits.
// Connected on the link made for the active slot, and able to take keys: either
// encrypted, or already subscribed to a report. Here rather than in the header
// because the subscription flags are this file's.
bool EspidfBleKeyboard::host_ready_() const {
    return is_connected_ && link_slot_.load() == (int8_t) active_slot_ &&
           (link_secure_.load() || ((report_ccc_val | boot_kb_in_ccc_val | consumer_ccc_val) & 0x0001));
}

// A slot's on-connect action runs once its host has been ready for 400 ms — the
// settle wait:connected gives, since keys sent before a host subscribes are
// lost. It goes through the action queue, never here: a delay: in it would
// stall the loop. Skipped while an action chain is running, because a macro
// visiting a host has its own plan for it, and for 30 s after the slot last ran
// it, so two hosts whose actions switch to each other, or one that reconnects
// its own host, stop after one round.
void EspidfBleKeyboard::check_on_connect_() {
    if (!host_ready_()) {
        ready_since_ms_ = 0;
        on_connect_checked_ = false;
        return;
    }
    if (on_connect_checked_) return;
    const uint32_t now = millis();
    if (ready_since_ms_ == 0) {
        ready_since_ms_ = now | 1;
        return;
    }
    if (now - ready_since_ms_ < 400) return;
    on_connect_checked_ = true;
    const uint8_t slot = active_slot_;
    if (on_connect_[slot].empty()) return;
    // Not the slot's own host — a stranger in the moment before it is refused.
    if (find_slot_for_peer(peer_addr_) != (int8_t) slot) return;
    if (style_hold_.load() >= 0) {
        ESP_LOGI(TAG, "Host slot %u connected during an action; its on-connect action is skipped",
                 (unsigned) slot);
        return;
    }
    if (on_connect_ran_ms_[slot] != 0 && now - on_connect_ran_ms_[slot] < 30000) {
        ESP_LOGW(TAG, "Host slot %u connected again within 30 s; its on-connect action is skipped",
                 (unsigned) slot);
        return;
    }
    on_connect_ran_ms_[slot] = now | 1;
    ESP_LOGI(TAG, "Host slot %u connected — running its on-connect action", (unsigned) slot);
    queue_action(on_connect_[slot]);
}

bool EspidfBleKeyboard::wait_host_ready_(uint32_t timeout_ms) {
    if (!slot_broadcasts(active_slot_)) return false;   // nothing will ever connect
    auto ready = [this]() { return host_ready_(); };
    if (ready()) return true;
    const bool on_loop = xTaskGetCurrentTaskHandle() != action_task_;
    const uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (on_loop) App.feed_wdt();
        if (ready()) {
            // A host subscribes to its reports straight after encryption, and
            // keys sent in that gap are dropped.
            vTaskDelay(pdMS_TO_TICKS(400));
            ESP_LOGD(TAG, "wait:connected ready after %u ms", (unsigned) (millis() - start));
            return true;
        }
    }
    ESP_LOGW(TAG, "wait:connected gave up after %u ms; carrying on with the macro", (unsigned) timeout_ms);
    return false;
}

// caps_lock:on|off|toggle. On and off tap Caps Lock only when the host reports
// it the other way, then wait for the host's LED report, so text queued next is
// corrected against the new state rather than the old one. That is the way to
// type lowercase on a Mac, where Shift can't undo Caps Lock. A host that has
// never reported its locks is left alone: a tap would be a guess.
void EspidfBleKeyboard::set_caps_lock_(const std::string &how) {
    if (how == "toggle") {
        send_key_combo(0, 0x39);
        return;
    }
    if (how != "on" && how != "off") {
        ESP_LOGW(TAG, "caps_lock: takes on, off or toggle — got '%s'", how.c_str());
        return;
    }
    const int want = how == "on" ? 1 : 0;
    const int now = host_caps();
    if (now < 0) {
        ESP_LOGW(TAG, "caps_lock:%s — the host hasn't reported its Caps Lock, so it is left alone", how.c_str());
        return;
    }
    if (now == want) return;
    send_key_combo(0, 0x39);
    const bool on_loop = xTaskGetCurrentTaskHandle() != action_task_;
    const uint32_t start = millis();
    while (millis() - start < 500) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (on_loop) App.feed_wdt();
        if (host_caps() == want) return;
    }
    ESP_LOGW(TAG, "caps_lock:%s — the host did not confirm it within 500 ms", how.c_str());
}

// Kept out of execute_action's frame, for the reason run_alternate_ gives: the
// step string here is the last of the big locals that every nested frame was
// reserving whether or not it split anything.
__attribute__((noinline)) void EspidfBleKeyboard::run_steps_(const std::string &action) {
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
}

// One line per chain, and only when it ran the stack down far enough to matter.
// Says which task, how deep it went and what was left, so the cost per frame can
// be read off rather than guessed at.
void EspidfBleKeyboard::report_action_stack_() {
    // Only a chain that came close is worth a line. Everything else is silence,
    // which is what a working device should print. The task name is in the
    // message because which task ran the chain is half the diagnosis: it ran on
    // the web server's 4352-byte task until it got its own.
    if (act_low_ <= 1200) {
        ESP_LOGW(TAG, "Action chain on '%s': %u frames deep, %u B stack left",
                 pcTaskGetName(nullptr), (unsigned) act_deepest_, (unsigned) act_low_);
    }
    act_low_ = (UBaseType_t) -1;
    act_deepest_ = 0;
}

// Actions handed over by the web server run here, not on its task. See
// queue_action() for the measurements that made this necessary.
void EspidfBleKeyboard::action_task_entry_(void *arg) {
    auto *self = static_cast<EspidfBleKeyboard *>(arg);
    std::string *job = nullptr;
#ifdef USE_BLE_KB_PEERS
    // With peers, wake twice a second even when idle: that is when the cache
    // behind the page's view of them is refreshed, and only with nothing queued.
    const TickType_t wait = self->peers_.empty() ? portMAX_DELAY : pdMS_TO_TICKS(500);
#else
    const TickType_t wait = portMAX_DELAY;
#endif
    while (true) {
        if (xQueueReceive(self->action_queue_, &job, wait) == pdTRUE && job != nullptr) {
#ifdef USE_BLE_KB_PEERS
            // A keyboard or mouse request for a peer is not an action string at
            // all, and a merged run of one key goes straight to the peer too:
            // execute_action would split its '|' and press the key here.
            if (!self->peers_.empty() && !job->empty() && (*job)[0] == PEER_JOB)
                self->run_peer_forward_(*job);
            else if (!self->peers_.empty() && self->coalesce_peer_presses_(*job))
                self->run_peer_action_(*job);
            else
#endif
            self->execute_action(*job);
            delete job;
            job = nullptr;
        }
#ifdef USE_BLE_KB_PEERS
        if (!self->peers_.empty() && uxQueueMessagesWaiting(self->action_queue_) == 0)
            self->refresh_peers_();
#endif
    }
}

void EspidfBleKeyboard::queue_macro_index_(int32_t index) {
    if (index < 0 || (size_t) index >= macros_.size()) {
        ESP_LOGW(TAG, "run_macro: no macro at index %d — deleting one shifts every later index, "
                      "which is why run_macro_name exists", (int) index);
        return;
    }
    queue_action(macros_[(size_t) index].action);
}

bool EspidfBleKeyboard::queue_action(const std::string &action) {
    // No task means setup() could not create one; running inline is what this
    // did before, and a working button on a thin stack beats no button at all.
    if (action_queue_ == nullptr) {
#ifdef USE_BLE_KB_PEERS
        if (!action.empty() && action[0] == PEER_JOB) {
            run_peer_forward_(action);
            return true;
        }
#endif
        execute_action(action);
        return true;
    }
    auto *job = new std::string(action);
    if (xQueueSend(action_queue_, &job, 0) != pdTRUE) {
        // The queue is only this deep because a chain can take seconds — a
        // backlog means someone is pressing faster than the actions run, and
        // dropping the newest says so rather than queueing minutes of them.
        delete job;
        ESP_LOGW(TAG, "Action queue full, dropped: %s", action.c_str());
        return false;
    }
    return true;
}

void EspidfBleKeyboard::execute_action(const std::string &action) {
    // Depth guard for every recursive path below — repeat:, alternate: and the
    // '|' split all re-enter here, and nesting them multiplies rather than adds.
    // Counted once at the entry instead of at each call site, so a path added
    // later is covered by construction rather than by remembering.
    //
    // This matters most for the web endpoint, which runs the whole chain inline
    // on the server task: an action string is free-form and arrives from the
    // network, so without a cap a nested one is an unauthenticated way to run
    // the stack off its end. The counter is decremented on every exit, which is
    // what the RAII guard is for — this function returns from a dozen places.
    if (action_depth_ >= MAX_ACTION_DEPTH) {
        ESP_LOGW(TAG, "Action nested more than %u deep, stopping: %s", (unsigned) MAX_ACTION_DEPTH,
                 action.c_str());
        return;
    }
    struct DepthGuard {
        EspidfBleKeyboard *kb;
        uint8_t &depth;
        DepthGuard(EspidfBleKeyboard *k, uint8_t &d) : kb(k), depth(d) { depth++; }
        // Report once the whole chain has unwound, not per level: logging from
        // inside the chain costs the very stack being measured. The remote's
        // style catches up here for the same reason it is held in the first
        // place — this is the one place that knows the chain is over, whichever
        // task ran it and however deeply the switch was nested.
        ~DepthGuard() {
            depth--;
            if (depth == 0) { kb->report_action_stack_(); kb->release_style_hold_(); }
        }
    } depth_guard(this, action_depth_);

    // How deep this chain went, and how little stack was left at the bottom.
    // Recorded rather than logged here for the reason the guard gives.
    {
        UBaseType_t left = uxTaskGetStackHighWaterMark(nullptr);
        if (left < act_low_) act_low_ = left;
        if (action_depth_ > act_deepest_) act_deepest_ = action_depth_;
    }

    // Remember the outermost press, for the @last and @station panel values.
    // Depth 1 here because the guard above has already counted this call: any
    // deeper one is a step of a macro or a chain, and recording those would
    // leave the panel showing the last leaf of a sequence rather than the key
    // that was actually pressed.
    // Container verbs are excluded: recording one verbatim would put a whole
    // conditional or a repeat count on a panel instead of the key that was hit.
    // A press sent to a linked keyboard is that keyboard's to show, not this one's.
    if (action_depth_ == 1 && action.find("lcd:") != 0 && action.find("if:") != 0 &&
        action.find("alternate:") != 0 && action.find("repeat:") != 0 && action.find("peer:") != 0) {
        // A key pressed on a tab showing another host's page arrives wrapped in
        // host_action:N:, and the panel wants the key — the style labels it — not
        // the wrapper.
        std::string pressed = action;
        int page_slot = 0;
        if (action.rfind("host_action:", 0) == 0) split_host_action(action, page_slot, pressed);
        if (last_action_ != pressed) {
            last_action_ = pressed;
            pending_lcd_publish_.store(true);
        }
        // A long press on a spare picks a station too. Kept with its @long, so
        // the panel can tell it from a tap on the same key.
        const bool spare = is_spare_action(is_long_name(pressed) ? pressed.substr(0, pressed.size() - 5) : pressed);
        if (spare && last_spare_ != pressed) {
            last_spare_ = pressed;
            pending_lcd_publish_.store(true);
        }
    }

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
    if (action.find("alternate:") == 0) { run_alternate_(action.substr(10)); return; }
    // Branch on something the device actually knows, rather than on a counter
    // that only hopes to agree with it. Same '||' grammar as alternate: above —
    // first branch when the source is on, second when off — and checked here,
    // before the '|' split, for the same reason: the split would otherwise cut a
    // branch into pieces and run its tail unconditionally.
    if (action.find("if:") == 0) { run_if_(action); return; }
#ifdef USE_BLE_KB_PEERS
    // Text typed on a linked keyboard runs to the end of the string, '|' and
    // all: splitting it would chop the text and run its tail as an action here.
    // Only this one peer form is taken before the split — `peer:x:a | peer:x:b`
    // is still two steps, as every other verb's chain is.
    if (action.rfind("peer:", 0) == 0) {
        const size_t sep = action.find(':', 5);
        if (sep != std::string::npos && action.compare(sep + 1, 7, "string:") == 0) {
            run_peer_action_(action);
            return;
        }
    }
#endif
    // Multi-step actions: split on '|' and execute each step
    if (action.find('|') != std::string::npos) { run_steps_(action); return; }
    // Delay action for multi-step macros
    if (action.find("delay:") == 0) {
        int ms = 0;
        if (sscanf(action.c_str(), "delay:%i", &ms) == 1 && ms > 0 && ms <= 10000)
            vTaskDelay(pdMS_TO_TICKS(ms));
        return;
    }
    // wait:connected[:ms] — hold the macro until the host is ready for keys,
    // rather than guessing how long a reconnect takes after a switch.
    if (action.find("wait:connected") == 0) {
        int ms = 10000;
        if (action.size() > 15 && action[14] == ':') ms = atoi(action.c_str() + 15);
        wait_host_ready_((uint32_t) std::clamp(ms, 100, 60000));
        return;
    }
    if (action.find("caps_lock:") == 0) { set_caps_lock_(action.substr(10)); return; }
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
    // Run an action on a linked keyboard. Consumed even in a build without
    // peers:, or the whole string would be typed to the host as text.
    if (action.rfind("peer:", 0) == 0) {
#ifdef USE_BLE_KB_PEERS
        run_peer_action_(action);
#else
        ESP_LOGW(TAG, "%s needs a peers: list in this keyboard's YAML", action.c_str());
#endif
        return;
    }
    // Run a key as another host's Host Action, whichever host is active. This is
    // how a tab showing a No BLE slot's page gets keys programmed on that slot
    // while the device stays on the host it is on. A key the slot leaves alone
    // runs as an ordinary press — the active host's own override, else the
    // built-in — so a volume key beside the programmed ones still reaches the TV.
    if (action.rfind("host_action:", 0) == 0) {
        int slot = 0;
        std::string name;
        if (!split_host_action(action, slot, name) || slot >= host_slots_) {
            ESP_LOGW(TAG, "host_action needs a configured slot and a name, e.g. host_action:3:spare1 — got %s",
                     action.c_str());
            return;
        }
        if (override_depth_ == 0) {
            const std::string *ovr = find_override_((uint8_t) slot, name);
            if (ovr != nullptr) {
                // Copied and depth-counted exactly as the active host's override
                // below, for the reasons given there.
                std::string body = *ovr;
                override_depth_++;
                execute_action(body);
                override_depth_--;
                return;
            }
        }
        execute_action(name);
        return;
    }
    if (action.find("switch_host:") == 0) {
        const std::string arg = action.substr(12);
        if (arg == "back") {
            return_to_last_host_();
            return;
        }
        if (arg == "next" || arg == "prev" || arg == "previous") {
            cycle_host_(arg == "next" ? 1 : -1);
            return;
        }
        int slot = 0;
        if (sscanf(action.c_str(), "switch_host:%i", &slot) == 1)
            switch_host((uint8_t) slot, true);
        return;
    }
    if (action.find("forget_host:") == 0) {
        int slot = 0;
        if (sscanf(action.c_str(), "forget_host:%i", &slot) == 1)
            forget_host((uint8_t) slot);
        return;
    }
    // Fire a Home Assistant action — the escape hatch to hardware BLE can't
    // reach, like an IR blaster's remote.send_command. Must be consumed here
    // even when API support is compiled out: falling through would type the
    // whole string to the host as text.
    if (action.find("ha_action:") == 0) {
        do_ha_action_(action.substr(10));
        return;
    }
    // Write the rest of the string onto any @msg line a remote style is showing.
    // Everything after the colon is the text, colons included, so a message can
    // read "Now playing: Netflix". Chain it with whatever the key really does —
    // consumer:0x0223 | lcd:Netflix — and the panel names where you are.
    if (action.find("lcd:") == 0) {
        std::string msg = action.substr(4);
        if (msg.size() > MAX_LCD_MSG_LEN) msg.resize(MAX_LCD_MSG_LEN);
        if (lcd_msg_ != msg) {
            lcd_msg_ = msg;
            pending_lcd_publish_.store(true);
        }
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
    // A long press on a key this host gives no second action. The name must not
    // reach the typed-text fallback below, or the host would receive it as text.
    if (is_long_name(action)) {
        ESP_LOGD(TAG, "%s: no long-press action on this host", action.c_str());
        return;
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
    // Remote keys for the same thing as switch_host:next|prev|back — bare names,
    // so a style can place them and a host can remap them.
    else if (action == "next_host")    cycle_host_(1);
    else if (action == "prev_host")    cycle_host_(-1);
    else if (action == "last_host")    return_to_last_host_();
    // Keys the web page's own remote acts on, to move a tab across linked
    // keyboards; it never sends them. Arriving from anywhere else — the Home
    // Assistant card, a macro — there is nothing for them to do here, and
    // falling through would type their names on the host.
    else if (action == "next_keyboard" || action == "prev_host_all" || action == "next_host_all")
        ESP_LOGI(TAG, "%s only does something on the web page's remote", action.c_str());
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
    // Logged, not silent: the caller is usually a lambda that discards the
    // return, so without this a macro deleted out from under an index-based
    // automation would do nothing with no trace of why.
    if (index >= macros_.size()) {
        ESP_LOGW(TAG, "No macro at index %u (%u stored) — deleting a macro shifts the later ones; "
                      "use execute_macro(\"<name>\") instead", index, (unsigned) macros_.size());
        return false;
    }
    execute_action(macros_[index].action);
    return true;
}

// The name form of the above, and the one to prefer — it is the identifier
// `macro:<name>` already uses, and it survives a delete that renumbers the rest.
bool EspidfBleKeyboard::execute_macro(const std::string &name) {
    return run_macro_by_name_(name);
}

// Backs the `macro:<name>` action, so a host override can *reference* a macro
// rather than hold a copy of its text and drift when the macro is edited.
//
// By name, not index: delete_macro erases from the middle of the vector, so an
// index would silently repoint at whatever shifted up. Names survive that, and
// a missing one fails loudly instead of running the wrong thing.
bool EspidfBleKeyboard::run_macro_by_name_(const std::string &name) {
    // Macros can now call each other, so a cycle is reachable. These run inline
    // (callers depend on the steps completing in order), so the stack is the
    // thing at risk — a depth cap is the guard, not deferral.
    if (macro_depth_ >= MAX_MACRO_DEPTH) {
        ESP_LOGW(TAG, "Macro nesting too deep at '%s' — stopping", name.c_str());
        return false;
    }
    for (const auto &m : macros_) {
        if (m.name != name) continue;
        macro_depth_++;
        execute_action(m.action);
        macro_depth_--;
        return true;
    }
    ESP_LOGW(TAG, "No macro named '%s'", name.c_str());
    return false;
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

// Backs the `ha_action:<domain>.<action>;key=value;…` action — asks Home
// Assistant to run one of its own actions over the native API, which is how a
// remote key reaches hardware the ESP can't: an IR blaster's
// remote.send_command, a script, a scene.
//
// Payload records split on ';'; the first is the action name, the rest are
// data pairs split at the FIRST '=' so values may contain '=' (base64
// padding). Keys and values are trimmed of surrounding whitespace, inner
// spaces survive. '|' never reaches here (it is the chain separator) and ';'
// cannot be escaped — documented limits, consistent with the rest of the
// action vocabulary.
void EspidfBleKeyboard::do_ha_action_(const std::string &payload) {
#if defined(USE_API) && defined(USE_API_HOMEASSISTANT_SERVICES)
    if (!ha_action_enabled_) {
        ESP_LOGW(TAG, "ha_action ignored — enable it with `ha_action: true` in YAML");
        return;
    }
    auto trim = [](const std::string &s) {
        size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos) return std::string();
        size_t e = s.find_last_not_of(" \t");
        return s.substr(b, e - b + 1);
    };
    size_t sep = payload.find(';');
    std::string service = trim(payload.substr(0, sep));
    size_t dot = service.find('.');
    if (dot == 0 || dot == std::string::npos || dot + 1 >= service.size() ||
        service.find('.', dot + 1) != std::string::npos) {
        ESP_LOGW(TAG, "ha_action '%s' is not a domain.action name", service.c_str());
        return;
    }
    std::map<std::string, std::string> data;
    while (sep != std::string::npos) {
        size_t start = sep + 1;
        sep = payload.find(';', start);
        std::string record = trim(payload.substr(
            start, sep == std::string::npos ? std::string::npos : sep - start));
        if (record.empty()) continue;
        size_t eq = record.find('=');
        std::string key = eq == std::string::npos ? std::string() : trim(record.substr(0, eq));
        if (key.empty()) {
            ESP_LOGW(TAG, "ha_action: skipping '%s' — data records are key=value", record.c_str());
            continue;
        }
        data[key] = trim(record.substr(eq + 1));
    }
    // Our own is_connected() is the BLE host, so ask the API server directly.
    // The call is not queued anywhere — an absent client just loses it.
    if (api::global_api_server == nullptr || !api::global_api_server->is_connected())
        ESP_LOGW(TAG, "ha_action: no Home Assistant client connected — the call will be lost");
    // Same rationale as press_external_button_: web handlers run on the
    // AsyncTCP task and the API send belongs on the main loop. One consequence:
    // `delay:` steps block the loop, so consecutive ha_action steps in a chain
    // coalesce at its end — space repeated calls with the action's own data
    // (num_repeats, delay_secs) instead.
    this->defer([this, service, data]() {
        ESP_LOGI(TAG, "ha_action: %s (%u data fields)", service.c_str(), (unsigned) data.size());
        this->call_homeassistant_service(service, data);
    });
#else
    (void) payload;
    ESP_LOGW(TAG, "ha_action needs the api component — set `ha_action: true` and add `api:` to the config");
#endif
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




