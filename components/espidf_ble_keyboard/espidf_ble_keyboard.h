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
#include <memory>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

// ── Bond-loss recorder ───────────────────────────────────────────────────────
// A bond can disappear days before anyone notices, and by then the log that
// would have explained it is long gone. So every removal is written to NVS with
// its cause, and read back later from /bondlog.

enum BondLossCause : uint8_t {
  BOND_LOSS_NONE = 0,
  BOND_LOSS_DISCONNECT = 1,  ///< stale-bond heuristic acted on an encryption-related disconnect
  BOND_LOSS_REJECT = 2,      ///< reject_host_(): a peer was refused because the slot was taken
  BOND_LOSS_FORGET = 3,      ///< forget_host(): deliberate, from the UI or an action
  BOND_LOSS_CONFIG = 4,      ///< passkey config changed, so every bond was cleared
  BOND_LOSS_MISSING = 5,     ///< an occupied slot had no bond at boot; nothing here removed it
  BOND_LOSS_KEPT = 6,        ///< no longer recorded — a peer could repeat it without limit.
                             ///< The value stays reserved so older records still decode.
  BOND_LOSS_STACK = 7,       ///< pairing failed and Bluedroid itself dropped the bond from flash
};

struct BondLossRecord {
  uint8_t cause{BOND_LOSS_NONE};
  uint8_t reason{0};    ///< HCI disconnect reason for DISCONNECT/KEPT, otherwise 0
  uint8_t slot{0xFF};   ///< 0xFF when the event cannot be attributed to a slot
  esp_bd_addr_t addr{};
  uint32_t uptime_s{0};  ///< seconds since boot, so same-boot events can be ordered
  uint32_t boot_seq{0};  ///< which boot this happened on
};

static const uint8_t BOND_LOG_SLOTS = 8;

/// The whole recorder, stored as one NVS blob. Small enough to keep in RAM and
/// rewrite whole, which keeps the write path free of partial-update states.
struct BondLogBlob {
  uint32_t boot_seq{0};
  uint8_t head{0};   ///< next slot to write
  uint8_t count{0};  ///< valid records, saturating at BOND_LOG_SLOTS
  BondLossRecord rec[BOND_LOG_SLOTS]{};
};

// ── Keyboard layout abstraction ──────────────────────────────────────────────
struct HidKeyMapping {
  uint8_t modifier;  // 0x00 none, 0x02 LShift, 0x40 RAlt/AltGr, etc.
  uint8_t keycode;   // USB HID usage; 0x00 = unmapped
  uint8_t followup_keycode;  // 0x00 = none; else send {followup_modifier, followup_keycode} after main stroke
  uint8_t followup_modifier; // modifier on the second stroke (e.g. Shift for uppercase accented vowels)
};

// One queued keystroke of typed text. `letter` marks a key whose character Caps
// Lock changes. That is decided from the character when the text is queued: a
// keycode alone can't say, since the key that is M in the US layout types a
// comma in the Belgian one.
struct TypedStroke {
  uint8_t modifier;
  uint8_t keycode;
  bool letter;
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
  /// key_hold, but lifting the key first if it is somehow still down — with a
  /// gap, so the host sees the lift as its own report rather than batching it
  /// away with the press that follows.
  void key_repress(uint8_t modifiers, uint8_t keycode);
  /// Hold whatever the active layout types for the first codepoint of `utf8`.
  /// False when nothing on this layout types it, or when it needs a dead-key
  /// compose — that is two strokes, so there is no single key to leave down and
  /// the caller should type it once instead.
  bool hold_char(const std::string &utf8);
  void consumer_hold(uint16_t usage);
  /// Release everything currently held — keys, consumer usage and mouse buttons.
  void release_held();
  bool has_held() const { return held_modifiers_ != 0 || held_key_count_() > 0 ||
                                 held_consumer_ != 0 || held_mouse_buttons_ != 0; }
  /// Is this exact keycode currently down? A caller that is about to press it
  /// again needs to know: holding an already-held key is a no-op, and even a
  /// *tap* of one is dropped from the report (see send_kb_report_), so a key
  /// whose release went missing is dead until something lifts it.
  bool is_key_held(uint8_t keycode) const {
    if (keycode == 0) return false;
    for (uint8_t k : held_keys_) if (k == keycode) return true;
    return false;
  }
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
  void set_ha_action(bool enabled) { ha_action_enabled_ = enabled; }
  bool ha_action_enabled() const { return ha_action_enabled_; }
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
  // 48 a slot: a No BLE slot sends no HID, so an IR page needs one on every key
  // it uses — Style 6 alone has 26 — and a tablet page can program all 32 spares
  // with room left for the keys around them. Eight was sized for remapping
  // Record. Raising it costs nothing held: the lists grow only as overrides are
  // added, and MAX_OVERRIDE_TEXT below is what bounds the memory.
  static const uint8_t MAX_OVERRIDES = 48;  // per slot
  // Overrides live in RAM, a typical ha_action: one about 130 bytes of heap, so
  // the name and action text of every override — saved and YAML, all slots — is
  // capped as well. 8000 is about a hundred typical ones, three full IR pages,
  // for roughly 13 KB: enough to use, and not enough to take the ~25 KB
  // free-heap trough down to where an allocation fails.
  static const uint16_t MAX_OVERRIDE_TEXT = 8000;
  // The longest action string a slot stores: an override's, or its on-connect action.
  static const size_t MAX_ACTION_LEN = 255;
  /// What a host slot runs each time its host connects and is ready for keys;
  /// empty for nothing. Stored in NVS, so it is set from the page, not YAML.
  const std::string &get_on_connect(uint8_t slot) const;
  bool set_on_connect(uint8_t slot, const std::string &action);
  void set_host_slot_override(uint8_t slot, const std::string &name, const std::string &action);
  enum class OverrideSave : uint8_t { OK, BAD, HOST_FULL, TEXT_FULL, WRITE_FAILED };
  OverrideSave set_override(uint8_t slot, const std::string &name, const std::string &action);
  /// Name and action characters of every override held, saved and YAML.
  size_t override_text() const;
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
  // Spare remote buttons: `spare1`..`spare<MAX_SPARES>`. They send nothing on
  // their own and exist purely as names to hang a per-host override on, for the
  // keys a remote layout needs that have no standard HID usage worth guessing.
  // 32: a real remote's app-launcher row alone wants four to ten of them on top
  // of the keys that have no standard usage (Input, Settings, Replay…), and a
  // tablet page of programmed keys wants a grid of them. Eight ran out on the
  // fuller layouts, sixteen on the grids. The count costs nothing here — this is
  // a bound, not a table — and is_spare_action's two-digit parse covers it.
  static const uint8_t MAX_SPARES = 32;
  static bool is_spare_action(const std::string &action);

  // 64, not the button count: "None" in the Remote Buttons panel saves *every*
  // action as hidden, so this ceiling has to clear the whole catalogue with room
  // for the ones added after it. The NVS blob stays well inside its 4000-byte
  // limit at this size (see load_hidden_'s length guard).
  static const uint8_t MAX_HIDDEN = 96;
  const std::vector<std::string> &get_hidden(uint8_t slot) const { return hidden_[slot]; }
  bool set_hidden(uint8_t slot, const std::vector<std::string> &names);  // replaces the set
  /// The active slot's hidden list as a comma-separated string, for the text sensor.
  std::string hidden_csv(uint8_t slot) const;

  // Per-host hold-to-repeat for the web remote. Like the hidden list above this
  // is presentation only: the browser does the timing and just sends the same
  // press again, so nothing here touches execute_action(). Stored per slot
  // because a TV wants a fast volume ramp while a PC may want only the D-pad.
  static const uint8_t MAX_REPEAT_BUTTONS = 96;  // clears the whole button catalogue, as MAX_HIDDEN does
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

  // Per-host press-and-hold (push-to-talk) for the web remote. Unlike the repeat
  // list above this one does reach the device: the browser sends a hold on
  // pointerdown and a release on pointerup, so the key stays down on the host
  // for exactly as long as the button is held.
  static const uint8_t MAX_HOLD = 96;  // clears the whole button catalogue, as MAX_HIDDEN does
  const std::vector<std::string> &get_hold(uint8_t slot) const { return hold_[slot]; }
  bool set_hold(uint8_t slot, const std::vector<std::string> &names);  // replaces the set
  /// The button in `names` that is already in the other per-host list for this
  /// slot, or an empty string. Hold-while-held and repeat-while-held are the
  /// same gesture, so one button cannot be in both.
  std::string hold_repeat_conflict(uint8_t slot, const std::vector<std::string> &names,
                                   bool checking_hold) const;
  /// The active slot's hold list as a comma-separated string, for the text sensor.
  std::string hold_csv(uint8_t slot) const;
  /// A long press sends `<key>@long`, and a Host Action of that name is the
  /// key's second action. The keys that have one on this slot, saved or YAML,
  /// without the suffix — what the page and the card time a long press for.
  static bool is_long_name(const std::string &name) {
    return name.size() > 5 && name.compare(name.size() - 5, 5, "@long") == 0;
  }
  std::vector<std::string> long_keys(uint8_t slot) const;

  // Per-host remote style: which template the web remote is drawn from for this
  // host, so a media-box slot gets a compact remote shaped for it while a PC
  // slot keeps the full one. Deliberately opaque here — the device stores an id and hands it
  // out with /hosts, and the page decides what it looks like. That way a new
  // built-in style is a page change alone, and an id this firmware has never
  // heard of (a custom one, or a newer page) still round-trips.
  // A slot that never advertises: a remote page whose keys drive Home Assistant
  // (an IR blaster, say) with no host on the other end. Everything else about a
  // slot still applies — its style, overrides, hidden/hold/repeat lists and its
  // place in the switcher — only the radio stays quiet. HID actions on such a
  // slot go nowhere, because there is nothing connected to send them to.
  bool slot_broadcasts(uint8_t slot) const {
    return slot >= MAX_HOST_SLOTS || (broadcast_mask_ & (uint16_t) (1u << slot)) != 0;
  }
  /// Persist the choice, and act on it now when it is the active slot — the tick
  /// would otherwise do nothing until the next host switch. False = bad slot.
  bool set_slot_broadcast(uint8_t slot, bool on);

  static const uint8_t MAX_STYLE_LEN = 15;
  const std::string &get_remote_style(uint8_t slot) const;
  /// Empty clears the slot back to the default style. False = bad slot or id.
  bool set_remote_style(uint8_t slot, const std::string &id);
  static bool valid_style_id(const std::string &id);

  // User-authored styles, stored whole as the compact JSON the page uploads.
  // Opaque too: the browser validates the structure (it is the only thing that
  // can — it owns the button catalogue), the device guards size and character
  // range. Uploaded in chunks because the web server takes 512 bytes of URL.
  static const uint8_t MAX_CUSTOM_TEMPLATES = 6;
  static const uint16_t MAX_TEMPLATE_LEN = 1500;
  const std::string &get_custom_template(uint8_t index) const;
  /// Append one chunk of an upload; seq 0 starts a new one. False = out of
  /// order, too long, or contains characters that can't survive the JSON trip.
  bool stage_template_chunk(uint16_t seq, const std::string &data);
  bool commit_template(uint8_t index);   // staged bytes -> storage slot
  bool delete_template(uint8_t index);

  // Imported button artwork: a logo pasted into the web page and converted there
  // into a small JSON record of path data. Opaque here for the same reason
  // styles are — the browser owns the validator, and re-applies it on every
  // draw. Unlike styles, the bodies are NOT kept in RAM. A logo runs to a few KB,
  // sixteen of them would sit against the ~23 KB free-heap trough, and the
  // device never needs the bytes itself; only a browser does. So the names live
  // here and a body is read from NVS when one is asked for, then dropped.
  static const uint8_t MAX_ICONS = 16;
  static const uint16_t MAX_ICON_LEN = 4200;
  /// Name of the icon in storage slot `index`; empty when that slot is free.
  const std::string &get_icon_name(uint8_t index) const;
  uint16_t get_icon_size(uint8_t index) const;
  /// One body, straight from NVS. False = no icon by that name, or a read failure.
  bool read_icon(const std::string &name, std::string &out) const;
  /// As stage_template_chunk, into the same buffer, capped at MAX_ICON_LEN.
  bool stage_icon_chunk(uint16_t seq, const std::string &data);
  enum class IconSave : uint8_t { OK, BAD_NAME, NOTHING_STAGED, FULL, WRITE_FAILED };
  /// Staged bytes -> the slot already holding `name`, else the first free one.
  IconSave commit_icon(const std::string &name);
  bool delete_icon(const std::string &name);

  /// Run an action string on the component's own action task instead of the
  /// caller's. The web server's task has 4352 bytes, and a chain measured on
  /// device went seven execute_action frames deep with 52 bytes left — one more
  /// frame overflowed it and rebooted the device. The main loop is not an
  /// option either: `delay:` blocks with vTaskDelay, so a chain with delays in
  /// it would stall ESPHome for its whole duration. Returns immediately; the
  /// caller does not learn whether the action ran.
  /// False when the queue was full and the action was dropped (and logged).
  bool queue_action(const std::string &action);

  /// Execute an action string (combo:, consumer:, named actions, or literal text).
  /// Used by buttons, macros, web API, and YAML automations.
  void execute_action(const std::string &action);
  /// Execute a macro by index. Returns false if index is out of range.
  /// Indices shift when an earlier macro is deleted — prefer the name overload
  /// below, which survives that.
  bool execute_macro(uint8_t index);
  /// Execute a macro by name — the stable identifier, and the same one
  /// `macro:<name>` uses. Returns false (and logs) if no macro has that name.
  /// Overload resolution keeps `execute_macro(0)` on the index form above: an
  /// integral conversion beats the user-defined const char* -> std::string.
  bool execute_macro(const std::string &name);

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  void set_web_server_base(web_server_base::WebServerBase *base) { web_server_base_ = base; }

  /// The control page, gzip-compressed at codegen from web_page.html. It points
  /// at a progmem array in main.cpp, so the handler can serve it straight out of
  /// flash without copying it to the heap.
  void set_web_page(const uint8_t *data, size_t size) {
    web_page_ = data;
    web_page_size_ = size;
  }
  const uint8_t *web_page() const { return web_page_; }
  size_t web_page_size() const { return web_page_size_; }

  /// Whether the handler checks that Host names this device — the part of the
  /// cross-origin defence a rebinding attack would otherwise walk through.
  /// Off for setups the check cannot anticipate, a reverse proxy above all.
  void set_web_host_check(bool on) { web_host_check_ = on; }
  bool web_host_check() const { return web_host_check_; }
  /// Extra names this device answers to, beyond an address literal and .local.
  void add_web_allowed_host(const std::string &host) { web_allowed_hosts_.push_back(host); }
  const std::vector<std::string> &web_allowed_hosts() const { return web_allowed_hosts_; }

  /// Whether the page may be embedded in a frame. Off by default: a framed page
  /// is still on its own origin, so a click on the framing site reaches this
  /// keyboard with every same-origin check satisfied.
  void set_web_allow_framing(bool on) { web_allow_framing_ = on; }
  bool web_allow_framing() const { return web_allow_framing_; }
#endif

#ifdef USE_BLE_KB_PEERS
  /// Another keyboard this one's page can drive over Wi-Fi, through `peer:`.
  /// `url` is http://host[:port] with no path; user/pass are its web_server login.
  void add_peer(const std::string &name, const std::string &url, const std::string &user,
                const std::string &pass);
  size_t peer_count() const { return peers_.size(); }
  /// A page is looking at the peers. The cache is only refreshed while one is.
  void note_peer_interest();
  /// The web server just took a request that is not one of the page's steady
  /// polls — a page loading, most likely. Peer reads wait until it has settled.
  void note_web_busy();
  /// The page itself was just served: a load is starting. On top of what
  /// note_web_busy() does, the cached peer state is let go for the duration.
  void note_page_load();
  /// Each peer as /peers reports it. `state` is shared with the cache rather than
  /// copied, and stays valid for as long as the caller holds it even if the
  /// cache moves on meanwhile.
  struct PeerSnapshot {
    const char *name;
    bool ok;
    int32_t age;  // seconds since its state was read; -1 before the first answer
    std::shared_ptr<const std::string> state;
  };
  void peer_snapshot(std::vector<PeerSnapshot> &out);
  /// Index of the peer called `name`, or -1.
  int peer_index(const std::string &name) const;
  /// One of the keyboard or mouse cards' requests, passed on to the same
  /// endpoint on a peer, on the action task in queue order. False when the
  /// queue was full.
  bool peer_forward(int index, const std::string &ep,
                    const std::vector<std::pair<std::string, std::string>> &params);
  /// Mouse movement and scrolling for a peer. Summed here and sent as one
  /// request whenever the last one has gone, so a drag never builds a backlog.
  void peer_add_motion(int index, int dx, int dy, int scroll);
#endif

  void set_paired_binary_sensor(binary_sensor::BinarySensor *sensor) {
    paired_binary_sensor_ = sensor;
    if (paired_binary_sensor_ != nullptr) {
      paired_binary_sensor_->publish_state(is_paired_);
    }
  }

  void set_paired(bool paired) {
    is_paired_ = paired;
    // @state is one of the panel values, and nothing else would notice.
    pending_lcd_publish_.store(true);
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
    host_leds_.store(led_byte);
    pending_led_value_.store(led_byte);
    pending_led_update_.store(true);
  }
  /// The connected host's Caps Lock as it last reported it: 1 on, 0 off, -1 not
  /// reported on this connection yet — some hosts never write the LED report.
  int host_caps() const {
    const int v = host_leds_.load();
    return v < 0 ? -1 : (v & 0x02) ? 1 : 0;
  }

  // ── Battery Service ────────────────────────────────────────────────────────
  // The service is always advertised, so an unfed build reports its initial
  // 100% forever. `battery_level:` points it at a sensor; the
  // set_battery_level action and HA service cover values that don't come from
  // one. Levels outside 0..100 are clamped rather than refused.
  void set_battery_level(uint8_t percent);
  uint8_t battery_level() const;
  /// Follow an existing sensor (a battery voltage-to-percent template, an ADC).
  void set_battery_sensor(sensor::Sensor *battery);
  /// Called from the GATTS task when a host subscribes — the send itself has to
  /// happen on the ESPHome loop, like every other queued BLE update here.
  void queue_battery_notify() { pending_battery_notify_.store(true); }

  void set_connected(bool connected, uint16_t conn_id) {
    is_connected_ = connected;
    conn_id_ = conn_id;
    // Which slot this link was made for. Advertising only ever runs for the
    // active slot, so that is the one a new connection belongs to.
    link_slot_.store(connected ? (int8_t) active_slot_ : (int8_t) -1);
    link_secure_.store(false);
    pending_lcd_publish_.store(true);   // @state
    // Lock states belong to the host: unknown until this one reports them, and
    // the LED sensors go dark on a disconnect rather than showing the last
    // host's Caps Lock against the next one.
    host_leds_.store(-1);
    if (!connected) {
      pending_led_value_.store(0);
      pending_led_update_.store(true);
    }
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
  /// Whether the radio is out there waiting to be found. Defined in the .cpp,
  /// where the advertising state lives: "Disconnected" cannot tell a keyboard
  /// that is calling from one that has gone quiet, and that is the difference
  /// between waiting and going to look at the host's Bluetooth settings.
  bool is_advertising() const;
  uint16_t conn_id() const { return conn_id_; }
  /// Called from the GAP task once the current link is encrypted.
  void mark_link_secure() { link_secure_.store(true); }

  // Multi-host switching
  /// `from_action` marks a switch made by an action string, whose style change
  /// waits for the whole action to finish — see switch_host() and style_slot().
  void switch_host(uint8_t slot, bool from_action = false);
  void forget_host(uint8_t slot);
  uint8_t active_host_slot() const { return active_slot_; }
  /// The slot whose style, hidden, hold and repeat lists the remote is drawn
  /// from. The active one, except while an action that switched host is still
  /// running: a macro that visits another host and comes back must not re-skin
  /// the remote twice on its way through.
  uint8_t style_slot() const {
    const int8_t held = style_hold_.load();
    return held >= 0 ? (uint8_t) held : active_slot_;
  }
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

  /// True if the stack holds a bond for this exact address, matching both the
  /// address a record was filed under and the identity inside its ID key.
  bool peer_is_bonded(const esp_bd_addr_t addr) const;

  /// Record that a bond went away, and why. Called from every site that removes
  /// one, plus the boot census for bonds that vanished on their own. Writes NVS,
  /// so it is for real events only — never per poll.
  void bond_log_record(uint8_t cause, uint8_t reason, uint8_t slot, const esp_bd_addr_t addr);
  /// The recorder as JSON, newest record first. Served by /bondlog.
  std::string bond_log_json() const;

  /// Resolve a peer's stable identity address. Android connects with a resolvable
  /// private address that rotates every ~15 minutes; the identity address it hands
  /// over at bonding does not. Matches `addr` against both the bonded connection
  /// address and the stored identity, so it works whichever one the caller holds.
  /// False when the peer is not bonded or sent no ID key — `out` is left alone.
  bool peer_identity_addr(const esp_bd_addr_t addr, esp_bd_addr_t &out) const;

  /// The peer's Identity Resolving Key, 16 bytes, from the same ID key the identity
  /// address above comes from. It is what lets something else — Home Assistant, a
  /// second ESP32 — recognise this host from the rotating address it advertises,
  /// which is the one thing the identity address cannot do: that address is only
  /// ever sent over an encrypted link at bonding, never broadcast.
  ///
  /// Treat what comes out of here as a secret. It de-anonymises that device's random
  /// address for as long as the bond lives, so it must not go anywhere the identity
  /// address freely goes (the /hosts poll, backups, logs).
  ///
  /// Handed back most-significant byte first, which is not how the stack stores it
  /// — see the definition. That is the order every consumer of an IRK as text
  /// expects, so this is the form to print; the reversed one silently matches
  /// nothing.
  ///
  /// False when the peer is not bonded or sent no ID key — `out` is left alone.
  bool peer_irk(const esp_bd_addr_t addr, uint8_t out[16]) const;

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

  // Which remote style the active host uses, so the Lovelace card can draw the
  // same one the web page does. Same reason as the lists below — the id is
  // already on the /hosts response, but a dashboard on https can't fetch it.
  // Empty state means the host is on the default style.
  void set_remote_style_sensor(text_sensor::TextSensor *sensor) {
    remote_style_sensor_ = sensor;
    publish_remote_style_();
  }

  // ── LCD panels ───────────────────────────────────────────────────────────
  // An ["lcd",…] section in a remote style shows live values. Which entities
  // are worth reading is declared in YAML rather than discovered from the
  // style, because the device never parses a style — it stores the JSON
  // opaquely, on purpose, so a new section kind stays a page change alone.
  // That leaves this list as the only statement of what to format and publish,
  // and it is what bounds both the /status payload and the 255-character state
  // Home Assistant will carry.
  static const uint8_t MAX_SOURCES = 8;
  /// Longest text an `lcd:` action may put on a panel. Generous against a
  /// 16-character line, mean against the 255 a Home Assistant state holds.
  static const uint8_t MAX_LCD_MSG_LEN = 64;
  void add_source_sensor(const std::string &key, sensor::Sensor *s,
                      const std::string &unit, int8_t decimals);
  void add_source_text_sensor(const std::string &key, text_sensor::TextSensor *s);
  /// A boolean source, published as the literal "on"/"off". This is the one the
  /// `if:` action branches on and a button's `lit:` token colours from — point
  /// it at a `homeassistant` platform binary sensor to follow Home Assistant.
  void add_source_binary_sensor(const std::string &key, binary_sensor::BinarySensor *s);
  /// True/false for a boolean source, or no value when it has none yet — which
  /// is what makes `if:` sit still rather than guess before HA has connected.
  bool source_bool(const std::string &key, bool &out) const;
#ifdef USE_TEXT
  void add_source_text(const std::string &key, text::Text *t);
#endif
  /// Every value a panel can name, already formatted — unit, decimals and all —
  /// so neither the web page nor the card has to know what kind of entity is
  /// behind a key. Includes the @-prefixed built-ins even with no sources
  /// declared. Built fresh per call; it is asked for at most every 3 s.
  std::vector<std::pair<std::string, std::string>> lcd_values() const;
  /// The unclamped map as a JSON object, rebuilt on the main loop. /status
  /// serves this rather than building it per request: the web task has 4352
  /// bytes of stack and about 860 spare, and walking every source there meant a
  /// call chain of string-building frames on the thinnest stack in the system.
  const std::string &lcd_status_json() const { return lcd_status_json_; }
  /// Optional text sensor carrying the panel values to the Lovelace card, cut
  /// to what a Home Assistant state holds — the same reason the hidden, hold and
  /// repeat lists travel as sensors: a dashboard on https cannot fetch this
  /// device's API at all.
  void set_lcd_sensor(text_sensor::TextSensor *sensor) {
    lcd_sensor_ = sensor;
    publish_lcd_();
  }
  /// What a host slot is called — its switch_host button's name if it has one,
  /// otherwise "Host N", counting from 1 as every other display of it does.
  std::string host_label(uint8_t slot) const;

  // Press-and-hold buttons for the active host, published the same way — the
  // Lovelace remote card reads it to know which of its buttons should hold
  // rather than tap. The card can't read the device's REST API reliably (HA
  // over https blocks the mixed-content fetch), so this is how the Host Actions
  // choice reaches it.
  void set_hold_sensor(text_sensor::TextSensor *sensor) {
    hold_sensor_ = sensor;
    publish_hold_();
  }

  // Same idea for the repeat set, in the NVS format "<delay>,<rate>,name,name".
  // Empty means this host was never configured, so the card keeps its own
  // defaults. Without this the card can only repeat its four hardcoded buttons
  // and silently ignores whatever Host Actions says.
  void set_repeat_sensor(text_sensor::TextSensor *sensor) {
    repeat_sensor_ = sensor;
    publish_repeat_();
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
  /// The last reading, for the @rssi panel value. has_rssi_ stays false until
  /// one arrives, so a panel shows dashes rather than a plausible-looking 0.
  bool has_rssi() const { return has_rssi_; }
  int8_t last_rssi() const { return last_rssi_; }
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

  /// Record a bond loss seen from a BLE callback. Same reason as the reject above:
  /// bond_log_record() commits to NVS, which has no business running on
  /// Bluedroid's task, so loop() does the write.
  void queue_bond_log(uint8_t cause, uint8_t reason, uint8_t slot, const esp_bd_addr_t addr) {
    uint8_t n = pending_bond_log_count_.load();
    // These events are rare by definition; a full queue would mean something is
    // wrong that the records already there describe.
    if (n >= PENDING_BOND_LOG) return;
    pending_bond_log_[n].cause = cause;
    pending_bond_log_[n].reason = reason;
    pending_bond_log_[n].slot = slot;
    memcpy(pending_bond_log_[n].addr, addr, sizeof(esp_bd_addr_t));
    pending_bond_log_count_.store((uint8_t) (n + 1));
  }

  // Peer address and RSSI state — public so static GAP/GATTS handlers can access them directly
  esp_bd_addr_t peer_addr_{};
  sensor::Sensor *active_host_sensor_{nullptr};
  text_sensor::TextSensor *hidden_sensor_{nullptr};
  text_sensor::TextSensor *remote_style_sensor_{nullptr};
  text_sensor::TextSensor *hold_sensor_{nullptr};
  text_sensor::TextSensor *repeat_sensor_{nullptr};
  text_sensor::TextSensor *host_mac_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  text_sensor::TextSensor *lcd_sensor_{nullptr};
  bool rssi_pending_{false};
  std::atomic<bool> pending_rssi_nan_{false};
  std::atomic<bool> pending_rssi_update_{false};
  std::atomic<int8_t> pending_rssi_value_{0};
  int8_t last_rssi_{0};
  bool has_rssi_{false};

 protected:
  /// The ID key a bonded peer distributed, looked up by either the address it
  /// connected with or the identity inside that key — callers hold one or the
  /// other depending on whether the slot has been matched up yet. Backs both
  /// peer_identity_addr() and peer_irk(), which each want a different field of it.
  /// False when the peer is not bonded or distributed no ID key.
  bool peer_id_keys_(const esp_bd_addr_t addr, esp_ble_pid_keys_t &out) const;

#if defined(USE_API) && defined(USE_API_CUSTOM_SERVICES)
  // Auto-registered HA services (api_services: true). Names and arg names must
  // stay in sync with the HA cards (docs/*-card.js) and the README yaml snippets.
  // register_service() static-asserts without USE_API_CUSTOM_SERVICES (the python
  // side force-enables `api: custom_services: true` whenever api_services is on).
  void register_api_services_();
  // int32_t (not int) — the api component only specializes service args for
  // int32_t, and on xtensa int32_t is a distinct type from int (link error).
  // Queued, not run here. These arrive on the API task, and an action string
  // is allowed to take seconds: `delay:` blocks with vTaskDelay, and a chain of
  // them blocked the loop long enough for the task watchdog to reboot the
  // device — an HA card press of a power button that fires Wake-on-LAN ten
  // times is about ten seconds of it. The action task exists for exactly this.
  void on_api_run_action_(std::string action) { queue_action(action); }
  void on_api_run_macro_(int32_t index) { queue_macro_index_(index); }
  void on_api_run_macro_name_(std::string name) { queue_action("macro:" + name); }
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
  void on_api_set_battery_level_(int32_t percent) {
    set_battery_level((uint8_t) (percent < 0 ? 0 : (percent > 100 ? 100 : percent)));
  }
  void on_api_switch_host_(int32_t slot) { switch_host((uint8_t) slot); }
  void on_api_forget_host_(int32_t slot) { forget_host((uint8_t) slot); }
#endif
  bool api_services_enabled_{false};
  bool ha_action_enabled_{false};
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
  std::atomic<int16_t> host_leds_{-1};  // written on the Bluetooth task, so a waiter sees it at once
  std::atomic<bool> pending_battery_notify_{false};
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
  /// Behind switch_host:next|prev|back and the next_host / prev_host /
  /// last_host remote keys.
  void cycle_host_(int delta);
  void return_to_last_host_();

  /// Backs the `press_button:<object_id>` action — presses another ESPHome
  /// button. Honours hide_buttons and refuses to nest.
  void press_external_button_(const std::string &object_id);

  /// Backs the `macro:<name>` action — runs a stored macro by name, so an
  /// override references it rather than copying its text. Depth-capped.
  /// False if the name doesn't resolve or the depth cap stopped it.
  bool run_macro_by_name_(const std::string &name);

  /// Backs the `ha_action:<domain>.<service>;k=v;…` action — fires a Home
  /// Assistant action over the native API. Declared unconditionally; the
  /// implementation degrades to a warning without api support.
  void do_ha_action_(const std::string &payload);

  const std::string *find_override_(uint8_t slot, const std::string &name) const;
  void load_overrides_();
  bool save_overrides_(uint8_t slot);

  // Per-host hidden buttons (NVS key "hid<slot>", comma-separated names).
  std::vector<std::string> hidden_[MAX_HOST_SLOTS];
  void load_hidden_();
  void save_hidden_(uint8_t slot);
  void publish_hidden_();  // push the shown slot's list to the text sensor
  void publish_remote_style_();  // push the shown slot's style id to the text sensor

  // One bit per slot, set = that slot advertises. A bitmask in a single NVS key
  // rather than ten keys: it is one boolean per slot and one write. An absent
  // key reads as all-set, so a device updating to this firmware keeps behaving
  // exactly as it did.
  uint16_t broadcast_mask_{0xFFFF};
  void load_broadcast_();
  void save_broadcast_();

  // LCD panel sources, and the last payload published for them. Every declared
  // source raises a flag on change rather than publishing from whatever task
  // updated it; loop() coalesces those into at most one publish a second, so a
  // fast sensor cannot flood the API connection.
  struct Source {
    std::string key;
    sensor::Sensor *num{nullptr};
    text_sensor::TextSensor *txt{nullptr};
    binary_sensor::BinarySensor *flag{nullptr};
#ifdef USE_TEXT
    text::Text *fld{nullptr};
#endif
    std::string unit;       // empty = whatever the sensor declares
    int8_t decimals{-1};    // <0 = whatever the sensor declares
  };
  std::vector<Source> sources_;
  // What a panel can say about what was just pressed. Held in RAM only: a
  // station key gets pressed many times an hour and none of this is worth a
  // flash write. last_action_ is every outermost press, last_spare_ only the
  // spares — so a volume tap cannot wipe the station you are on — and lcd_msg_
  // is whatever an `lcd:` action put there.
  std::string last_action_, last_spare_, lcd_msg_;
  std::string last_lcd_json_;
  // Which keys the sensor string had no room for last time — the ones worth
  // reporting, not @host/@slot/@mac — so the warning is one line per change
  // rather than one per rebuild.
  std::string last_lcd_drop_;
  std::string lcd_status_json_{"{}"};
  std::string lcd_sensor_json_{"{}"};
  UBaseType_t lcd_stack_low_{0};
  void rebuild_lcd_status_();
  uint32_t lcd_last_publish_ms_{0};
  // True at boot so the /status cache is built on the first loop.
  std::atomic<bool> pending_lcd_publish_{true};
  void publish_lcd_();

  // The two branch-running verbs, kept out of execute_action's own frame. That
  // function recurses, and a frame reserves every branch's locals whether or not
  // it takes them — which is what ran the 4352-byte web task down to 484 bytes.
  void run_alternate_(const std::string &body);
  void run_if_(const std::string &action);
  void run_steps_(const std::string &action);
  // Stack accounting for a whole action chain — see report_action_stack_().
  void report_action_stack_();
  /// Resolve a macro index to its action string and queue that.
  void queue_macro_index_(int32_t index);

  // The action task and its work queue. Sized from the measurement above:
  // ~240 bytes a frame, seven frames on the deepest real chain, so 6 KB leaves
  // room for roughly twice that depth.
  static const uint32_t ACTION_TASK_STACK = 6144;
  // For switch_host:back and wait:connected. previous_slot_ is the slot active
  // before the last switch; link_slot_ and link_secure_ are written from the
  // Bluetooth task and read by whichever task runs the macro.
  int8_t previous_slot_{-1};
  std::atomic<int8_t> link_slot_{-1};
  std::atomic<bool> link_secure_{false};
  bool wait_host_ready_(uint32_t timeout_ms);
  void set_caps_lock_(const std::string &how);
  // The slot the remote stays drawn for while an action that switched host is
  // still running; -1 when it simply follows the active slot. Written by the
  // task running the action, read by the web task serving /hosts.
  std::atomic<int8_t> style_hold_{-1};
  void release_style_hold_();
  void publish_remote_lists_();
  static const uint8_t ACTION_QUEUE_DEPTH = 8;
  QueueHandle_t action_queue_{nullptr};
  TaskHandle_t action_task_{nullptr};
  static void action_task_entry_(void *arg);
  UBaseType_t act_low_{(UBaseType_t) -1};
  uint8_t act_deepest_{0};

  // Per-host hold-to-repeat (NVS key "rpt<slot>", "<delay>,<rate>,name,name").
  RepeatCfg repeat_[MAX_HOST_SLOTS];
  void load_repeat_();
  void save_repeat_(uint8_t slot);
  void publish_repeat_();  // push the active slot's config to the text sensor

  // Per-host press-and-hold (NVS key "hld<slot>", comma-separated names).
  std::vector<std::string> hold_[MAX_HOST_SLOTS];
  void load_hold_();
  void save_hold_(uint8_t slot);
  void publish_hold_();  // push the active slot's list to the text sensor

  // Per-host remote style id (NVS key "rst<slot>"); empty = the default style.
  std::string remote_style_[MAX_HOST_SLOTS];
  void load_remote_style_();
  void save_remote_style_(uint8_t slot);

  // Per-host on-connect action (NVS key "onc<slot>"). loop() watches the link
  // become ready: ready_since_ms_ is when it did (0 while it isn't), and
  // on_connect_ran_ms_ when each slot last ran its action, to space runs out.
  std::string on_connect_[MAX_HOST_SLOTS];
  void load_on_connect_();
  bool host_ready_() const;
  void check_on_connect_();
  uint32_t ready_since_ms_{0};
  bool on_connect_checked_{false};
  uint32_t on_connect_ran_ms_[MAX_HOST_SLOTS]{};

  // Custom remote styles (NVS key "ctpl<index>"), and the buffer a chunked
  // upload fills before commit_template() moves it into one of the slots.
  std::string custom_templates_[MAX_CUSTOM_TEMPLATES];
  std::string tpl_staging_;
  uint16_t tpl_next_seq_{0};
  // What tpl_staging_ holds. Styles and icons share the buffer, and an upload of
  // one kind must never be committed as the other — a style import cut short by
  // an icon import would otherwise save a logo into a style slot.
  enum : uint8_t { STAGED_NONE, STAGED_STYLE, STAGED_ICON };
  uint8_t staging_kind_{STAGED_NONE};
  bool stage_chunk_(uint16_t seq, const std::string &data, uint16_t cap, uint8_t kind);
  void clear_staging_();
  void load_templates_();
  void save_template_(uint8_t index);

  // Imported icons (NVS keys "icn<index>" for the body, "icnm<index>" for the
  // name). Names and sizes only — see MAX_ICONS for why the bodies stay in flash.
  std::string icon_names_[MAX_ICONS];
  uint16_t icon_sizes_[MAX_ICONS]{};
  void load_icon_names_();
  int find_icon_(const std::string &name) const;

  // Multi-host state
  uint8_t host_slots_{MAX_HOST_SLOTS};
  uint8_t active_slot_{0};
  HostSlot hosts_[MAX_HOST_SLOTS];
  HostSlotConfig host_slot_configs_[MAX_HOST_SLOTS]{};
  std::string slot_layout_id_[MAX_HOST_SLOTS]{};  // YAML per-slot layout; empty = no override
  esp_bd_addr_t slot_addrs_[MAX_HOST_SLOTS]{};  // per-slot random BLE address
  void load_host_slots_();
  void generate_slot_addrs_();

  // Bond-loss recorder state
  static const uint8_t PENDING_BOND_LOG = 4;
  std::atomic<uint8_t> pending_bond_log_count_{0};
  BondLossRecord pending_bond_log_[PENDING_BOND_LOG]{};
  BondLogBlob bond_log_{};
  /// Load the recorder and count this boot. Must run before the boot census.
  void bond_log_init_();
  void bond_log_save_();
  /// Warn about — and record — any occupied slot the stack has no bond for. Runs
  /// once at boot, and is the only thing that can catch a bond removed by
  /// something outside this component.
  void bond_census_();

#ifdef USE_BLE_KEYBOARD_WEB_CONTROL
  web_server_base::WebServerBase *web_server_base_{nullptr};
  BleKeyboardWebControl *web_control_{nullptr};
  const uint8_t *web_page_{nullptr};
  size_t web_page_size_{0};
  bool web_host_check_{true};
  bool web_allow_framing_{false};
  std::vector<std::string> web_allowed_hosts_;
#endif

#ifdef USE_BLE_KB_PEERS
  // Linked keyboards. Every request to one runs on the action task: presses in
  // the order they were queued, and the /state cache on its quiet ticks. The
  // web task only reads the cache, under peer_mutex_.
  struct Peer {
    std::string name, host, user, pass;
    uint16_t port{80};
    uint32_t ip{0};        // network order; 0 until a .local name has been looked up
    bool by_name{false};   // a .local name, looked up again if its address stops answering
    // The login it last asked for, answered up front on every request after.
    enum : uint8_t { AUTH_NONE, AUTH_BASIC, AUTH_DIGEST } auth{AUTH_NONE};
    std::string realm, nonce, opaque;
    bool qop_auth{false};
    uint32_t nc{0};
    // Its last /state reply; null when nobody is looking. Shared so /peers can
    // send it straight from here instead of copying it into a reply.
    std::shared_ptr<const std::string> state;
    uint32_t fetched_ms{0};     // when `state` arrived
    uint32_t next_due_ms{0};    // next /state read
    uint32_t down_until_ms{0};  // after it can't be reached, presses are dropped until this
    bool ok{false};             // whether it is answering; what greys its bar
    uint8_t read_fails{0};      // /state reads failed in a row
  };
  std::vector<Peer> peers_;
  SemaphoreHandle_t peer_mutex_{nullptr};
  std::atomic<uint32_t> peer_interest_ms_{0};
  std::atomic<uint32_t> web_busy_ms_{0};
  std::atomic<uint32_t> page_load_ms_{0};
  bool peer_cache_live_{false};
  bool peer_starved_{false};  // reads paused for want of memory; logged on each change
  static const uint32_t PEER_QUIET_MS = 1500;          // no reads this soon after a page-load request
  static const size_t PEER_READ_MIN_BLOCK = 8192;      // a read holds a client, a socket and the reply
  static const size_t PEER_READ_MIN_FREE = 16384;
  static const size_t PEER_PRESS_MIN_BLOCK = 4096;
  static const uint32_t PEER_TIMEOUT_MS = 1500;       // a press
  static const uint32_t PEER_READ_TIMEOUT_MS = 3000;  // a /state read, which can wait behind its own page
  static const uint32_t PEER_POLL_MS = 4000;
  static const uint32_t PEER_RETRY_MS = 15000;
  static const uint32_t PEER_INTEREST_MS = 15000;
  static const uint32_t PEER_SLOW_MS = 700;
  static const size_t PEER_MAX_REPLY = 6144;
  static const size_t PEER_MAX_CHAIN = 240;
  // BUSY is its 409: short of memory for the reply just now, not gone.
  enum PeerResult : uint8_t { PEER_OK, PEER_BUSY, PEER_NO_REPLY, PEER_UNREACHABLE };
  PeerResult peer_request_(Peer &p, bool post, const char *path, const std::string &body, std::string *out);
  PeerResult peer_exchange_(Peer &p, bool post, const char *path, const std::string &body, std::string *out,
                            int &status, std::string &challenge);
  bool peer_resolve_(Peer &p);
  void peer_auth_append_(Peer &p, const char *method, const char *path, std::string &out);
  bool peer_take_challenge_(Peer &p, const std::string &challenge);
  void run_peer_action_(const std::string &action);
  bool coalesce_peer_presses_(std::string &job);
  void refresh_peers_();
  // Keyboard and mouse requests for a peer ride the action queue as
  // "\x1F<index>\x1F<endpoint>\x1F<payload>" — a control character no action
  // string starts with. The payload is the text for "string", nothing for
  // "motion", and the form-encoded body for the rest.
  static const char PEER_JOB = '\x1F';
  static const size_t MAX_PEERS = 4;          // keep in sync with MAX_PEERS in __init__.py
  static const size_t PEER_MAX_TEXT_BODY = 800;  // typed text merged into one request, encoded
  std::atomic<int32_t> peer_dx_[MAX_PEERS]{}, peer_dy_[MAX_PEERS]{}, peer_scroll_[MAX_PEERS]{};
  std::atomic<bool> peer_motion_queued_[MAX_PEERS]{};
  PeerResult peer_post_(Peer &p, const std::string &path, const std::string &body, const char *what);
  void run_peer_forward_(const std::string &job);
  void flush_peer_motion_(int index);
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
  uint8_t caps_corrected_(uint8_t modifier, bool letter) const;
  void send_battery_notify_();
  void send_mouse_report_(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

  // Non-blocking string typing state machine (driven from loop())
  // Keystrokes are pre-resolved (UTF-8 decoded + layout-mapped) at enqueue time,
  // so a mid-type layout switch can't garble already-queued text.
  SemaphoreHandle_t type_mutex_{nullptr};
  std::vector<TypedStroke> type_queue_;
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
  // Nesting depth for action strings themselves. repeat:, alternate: and the
  // '|' split all call back into execute_action(), and nothing bounded that:
  // the per-level repeat cap multiplies through nesting rather than limiting
  // it, so a string short enough to fit in one request could recurse far deeper
  // than the web server task's stack — measured at ~860 bytes free — and run
  // off the end of it before typing a single key. Eight is past anything
  // written by hand and well short of what the stack can carry.
  static const uint8_t MAX_ACTION_DEPTH = 8;
  uint8_t action_depth_{0};
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
