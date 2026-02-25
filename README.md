# ESP32 BLE HID Keyboard for ESPHome

This is a custom ESPHome component that transforms an ESP32 into a Bluetooth Low Energy (BLE) HID Keyboard. Unlike many other implementations, this component uses the **direct ESP-IDF Bluedroid GATTS API** for improved reliability and compatibility.

## Features

* **Standard HID Keyboard:** Recognized as a native keyboard by Windows.
* **Secure Pairing:** Supports a configurable 6-digit static passkey (PIN) for secure bonding.
* **Efficient Memory Usage:** Direct API implementation ensures stability even with complex ESPHome configurations.
* **Key Combos:** Send any modifier + key combination using hex keycodes (e.g. Win+R, Ctrl+C).
* **String Typing:** Type any string directly including letters, numbers and punctuation.
* **Pre-defined Actions:** Built-in helpers for `ctrl_alt_del`, `sleep`, `hibernate` and `shutdown`.

📖 [Keycode Reference](docs/keycodes.md) · [🌐 View Keycode Reference as Web Page](https://markusg1234.github.io/ESPHome-espidf_ble_keyboard/)keycodes.html)


## Usage Example

Add the following to your ESPHome YAML configuration:

```yaml
substitutions:
  device_name: bluetooth-keyboard
  friendly_name: "Bluetooth keyboard"
  wifi_ssid: "***"
  wifi_password: "***"
  api_encryption_key: "***"
  ota_password: "***"

esphome:
  name: ${device_name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    # Version 5.5.2 is standard now in PlatformIO for ESP-IDF
    version: 5.5.2 
    sdkconfig_options:
      # These are the essential ones for HID/Keyboard stability
      CONFIG_BT_ENABLED: y
      CONFIG_BT_BLE_ENABLED: y
      CONFIG_BT_BLUEDROID_ENABLED: y
      CONFIG_GATTS_ENABLE: y
      # Windows requires a higher security level for HID devices
      CONFIG_BT_SMP_ENABLE: y
      CONFIG_BT_ACL_CONNECTIONS: "4"

logger:

api:
  encryption:
    key: ${api_encryption_key}

ota:
  - platform: esphome
    password: ${ota_password}

wifi:
  ssid: ${wifi_ssid}
  password: ${wifi_password}
  power_save_mode: light
  fast_connect: true

external_components:
  - source:
      type: git
      url: https://github.com/markusg1234/ESPHome-espidf_ble_keyboard
      ref: main
      path: components
    components: [ espidf_ble_keyboard ]

espidf_ble_keyboard:
  id: my_keyboard
  # Optional: Set a 6-digit pairing code. 
  # If omitted, the device will use "Just Works" (no PIN) pairing.
  passkey: 123456

button:

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Ctrl + F1"
    action: "combo:0x01:0x3A"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Win + R (Run Dialog)"
    # 0x08 = Windows Key, 0x15 = 'r'
    action: "combo:0x08:0x15"

  - platform: template
    name: "Template Hello"
    on_press:
      - lambda: |-
          id(my_keyboard).send_string("Hello\n");

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Type Hello"
    action: "Hello\n"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Ctrl Alt Del"
    action: "ctrl_alt_del"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Sleep PC"
    action: "sleep"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Hibernate PC"
    action: "hibernate"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Shutdown PC"
    action: "shutdown"

  - platform: restart
    name: ${friendly_name}

binary_sensor:
  - platform: status
    name: ${friendly_name}
```

## Configuration Variables

### `espidf_ble_keyboard`

* **id** (Required, ID): The ID used to link buttons or automations to this keyboard.
* **passkey** (Optional, int): A 6-digit static PIN (000000–999999). If set, the device will require this PIN during the initial pairing process.

### `button` (Platform: `espidf_ble_keyboard`)

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **action** (Required, string): The action to perform when the button is pressed.

#### Action Types

| Action | Description |
|---|---|
| `"Hello\n"` | Type a string. Use `\n` for Enter. Supports letters, numbers and common punctuation. |
| `"combo:0x08:0x15"` | Send a key combination. Format: `combo:<modifier_hex>:<keycode_hex>`. See [Keycode Reference](docs/keycodes.md). |
| `"ctrl_alt_del"` | Send the Ctrl+Alt+Del secure login sequence. |
| `"sleep"` | Put the PC to sleep (S3 suspend). Requires USB wake support enabled in BIOS to wake with keyboard. |
| `"hibernate"` | Hibernate the PC (saves to disk, full power off). Wake with power button. Requires hibernate enabled in Windows (`powercfg /hibernate on`). |
| `"shutdown"` | Shut down the PC immediately. |

---

## Pairing with Windows

When you first flash the device or change the `passkey`:

1. Open **Bluetooth & other devices** on Windows.
2. If "ESP32 BLE Keyboard" is already listed, **Remove Device**.
3. Click **Add device** -> **Bluetooth**.
4. Select **ESP32 BLE Keyboard**.
5. Windows will prompt you to enter the PIN. Type your configured `passkey` (e.g., `123456`) and click **Connect**.

---

## Troubleshooting

* **Not appearing in search:** Ensure no other device is currently connected. The ESP32 stops advertising once a connection is established.
* **PIN prompt not appearing:** Windows often caches old security profiles. Fully "Remove" the device from Windows Bluetooth settings and try again.
* **Typing speed:** The component includes a 20ms delay between keypresses to ensure the host OS registers them correctly. This can be adjusted in `espidf_ble_keyboard.cpp` if needed.
* **Sleep/Hibernate/Shutdown not working:** These actions open the Windows Run dialog (Win+R) and type a command. Ensure the PC is not in a state where the Run dialog is blocked (e.g., fullscreen app or UAC prompt on screen).
* **PC not waking from sleep:** Check that **USB Wake Support** (or similar) is enabled in your BIOS/UEFI Power Management settings.