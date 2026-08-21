#pragma once
#include "esphome/core/automation.h"
#include "espidf_ble_keyboard.h"
#include <string>

namespace esphome {
namespace espidf_ble_keyboard {

class SendStringTrigger : public Trigger<> {
 public:
  explicit SendStringTrigger(EspidfBleKeyboard *parent, std::string str)
      : parent_(parent), str_(std::move(str)) {}
  void trigger() { parent_->send_string(str_); }
 protected:
  EspidfBleKeyboard *parent_;
  std::string str_;
};

class SendCtrlAltDelTrigger : public Trigger<> {
 public:
  explicit SendCtrlAltDelTrigger(EspidfBleKeyboard *parent) : parent_(parent) {}
  void trigger() { parent_->send_ctrl_alt_del(); }
 protected:
  EspidfBleKeyboard *parent_;
};

// ── Press-and-hold actions ───────────────────────────────────────────────────
//
// An ESPHome `button` is momentary — press_action() with no release — so a
// physical push-to-talk key drives these from a binary_sensor's on_press /
// on_release instead. Templatable so the key can come from a lambda.

template<typename... Ts> class KeyHoldAction : public Action<Ts...> {
 public:
  explicit KeyHoldAction(EspidfBleKeyboard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, modifier)
  TEMPLATABLE_VALUE(uint8_t, key)
  void play(Ts... x) override {
    parent_->key_hold(this->modifier_.value(x...), this->key_.value(x...));
  }
 protected:
  EspidfBleKeyboard *parent_;
};

/// Hold a named / parametric action (`consumer:0x00E9`, `volume_up`, …), so
/// anything holdable is reachable without spelling out a modifier and keycode.
template<typename... Ts> class HoldActionAction : public Action<Ts...> {
 public:
  explicit HoldActionAction(EspidfBleKeyboard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, action)
  void play(Ts... x) override {
    std::string a = this->action_.value(x...);
    if (!parent_->hold_action(a)) parent_->execute_action(a);
  }
 protected:
  EspidfBleKeyboard *parent_;
};

/// Run any action string from a YAML automation without a lambda — the same
/// strings a `button:` entry takes, so a trigger reaches `macro:<name>`, a
/// multi-step sequence or any named action directly. Blocking, like the rest:
/// `delay:` steps are vTaskDelay, so the whole sequence finishes inside play().
template<typename... Ts> class RunActionAction : public Action<Ts...> {
 public:
  explicit RunActionAction(EspidfBleKeyboard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, action)
  void play(Ts... x) override { parent_->execute_action(this->action_.value(x...)); }
 protected:
  EspidfBleKeyboard *parent_;
};

template<typename... Ts> class KeyReleaseAction : public Action<Ts...> {
 public:
  explicit KeyReleaseAction(EspidfBleKeyboard *parent) : parent_(parent) {}
  void play(Ts... x) override { parent_->release_held(); }
 protected:
  EspidfBleKeyboard *parent_;
};

/// Feed the BLE Battery Service from a lambda or a template value, for a level
/// that doesn't come from a plain sensor `battery_level:` can point at.
template<typename... Ts> class SetBatteryLevelAction : public Action<Ts...> {
 public:
  explicit SetBatteryLevelAction(EspidfBleKeyboard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, level)
  void play(Ts... x) override { parent_->set_battery_level(this->level_.value(x...)); }
 protected:
  EspidfBleKeyboard *parent_;
};

class RssiAboveTrigger : public Trigger<int> {
 public:
  RssiAboveTrigger(EspidfBleKeyboard *parent, int8_t threshold) {
    parent->add_rssi_above_callback([this, threshold](int8_t rssi) {
      if (rssi > threshold) this->trigger(rssi);
    });
  }
};

class RssiBelowTrigger : public Trigger<int> {
 public:
  RssiBelowTrigger(EspidfBleKeyboard *parent, int8_t threshold) {
    parent->add_rssi_below_callback([this, threshold](int8_t rssi) {
      if (rssi < threshold) this->trigger(rssi);
    });
  }
};

}  // namespace espidf_ble_keyboard
}  // namespace esphome
