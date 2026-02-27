# ESP32 BLE HID Keyboard for ESPHome

This is a custom ESPHome component that transforms an ESP32 into a Bluetooth Low Energy (BLE) HID Keyboard. Unlike many other implementations, this component uses the **direct ESP-IDF Bluedroid GATTS API** for improved reliability and compatibility.

## Features

* **Standard HID Keyboard:** Recognized as a native keyboard by Windows.
* **Secure Pairing:** Supports a configurable 6-digit static passkey (PIN) for secure bonding.
* **Efficient Memory Usage:** Direct API implementation ensures stability even with complex ESPHome configurations.
* **Key Combos:** Send any modifier + key combination using hex keycodes (e.g. Win+R, Ctrl+C).
* **String Typing:** Type any string directly including letters, numbers and punctuation.
* **Pre-defined Actions:** Built-in helpers for `ctrl_alt_del`, `sleep`, `hibernate` and `shutdown`.
* **Media Keys:** Control volume, playback, mute and more via HID consumer control.
* **Power Button:** Native HID power/sleep signals — no Run dialog, clean OS-level control.
* **Consumer Control:** Send any HID consumer code directly from YAML using `consumer:0xXXXX` syntax.
* **Custom Text Input:** Send any text typed in Home Assistant directly to the PC.

📖 [Keycode Reference](docs/keycodes.md) · [🌐 View Web Page](https://markusg1234.github.io/ESPHome-espidf_ble_keyboard)


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

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Mute"
    action: "mute"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Volume Up"
    action: "volume_up"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Volume Down"
    action: "volume_down"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Play / Pause"
    action: "play_pause"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Open Calculator"
    action: "consumer:0x0192"

  - platform: template
    name: "Send Custom Text"
    on_press:
      - lambda: |-
          id(my_keyboard).send_string(id(custom_text).state);

  - platform: restart
    name: ${friendly_name}

text:
  - platform: template
    name: "Custom Text"
    id: custom_text
    mode: text
    optimistic: true

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
* **action** (Required, string or mapping): The action to perform when the button is pressed. Accepts either a string or a dict with `type` key (see below).

#### Action Types

| Action | Description |
|---|---|
| `"Hello\n"` | Type a string. Use `\n` for Enter. Supports letters, numbers and common punctuation. |
| `"combo:0x08:0x15"` | Send a key combination. Format: `combo:<modifier_hex>:<keycode_hex>`. Use `0x00` as modifier for no modifier key. See [Keycode Reference](docs/keycodes.md). |
| `"combo:0x00:0x04"` | Send a plain keypress with no modifier. `0x04` = A, `0x05` = B ... `0x1D` = Z. |
| `"consumer:0x0192"` | Send any HID consumer control code. Format: `consumer:<usage_hex>`. See [Keycode Reference](docs/keycodes.md) for full list. |
| `"ctrl_alt_del"` | Send the Ctrl+Alt+Del secure login sequence. |
| `"sleep"` | HID System Sleep signal — clean OS-level sleep. |
| `"hibernate"` | Hibernate the PC — saves to disk, full power off. Requires `powercfg /hibernate on`. |
| `"shutdown"` | HID System Power Down signal — clean OS-level shutdown. |
| `"power"` | HID power button — triggers Windows power button action. |
| `"mute"` | Toggle mute. |
| `"volume_up"` | Volume up. |
| `"volume_down"` | Volume down. |
| `"play_pause"` | Play / pause media. |
| `"next_track"` | Skip to next track. |
| `"prev_track"` | Previous track. |
| `"stop"` | Stop media playback. |

---

## Dict Action Format

Instead of a string, `action` also accepts a mapping with a `type` key. This can be more readable for complex actions:

```yaml
# Combo — modifier + key
action:
  type: combo
  modifier: 0x01   # 0x00 = none, 0x01 = Ctrl, 0x02 = Shift, 0x04 = Alt, 0x08 = Win
  key: 0x04        # 0x04 = A ... 0x1D = Z, see Keycode Reference

# Plain keypress — no modifier
action:
  type: combo
  modifier: 0x00
  key: 0x04        # Just 'A'

# Consumer control
action:
  type: consumer
  code: 0x0192     # Open Calculator
```

Both formats are equivalent — the dict format is converted to the string format at compile time so there is no runtime difference.

---

## Custom Text Input

You can send arbitrary text from Home Assistant to the PC without hardcoding it in the YAML. Add the following to your ESPHome config:

```yaml
text:
  - platform: template
    name: "Custom Text"
    id: custom_text
    mode: text
    optimistic: true

button:
  - platform: template
    name: "Send Custom Text"
    on_press:
      - lambda: |-
          id(my_keyboard).send_string(id(custom_text).state);
```

This adds a text input field and a send button to Home Assistant. You can also drive it from a Home Assistant automation — for example, updating the text entity from an `input_text` helper and then pressing the button:

```yaml
automation:
  - alias: "Send text via BLE keyboard"
    trigger:
      - platform: state
        entity_id: input_text.ble_keyboard_text
    action:
      - service: text.set_value
        target:
          entity_id: text.bluetooth_keyboard_custom_text
        data:
          value: "{{ states('input_text.ble_keyboard_text') }}"
      - service: button.press
        target:
          entity_id: button.bluetooth_keyboard_send_custom_text
```

> **Note:** Only letters, numbers and common punctuation are supported. Unsupported characters are silently skipped.

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
* **Hibernate not working:** Hibernate uses the Windows Run dialog. Ensure the PC is not in a state where it is blocked (e.g., fullscreen app or UAC prompt). Also ensure hibernate is enabled: run `powercfg /hibernate on` in an admin command prompt.
* **PC not waking from sleep:** Check that **USB Wake Support** (or similar) is enabled in your BIOS/UEFI Power Management settings.
* **Re-pair after firmware update:** If the HID descriptor changes (e.g. after adding media keys), you must remove and re-pair the device in Windows Bluetooth settings.