#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif
#ifdef USE_TEXT
#include "esphome/components/text/text.h"
#endif
#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "freertos/semphr.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
#include "esphome/components/web_server_base/web_server_base.h"
#include "web_control.h"
#endif

namespace esphome {
namespace espidf_ble_keyboard {

// Maximum number of host slots for multi-host switching
static const uint8_t MAX_HOST_SLOTS = 10;

/// Render a BLE address as AA:BB:CC:DD:EE:FF. `out` must hold 18 bytes.
void format_bd_addr(const esp_bd_addr_t addr, char out[18]);

// ── Keyboard layout abstraction ──────────────────────────────────────────────
struct HidKeyMapping {
  uint8_t modifier;  // 0x00 none, 0x02 LShift, 0x40 RAlt/AltGr, etc.
  uint8_t keycode;   // USB HID usage; 0x00 = unmapped
  uint8_t followup_keycode;  // 0x00 = none; else send {followup_modifier, followup_keycode} after main stroke
  uint8_t followup_modifier; // modifier on the second stroke (e.g. Shift for uppercase accented vowels)
};

struct UnicodeKeyMapping {
  uint32_t codepoint;
  uint8_t modifier;
  uint8_t keycode;
  uint8_t followup_keycode;  // same semantics as HidKeyMapping
  uint8_t followup_modifier;
};

struct KeyboardLayout {
  const char *id;                        // "us", "uk", ...
  const char *display_name;              // "English (US)"
  const HidKeyMapping *ascii_map;        // 128 entries
  const UnicodeKeyMapping *unicode_map;  // may be nullptr if unicode_map_len == 0
  size_t unicode_map_len;
};

const KeyboardLayout *get_layout_by_id(const char *id);
const KeyboardLayout *default_layout();
size_t layout_count();
const KeyboardLayout *layout_at(size_t i);

class EspidfBleKeyboard : public Component
#ifdef USE_API
    , public api::CustomAPIDevice
#endif
{
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return -200.0f; }
  void send_string(const std::string &str);
  void send_ctrl_alt_del();
  void send_key_combo(uint8_t modifiers, uint8_t keycode);
  void send_sleep();
  void send_shutdown();
  void send_hibernate();
  void send_consumer(uint16_t usage);
  void send_power();
  void send_media_play_pause();
  void send_media_next();
  void send_media_prev();
  void send_media_stop();
  void send_media_record();
  void send_volume_up();
  void send_volume_down();
  void send_mute();
  void send_mouse_click_start(uint8_t buttons);
  void send_mouse_click_release();
  void send_mouse_click(uint8_t buttons);
  // Press-and-hold, the keyboard/consumer counterpart of send_mouse_click_start.
  // The key stays down on the host until release_held() — that is what makes
  // push-to-talk possible; every other keyboard path here taps and releases.
  void key_hold(uint8_t modifiers, uint8_t keycode);
  void consumer_hold(uint16_t usage);
  /// Release everything currently held — keys, consumer usage and mouse buttons.
  void release_held();
  bool has_held() const { return held_modifiers_ != 0 || held_key_count_() > 0 ||
                                 held_consumer_ != 0 || held_mouse_buttons_ != 0; }
  /// Hold whatever `action` resolves to. False if it isn't something that can be
  /// held (text, macros, host switching…), leaving the caller to run it normally.
  bool hold_action(const std::string &action);
  void send_mouse_move(int8_t x, int8_t y);
  void send_mouse_scroll(int8_t wheel);
  // Absolute pointer: x/y in 0..32767 (host maps onto the screen). Tracks the
  // last commanded position for mouse_abs_save / mouse_abs_restore.
  void send_mouse_move_abs(uint16_t x, uint16_t y, uint8_t buttons = 0);
  // Go to a Windows virtual-desktop pixel (primary top-left = 0,0; can be
  // negative) by homing absolute to the origin then stepping relatively. Spans
  // all monitors. Needs pointer acceleration off + 1:1 speed for exactness.
  void send_mouse_goto(int32_t x, int32_t y);

  void set_passkey(uint32_t passkey) {
    passkey_ = passkey;
    has_passkey_ = true;
  }
  void set_passkey_secure_connections(bool enabled) { passkey_secure_connections_ = enabled; }
  bool has_passkey() const { return has_passkey_; }
  uint32_t passkey() const { return passkey_; }
  bool passkey_secure_connections() const { return passkey_secure_connections_; }

  void set_device_name(const std::string &name) { device_name_ = name; }
  const std::string &device_name() const { return device_name_; }

  void set_key_delay_ms(uint32_t ms) { key_delay_ms_ = ms; }
  uint32_t key_delay_ms() const { return key_delay_ms_; }

  // Safety net for a hold whose release never arrives (browser closed mid-press,
  // a lost on_release). 0 = off: a held key then stays down until something
  // releases it, the host disconnects, or the slot changes.
  void set_max_key_hold_ms(uint32_t ms) { max_key_hold_ms_ = ms; }
  uint32_t max_key_hold_ms() const { return max_key_hold_ms_; }

  void set_web_control(bool enabled) { web_control_enabled_ = enabled; }
  void set_api_services(bool enabled) { api_services_enabled_ = enabled; }
  void set_host_slots(uint8_t slots) { host_slots_ = slots > MAX_HOST_SLOTS ? MAX_HOST_SLOTS : slots; }

  // Keyboard layout
  void set_keyboard_layout(const std::string &id);      // YAML default
  void set_runtime_layout(const std::string &id, bool persist = true);  // web UI persists; slot-driven does not
  const KeyboardLayout *active_layout() const { return active_layout_; }
  const char *active_layout_id() const { return active_layout_ != nullptr ? active_layout_->id : "us"; }

  // Web mouse sensitivity
  void set_mouse_sensitivity(float s) { mouse_sensitivity_ = s; }
  void set_mouse_accel(float a) { mouse_accel_ = a; }
  void set_mouse_max_speed(float m) { mouse_max_speed_ = m; }
  void set_scroll_sensitivity(float s) { scroll_sensitivity_ = s; }

  // Absolute-pointer screen geometry (for pixel + multi-monitor addressing).
  // screen size = the pixel space the host maps 0..32767 onto (a single
  // resolution, or the whole virtual desktop for a spanned multi-monitor setup).
  struct MonitorRect { int32_t x, y; uint32_t width, height; bool primary; };
  void set_screen_size(uint32_t w, uint32_t h) { screen_w_ = w; screen_h_ = h; }
  void add_monitor(int32_t x, int32_t y, uint32_t w, uint32_t h, bool primary = false) {
    monitors_.push_back({x, y, w, h, primary});
  }
  // Calibration multipliers for mouse_goto's relative step (compensate the host's
  // pointer-speed / DPI scaling so a count maps 1:1 to a pixel). 1.0 = no scaling.
  // X and Y are independent because some hosts scale the axes differently.
  void set_mouse_goto_scale(float s) { goto_scale_x_ = s; goto_scale_y_ = s; }  // both
  void set_mouse_goto_scale_x(float s) { goto_scale_x_ = s; }
  void set_mouse_goto_scale_y(float s) { goto_scale_y_ = s; }
  float mouse_goto_scale_x() const { return goto_scale_x_; }
  float mouse_goto_scale_y() const { return goto_scale_y_; }
  // Last mouse_goto target (Windows virtual-desktop coords) — for the web Finder
  // to mark where the cursor was last sent, from any source.
  int32_t last_goto_x() const { return last_goto_x_; }
  int32_t last_goto_y() const { return last_goto_y_; }
  // Per-host calibration: persist/restore the goto scale per host slot in NVS, so
  // each paired host (different DPI / pointer settings) keeps its own values.
  void save_goto_scale_for_host();
  void load_goto_scale_for_host(uint8_t slot);
  // Backup/restore accessors: read or write any slot's stored calibration
  // without disturbing the live values (load_goto_scale_for_host applies what it
  // reads, and save_goto_scale_for_host only ever writes the active slot).
  bool get_saved_goto_scale(uint8_t slot, float &x, float &y) const;
  void set_saved_goto_scale(uint8_t slot, float x, float y);
  // Reset the goto scale back to the YAML-configured defaults and save to host.
  void reset_goto_scale_for_host() {
    goto_scale_x_ = yaml_goto_scale_x_;
    goto_scale_y_ = yaml_goto_scale_y_;
    save_goto_scale_for_host();
  }
  uint32_t screen_width() const { return screen_w_; }
  uint32_t screen_height() const { return screen_h_; }
  const std::vector<MonitorRect> &get_monitors() const { return monitors_; }
  // Device-space coords of the primary monitor's top-left = the Windows (0,0)
  // origin that mouse_goto homes to. Used by the web Position Finder to emit
  // mouse_goto values. Defaults to (0,0) if no monitor is marked primary.
  int32_t primary_origin_x() const {
    for (const auto &m : monitors_) if (m.primary) return m.x;
    return 0;
  }
  int32_t primary_origin_y() const {
    for (const auto &m : monitors_) if (m.primary) return m.y;
    return 0;
  }
  float mouse_sensitivity() const { return mouse_sensitivity_; }
  float mouse_accel() const { return mouse_accel_; }
  float mouse_max_speed() const { return mouse_max_speed_; }
  float scroll_sensitivity() const { return scroll_sensitivity_; }

  struct ButtonInfo {
    std::string name;
    std::string action;
  };
  void register_button(const std::string &name, const std::string &action) {
    buttons_.push_back({name, action});
  }
  const std::vector<ButtonInfo> &get_buttons() const { return buttons_; }

  // User-editable macros (NVS-persisted, web-editable)
  static const uint8_t MAX_MACROS = 16;
  const std::vector<ButtonInfo> &get_macros() const { return macros_; }
  bool add_macro(const std::string &name, const std::string &action);
  bool update_macro(uint8_t index, const std::string &name, const std::string &action);
  bool delete_macro(uint8_t index);
  /// True if `name` is usable as a macro name: no '|' (it would split a
  /// `macro:<name>` reference into steps or alternate: branches) and not
  /// already taken. `skip_index` excludes the row being edited; pass -1 when
  /// adding. Public so the web handlers can report *why* a save was refused.
  bool macro_name_available(const std::string &name, int skip_index) const;

  // Per-host action overrides: remap a named action for one host slot, so the
  // same button does the right thing on whichever host is active. Motivating
  // case: "record" sends HID Record (0x00B2), which Windows ignores entirely —
  // a Windows slot can remap it to Game Bar's Win+Alt+R while a TV slot keeps
  // the HID usage. YAML sets defaults; the web UI persists overrides to NVS.
  // Resolution order: NVS override, then YAML override, then built-in.
  static const uint8_t MAX_OVERRIDES = 8;  // per slot
  void set_host_slot_override(uint8_t slot, const std::string &name, const std::string &action);
  bool set_override(uint8_t slot, const std::string &name, const std::string &action);
  bool clear_override(uint8_t slot, const std::string &name);
  const std::vector<ButtonInfo> &get_yaml_overrides(uint8_t slot) const { return yaml_overrides_[slot]; }
  const std::vector<ButtonInfo> &get_nvs_overrides(uint8_t slot) const { return nvs_overrides_[slot]; }
  /// True if `name` is usable as an override name (no separator characters that
  /// would corrupt the NVS blob, and within the length cap).
  static bool valid_override_name(const std::string &name);

  // Per-host hidden remote buttons. Presentation only — the actions still run
  // from macros, YAML and the API; this just removes the buttons from the
  // remote for hosts that have no use for them. Stored separately from the
  // overrides above so it doesn't eat into MAX_OVERRIDES.
  static const uint8_t MAX_HIDDEN = 40;
  const std::vector<std::string> &get_hidden(uint8_t slot) const { return hidden_[slot]; }
  bool set_hidden(uint8_t slot, const std::vector<std::string> &names);  // replaces the set
  /// The active slot's hidden list as a comma-separated string, for the text sensor.
  std::string hidden_csv(uint8_t slot) const;

  // Per-host hold-to-repeat for the web remote. Like the hidden list above this
  // is presentation only: the browser does the timing and just sends the same
  // press again, so nothing here touches execute_action(). Stored per slot
  // because a TV wants a fast volume ramp while a PC may want only the D-pad.
  static const uint8_t MAX_REPEAT_BUTTONS = 40;
  // Bounds. `rate` cannot go below the 30ms dedup guard in send_consumer() /
  // send_key_combo() — a faster repeat of the same action would be silently
  // dropped there, so a too-low rate must be clamped, not honoured.
  static const uint16_t REPEAT_DELAY_MIN = 100, REPEAT_DELAY_MAX = 2000;
  static const uint16_t REPEAT_RATE_MIN = 50, REPEAT_RATE_MAX = 2000;
  struct RepeatCfg {
    /// False = this slot was never configured, so the browser falls back to the
    /// page's own `data-repeat` defaults. Distinct from a configured-but-empty
    /// list, which means "nothing repeats on this host".
    bool set{false};
    uint16_t delay{400};  // held this long before repeating starts
    uint16_t rate{180};   // and once per this long after that
    std::vector<std::string> names;
  };
  const RepeatCfg &get_repeat(uint8_t slot) const { return repeat_[slot]; }
  bool set_repeat(uint8_t slot, uint16_t delay, uint16_t rate,
                  const std::vector<std::string> &names);  // replaces the set
  void clear_repeat(uint8_t slot);  // back to the page defaults

  // Per-host hold-to-send (push-to-talk) for the web remote. Unlike the repeat
  // list above this one does reach the device: the browser sends a hold on
  // pointerdown and a release on pointerup, so the key stays down on the host
  // for exactly as long as the button is held.
  static const uint8_t MAX_HOLD = 40;
  const std::vector<std::string> &get_hold(uint8_t slot) const { return hold_[slot]; }
  bool set_hold(uint8_t slot, const std::vector<std::string> &names);  // replaces the set
  /// The button in `names` that is already in the other per-host list for this
  /// slot, or an empty string. Hold-while-held and repeat-while-held are the
  /// same gesture, so one button cannot be in both.
  std::string hold_repeat_conflict(uint8_t slot, const std::vector<std::string> &names,
                                   bool checking_hold) const;

  /// Execute an action string (combo:, consumer:, named actions, or literal text).
  /// Used by buttons, macros, web API, and YAML automations.
  void execute_action(const std::string &action);
  /// Execute a macro by index. Returns false if index is out of range.
  bool execute_macro(uint8_t index);

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  void set_web_server_base(web_server_base::WebServerBase *base) { web_server_base_ = base; }
#endif

  void set_paired_binary_sensor(binary_sensor::BinarySensor *sensor) {
    paired_binary_sensor_ = sensor;
    if (paired_binary_sensor_ != nullptr) {
      paired_binary_sensor_->publish_state(is_paired_);
    }
  }

  void set_paired(bool paired) {
    is_paired_ = paired;
    if (paired_binary_sensor_ != nullptr) {
      paired_binary_sensor_->publish_state(paired);
    }
  }
  bool is_paired() const { return is_paired_; }

  void queue_paired_state(bool paired) {
    pending_paired_state_.store(paired);
    pending_paired_update_.store(true);
  }

  void set_num_lock_binary_sensor(binary_sensor::BinarySensor *sensor) { num_lock_binary_sensor_ = sensor; }
  void set_caps_lock_binary_sensor(binary_sensor::BinarySensor *sensor) { caps_lock_binary_sensor_ = sensor; }
  void set_scroll_lock_binary_sensor(binary_sensor::BinarySensor *sensor) { scroll_lock_binary_sensor_ = sensor; }
  void queue_led_state(uint8_t led_byte) {
    pending_led_value_.store(led_byte);
    pending_led_update_.store(true);
  }

  void set_connected(bool connected, uint16_t conn_id) {
    is_connected_ = connected;
    conn_id_ = conn_id;
    // Drop held state rather than releasing it: the link is already gone, so no
    // report would reach the host anyway, and a host releases everything itself
    // when a HID device disconnects.
    if (!connected) {
      held_mouse_buttons_ = 0;
      held_modifiers_ = 0;
      memset(held_keys_, 0, sizeof(held_keys_));
      held_consumer_ = 0;
    }
  }
  bool is_connected() const { return is_connected_; }
  uint16_t conn_id() const { return conn_id_; }

  // Multi-host switching
  void switch_host(uint8_t slot);
  void forget_host(uint8_t slot);
  uint8_t active_host_slot() const { return active_slot_; }
  uint8_t host_slots() const { return host_slots_; }

  struct HostSlotConfig {
    bool has_passkey{false};
    uint32_t passkey{0};
    bool secure_connections{false};  // true = secure_connections, false = legacy
  };

  struct HostSlot {
    bool occupied{false};
    esp_bd_addr_t addr{};
    esp_ble_addr_type_t addr_type{BLE_ADDR_TYPE_PUBLIC};
    /// The host's stable identity, remembered the first time it can be resolved.
    /// `addr` is only the address it happened to connect with: a phone rotates
    /// that, and once it has, the bond table no longer holds an entry under the
    /// old value, so it can never be resolved again. Recording it while the host
    /// is connected is the only way the identity survives that rotation.
    /// Display and slot matching use this; advertising still uses `addr`.
    esp_bd_addr_t identity{};
    bool has_identity{false};
    std::string name;  // friendly label
  };
  const HostSlot &get_host_slot(uint8_t slot) const { return hosts_[slot]; }
  const uint8_t *get_slot_addr(uint8_t slot) const { return slot_addrs_[slot]; }
  void assign_host_slot_(uint8_t slot, const esp_bd_addr_t addr, esp_ble_addr_type_t addr_type);
  void save_host_slots_();
  /// True if the BLE stack still holds a bond for this slot's address. A slot can
  /// hold an address with no bond (e.g. after a restore) — it will advertise at a
  /// host that then refuses encryption, so the UI must surface the difference.
  bool host_slot_bonded(uint8_t slot) const;

  /// Resolve a peer's stable identity address. Android connects with a resolvable
  /// private address that rotates every ~15 minutes; the identity address it hands
  /// over at bonding does not. Matches `addr` against both the bonded connection
  /// address and the stored identity, so it works whichever one the caller holds.
  /// False when the peer is not bonded or sent no ID key — `out` is left alone.
  bool peer_identity_addr(const esp_bd_addr_t addr, esp_bd_addr_t &out) const;

  /// Slot holding this peer, compared by identity so a rotated address still
  /// matches, falling back to the raw address when no identity is available.
  /// -1 when no slot holds it.
  int8_t find_slot_for_peer(const esp_bd_addr_t addr) const;

  /// True when this slot's owner can be told apart from any other device. False
  /// means its stored address rotated beyond what we can resolve, so a returning
  /// owner would be indistinguishable from a stranger — such a slot must not be
  /// defended, or the owner gets locked out of it.
  bool host_slot_identifiable(uint8_t slot) const;

  void set_host_slot_passkey(uint8_t slot, uint32_t passkey, bool secure_connections) {
    if (slot < MAX_HOST_SLOTS) {
      host_slot_configs_[slot].has_passkey = true;
      host_slot_configs_[slot].passkey = passkey;
      host_slot_configs_[slot].secure_connections = secure_connections;
    }
  }
  bool get_active_slot_passkey(bool &has_passkey, uint32_t &passkey, bool &secure_connections) const;
  const HostSlotConfig &get_host_slot_config(uint8_t slot) const { return host_slot_configs_[slot]; }

  void set_host_slot_layout(uint8_t slot, const std::string &id) {
    if (slot < MAX_HOST_SLOTS) slot_layout_id_[slot] = id;
  }
  const std::string &get_host_slot_layout(uint8_t slot) const { return slot_layout_id_[slot]; }

  // Custom text entities
#ifdef USE_TEXT
  void add_custom_text(text::Text *t) { custom_texts_.push_back(t); }
  const std::vector<text::Text *> &get_custom_texts() const { return custom_texts_; }
#endif

  // Buttons defined by other ESPHome platforms (wake_on_lan, template, …) are
  // listed on the web page alongside this component's own, so the page can
  // reach things BLE can't — e.g. WOL to power a monitor back on.
  void set_expose_buttons(bool v) { expose_buttons_ = v; }
  void add_hidden_button(button::Button *b) { hidden_buttons_.push_back(b); }
  // Called by EspidfBleKeyboardButton::set_parent so the scan can tell this
  // component's own buttons apart without RTTI (ESPHome builds -fno-rtti).
  void add_own_button(button::Button *b) { own_buttons_.push_back(b); }
  // Not const: fills the list on first call. Scanning lazily rather than in
  // setup() avoids depending on cross-component registration order.
  const std::vector<ButtonInfo> &get_external_buttons();

  // Active host sensor
  void set_active_host_sensor(sensor::Sensor *sensor) { active_host_sensor_ = sensor; }

  // Hidden-buttons text sensor — lets the Home Assistant card mirror the web
  // remote's per-host hiding. Optional; without it the card shows everything.
  void set_hidden_sensor(text_sensor::TextSensor *sensor) {
    hidden_sensor_ = sensor;
    publish_hidden_();
  }

  // Connected-host address text sensor — lets an automation tell *which* host it
  // is talking to, which the active-host slot number cannot do on its own.
  void set_host_mac_sensor(text_sensor::TextSensor *sensor) {
    host_mac_sensor_ = sensor;
    publish_host_mac_();
  }
  void queue_host_mac_update() { pending_host_mac_update_.store(true); }

  // RSSI sensor
  void set_rssi_sensor(sensor::Sensor *sensor) { rssi_sensor_ = sensor; }
  void set_rssi_update_interval(uint32_t ms) { rssi_update_interval_ms_ = ms; }
  void update_rssi(int8_t rssi);
  void add_rssi_above_callback(std::function<void(int8_t)> cb) { rssi_above_callbacks_.push_back(std::move(cb)); }
  void add_rssi_below_callback(std::function<void(int8_t)> cb) { rssi_below_callbacks_.push_back(std::move(cb)); }

  /// Turn away a peer that bonded while the active slot was already taken. Called
  /// from the GAP handler; the disconnect and bond removal happen in loop().
  void queue_host_reject(const esp_bd_addr_t addr, uint8_t slot) {
    memcpy(reject_addr_, addr, sizeof(esp_bd_addr_t));
    reject_slot_ = slot;
    pending_host_reject_.store(true);
  }

  // Peer address and RSSI state — public so static GAP/GATTS handlers can access them directly
  esp_bd_addr_t peer_addr_{};
  sensor::Sensor *active_host_sensor_{nullptr};
  text_sensor::TextSensor *hidden_sensor_{nullptr};
  text_sensor::TextSensor *host_mac_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  bool rssi_pending_{false};
  std::atomic<bool> pending_rssi_nan_{false};
  std::atomic<bool> pending_rssi_update_{false};
  std::atomic<int8_t> pending_rssi_value_{0};

 protected:
#if defined(USE_API) && defined(USE_API_CUSTOM_SERVICES)
  // Auto-registered HA services (api_services: true). Names and arg names must
  // stay in sync with the HA cards (docs/*-card.js) and the README yaml snippets.
  // register_service() static-asserts without USE_API_CUSTOM_SERVICES (the python
  // side force-enables `api: custom_services: true` whenever api_services is on).
  void register_api_services_();
  // int32_t (not int) — the api component only specializes service args for
  // int32_t, and on xtensa int32_t is a distinct type from int (link error).
  void on_api_run_action_(std::string action) { execute_action(action); }
  void on_api_run_macro_(int32_t index) { execute_macro((uint8_t) index); }
  void on_api_send_string_(std::string keys) { send_string(keys); }
  void on_api_send_key_(int32_t modifier, int32_t keycode) { send_key_combo((uint8_t) modifier, (uint8_t) keycode); }
  void on_api_send_consumer_(int32_t code) { send_consumer((uint16_t) code); }
  void on_api_mouse_move_(int32_t x, int32_t y) { send_mouse_move((int8_t) x, (int8_t) y); }
  void on_api_mouse_scroll_(int32_t amount) { send_mouse_scroll((int8_t) amount); }
  void on_api_mouse_click_(int32_t btn) { send_mouse_click((uint8_t) btn); }
  void on_api_mouse_hold_(int32_t btn) { send_mouse_click_start((uint8_t) btn); }
  void on_api_mouse_release_() { send_mouse_click_release(); }
  void on_api_mouse_abs_(float x, float y) {  // percent of the mapped space (0..100)
    x = x < 0 ? 0 : (x > 100 ? 100 : x);
    y = y < 0 ? 0 : (y > 100 ? 100 : y);
    send_mouse_move_abs((uint16_t)(x / 100.0f * 32767.0f), (uint16_t)(y / 100.0f * 32767.0f));
  }
  void on_api_switch_host_(int32_t slot) { switch_host((uint8_t) slot); }
  void on_api_forget_host_(int32_t slot) { forget_host((uint8_t) slot); }
#endif
  bool api_services_enabled_{false};
  bool is_connected_{false};
  uint16_t conn_id_{0};
  bool is_paired_{false};
  std::atomic<bool> pending_paired_update_{false};
  std::atomic<bool> pending_paired_state_{false};
  // Host address publish + intruder rejection, both deferred out of the GAP
  // handler into loop() like every other state change here.
  std::atomic<bool> pending_host_mac_update_{false};
  std::atomic<bool> pending_host_reject_{false};
  esp_bd_addr_t reject_addr_{};
  uint8_t reject_slot_{0};
  void publish_host_mac_();
  void remember_host_identity_();
  void reject_host_();
  binary_sensor::BinarySensor *paired_binary_sensor_{nullptr};
  std::atomic<bool> pending_led_update_{false};
  std::atomic<uint8_t> pending_led_value_{0};
  binary_sensor::BinarySensor *num_lock_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *caps_lock_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *scroll_lock_binary_sensor_{nullptr};
  uint32_t passkey_{0};
  bool has_passkey_{false};
  bool passkey_secure_connections_{false};
  std::string device_name_{"ESP32 BLE KB"};
  uint32_t key_delay_ms_{80};

  bool web_control_enabled_{false};
  float mouse_sensitivity_{1.0f};
  float mouse_accel_{0.15f};
  float mouse_max_speed_{4.0f};
  float scroll_sensitivity_{2.0f};

  // Absolute-pointer geometry + position tracking
  uint32_t screen_w_{1920}, screen_h_{1080};   // pixel space mapped to 0..32767
  std::vector<MonitorRect> monitors_;          // optional per-monitor regions
  float goto_scale_x_{1.0f}, goto_scale_y_{1.0f};  // mouse_goto per-axis calibration
  float yaml_goto_scale_x_{1.0f}, yaml_goto_scale_y_{1.0f};  // YAML defaults (for Reset)
  int32_t last_goto_x_{0}, last_goto_y_{0};        // last mouse_goto target (Windows coords)
  uint16_t cur_abs_x_{16384}, cur_abs_y_{16384};  // last position WE set (center default)
  uint16_t saved_abs_x_{0}, saved_abs_y_{0};
  bool has_saved_abs_{false};
  std::vector<ButtonInfo> buttons_;
  std::vector<ButtonInfo> macros_;   // user-editable, NVS-persisted
  void load_macros_();
  void save_macros_();

  // Per-host action overrides. NVS wins over YAML; empty = no override.
  std::vector<ButtonInfo> yaml_overrides_[MAX_HOST_SLOTS];
  std::vector<ButtonInfo> nvs_overrides_[MAX_HOST_SLOTS];
  // Depth guard: overrides only apply at depth 0, so an override body naming
  // itself runs the built-in action instead of recursing forever.
  uint8_t override_depth_{0};
  /// Table-driven remote button actions (D-pad, Power, Channel, colour, apps).
  /// Returns false if the name isn't one of them, so the caller falls through.
  bool execute_remote_action_(const std::string &action);

  /// Backs the `press_button:<object_id>` action — presses another ESPHome
  /// button. Honours hide_buttons and refuses to nest.
  void press_external_button_(const std::string &object_id);

  /// Backs the `macro:<name>` action — runs a stored macro by name, so an
  /// override references it rather than copying its text. Depth-capped.
  void run_macro_by_name_(const std::string &name);

  const std::string *find_override_(uint8_t slot, const std::string &name) const;
  void load_overrides_();
  void save_overrides_(uint8_t slot);

  // Per-host hidden buttons (NVS key "hid<slot>", comma-separated names).
  std::vector<std::string> hidden_[MAX_HOST_SLOTS];
  void load_hidden_();
  void save_hidden_(uint8_t slot);
  void publish_hidden_();  // push the active slot's list to the text sensor

  // Per-host hold-to-repeat (NVS key "rpt<slot>", "<delay>,<rate>,name,name").
  RepeatCfg repeat_[MAX_HOST_SLOTS];
  void load_repeat_();
  void save_repeat_(uint8_t slot);

  // Per-host hold-to-send (NVS key "hld<slot>", comma-separated names).
  std::vector<std::string> hold_[MAX_HOST_SLOTS];
  void load_hold_();
  void save_hold_(uint8_t slot);

  // Multi-host state
  uint8_t host_slots_{MAX_HOST_SLOTS};
  uint8_t active_slot_{0};
  HostSlot hosts_[MAX_HOST_SLOTS];
  HostSlotConfig host_slot_configs_[MAX_HOST_SLOTS]{};
  std::string slot_layout_id_[MAX_HOST_SLOTS]{};  // YAML per-slot layout; empty = no override
  esp_bd_addr_t slot_addrs_[MAX_HOST_SLOTS]{};  // per-slot random BLE address
  void load_host_slots_();
  void generate_slot_addrs_();

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  web_server_base::WebServerBase *web_server_base_{nullptr};
  BleKeyboardWebControl *web_control_{nullptr};
#endif

  // RSSI state (interval/timing/callbacks stay protected — only touched by member functions)
  uint32_t rssi_update_interval_ms_{10000};
  uint32_t rssi_last_poll_ms_{0};
  std::vector<std::function<void(int8_t)>> rssi_above_callbacks_;
  std::vector<std::function<void(int8_t)>> rssi_below_callbacks_;

  // Keyboard layout state
  const KeyboardLayout *active_layout_{nullptr};
  std::string yaml_layout_id_{"us"};
  void load_layout_();
  void save_layout_(const std::string &id);
  void update_led_state_(uint8_t led_byte);
  void send_mouse_report_(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

  // Non-blocking string typing state machine (driven from loop())
  // Keystrokes are pre-resolved (UTF-8 decoded + layout-mapped) at enqueue time,
  // so a mid-type layout switch can't garble already-queued text.
  SemaphoreHandle_t type_mutex_{nullptr};
  std::vector<HidKeyMapping> type_queue_;
  size_t type_index_{0};
  bool type_key_up_pending_{false};
  uint32_t type_next_ms_{0};

  // Dedup guard — ESPHome API can deliver service calls twice
  uint32_t last_send_string_ms_{0};
  std::string last_send_string_;
  uint32_t last_send_key_ms_{0};
  uint16_t last_send_key_id_{0};  // (modifier << 8) | keycode
  uint32_t last_consumer_ms_{0};
  uint16_t last_consumer_usage_{0};
  uint32_t last_mouse_click_ms_{0};
  uint8_t last_mouse_click_{0};
  uint8_t held_mouse_buttons_{0};

  // Held-key state. Every keyboard report is built from this plus the transient
  // key being tapped (send_kb_report_), so typing or pressing another key while
  // something is held doesn't knock the held key back up.
  uint8_t held_modifiers_{0};
  uint8_t held_keys_[6]{};       // the boot report carries at most 6 keycodes
  uint16_t held_consumer_{0};    // only one: the consumer report has one usage field
  uint32_t hold_start_ms_{0};    // when the first of the current holds went down
  uint32_t max_key_hold_ms_{0};  // 0 = no auto-release
  uint8_t held_key_count_() const {
    uint8_t n = 0;
    for (uint8_t k : held_keys_) if (k != 0) n++;
    return n;
  }
  /// Send an 8-byte keyboard report holding everything in held_* plus one
  /// transient modifier/keycode. (0, 0) is the "key up" of a tap: it drops the
  /// transient key and leaves the held ones down. Returns what the BLE stack
  /// said, so the typing state machine can retry a full queue next loop().
  esp_err_t send_kb_report_(uint8_t extra_mod, uint8_t extra_key);
#ifdef USE_TEXT
  std::vector<text::Text *> custom_texts_;
#endif
  bool expose_buttons_{true};
  bool external_scanned_{false};
  // Which button is mid-press, so a button can't trigger itself. Null when
  // idle. A pointer rather than a flag: chaining one button to a different one
  // is legitimate, only self-reference isn't.
  button::Button *pressing_button_{nullptr};
  std::vector<button::Button *> hidden_buttons_;
  std::vector<button::Button *> own_buttons_;
  std::vector<ButtonInfo> external_buttons_;
  // Per-sequence position for `alternate:` actions, keyed on the action body.
  // RAM only — after a reboot the device can't know what it last toggled, so
  // persisting the guess would add no accuracy.
  static const size_t MAX_ALTERNATE_COUNTERS = 16;
  std::map<std::string, uint8_t> alternate_index_;
  // Nesting depth for `macro:<name>`; macros run inline, so this bounds the
  // stack when macros reference each other.
  static const uint8_t MAX_MACRO_DEPTH = 4;
  uint8_t macro_depth_{0};
};

class EspidfBleKeyboardButton : public button::Button, public Component {
 public:
  void set_parent(EspidfBleKeyboard *parent) {
    parent_ = parent;
    if (parent_ != nullptr) parent_->add_own_button(this);
  }
  void press_action() override;
  void set_action(const std::string &action) { action_ = action; }
  float get_setup_priority() const override { return -200.0f; }
 protected:
  EspidfBleKeyboard *parent_{nullptr};
  std::string action_;
};

}  // namespace espidf_ble_keyboard
}  // namespace esphome
