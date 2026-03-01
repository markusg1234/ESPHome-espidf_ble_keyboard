#pragma once
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include <string>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"

namespace esphome {
namespace espidf_ble_keyboard {

class EspidfBleKeyboard : public Component {
 public:
  void setup() override;
  void loop() override;
  void send_string(const std::string &str);
  void send_ctrl_alt_del();
  void send_key_combo(uint8_t modifiers, uint8_t keycode);
  void send_sleep();
  void send_shutdown();
  void send_hibernate();
  // Consumer control
  void send_consumer(uint16_t usage);
  void send_power();
  void send_media_play_pause();
  void send_media_next();
  void send_media_prev();
  void send_media_stop();
  void send_volume_up();
  void send_volume_down();
  void send_mute();

  // Setter and check for YAML-configured passkey
  void set_passkey(uint32_t passkey) { 
    passkey_ = passkey; 
    has_passkey_ = true; 
  }
  bool has_passkey() const { return has_passkey_; }
  uint32_t passkey() const { return passkey_; }

  void set_connected(bool connected, uint16_t conn_id) {
    is_connected_ = connected;
    conn_id_ = conn_id;
  }
  bool is_connected() const { return is_connected_; }
  uint16_t conn_id() const { return conn_id_; }

 protected:
  bool is_connected_{false};
  uint16_t conn_id_{0};
  
  uint32_t passkey_{0};
  bool has_passkey_{false};
};

class EspidfBleKeyboardButton : public button::Button, public Component {
 public:
  void set_parent(EspidfBleKeyboard *parent) { parent_ = parent; }
  void press_action() override;
  void set_action(const std::string &action) { action_ = action; }
 protected:
  EspidfBleKeyboard *parent_{nullptr};
  std::string action_;
};

}  // namespace espidf_ble_keyboard
}  // namespace esphome