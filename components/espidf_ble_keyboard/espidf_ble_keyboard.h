#pragma once
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <atomic>
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

  // Setter for YAML-configured device name
  void set_device_name(const std::string &name) { device_name_ = name; }
  const std::string &device_name() const { return device_name_; }

  // Setter and check for YAML-configured passkey
  void set_passkey(uint32_t passkey) { 
    passkey_ = passkey; 
    has_passkey_ = true; 
  }
  void set_passkey_secure_connections(bool enabled) { passkey_secure_connections_ = enabled; }
  bool has_passkey() const { return has_passkey_; }
  uint32_t passkey() const { return passkey_; }
  bool passkey_secure_connections() const { return passkey_secure_connections_; }

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

  // Queue state updates from BLE callback context for publish in loop().
  void queue_paired_state(bool paired) {
    pending_paired_state_.store(paired);
    pending_paired_update_.store(true);
  }

  void set_connected(bool connected, uint16_t conn_id) {
    is_connected_ = connected;
    conn_id_ = conn_id;
  }
  bool is_connected() const { return is_connected_; }
  uint16_t conn_id() const { return conn_id_; }

 protected:
  std::string device_name_{"ESP32 BLE KB"};
  bool is_connected_{false};
  uint16_t conn_id_{0};
  bool is_paired_{false};
  std::atomic<bool> pending_paired_update_{false};
  std::atomic<bool> pending_paired_state_{false};
  binary_sensor::BinarySensor *paired_binary_sensor_{nullptr};
  
  uint32_t passkey_{0};
  bool has_passkey_{false};
  bool passkey_secure_connections_{false};

  // Non-blocking string typing state machine (driven from loop())
  std::string type_queue_;
  size_t type_index_{0};
  bool type_key_up_pending_{false};
  uint32_t type_next_ms_{0};
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