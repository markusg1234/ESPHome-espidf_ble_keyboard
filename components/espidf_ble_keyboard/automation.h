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

}  // namespace espidf_ble_keyboard
}  // namespace esphome
