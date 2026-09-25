# ESP32 BLE HID Keyboard & Remote for ESPHome

This is a custom [ESPHome](https://esphome.io) component that turns an ESP32 into a Bluetooth Low Energy (BLE) HID keyboard and mouse that can switch between up to ten paired hosts. It serves its own web remote and ships Home Assistant dashboard cards. Any key on any host can also fire a Home Assistant action, such as an IR command, so one remote can mix Bluetooth keys and IR on the same page — and a host slot can have Bluetooth turned off entirely to be a pure IR remote, making it a universal remote as well. This component targets **ESP-IDF Bluedroid GATTS** (rather than NimBLE), chosen for the HID behavior and host compatibility validated in this project.

## Features

* **Universal Remote:** A web remote and Home Assistant card, drawn in built-in or your own styles with logos and a small status screen. Any key on any host can fire a Home Assistant action, such as sending an IR command, alongside keys that go over Bluetooth — power over IR and navigation over Bluetooth on the same page, say. A host slot can also have Bluetooth turned off to act purely as a remote page. Styles travel: **Export all** on one keyboard's page and **Import** on another copies every style you have made, logos included, in one paste. See [A slot that never advertises](#a-slot-that-never-advertises) and [Copying styles to another keyboard](#copying-styles-to-another-keyboard).
* **Linked Keyboards:** A second ESP32 running this component, listed under `peers:`, puts its hosts on this one's web page as a bar of their own — tap one and the remote, keyboard, paste box and mouse drive it over Wi-Fi, for hosts one keyboard's Bluetooth can't reach. Macros reach it with `peer:<name>:<action>`, and a Home Assistant card can step through both keyboards' hosts. See [Linking a second keyboard](#linking-a-second-keyboard).
* **Standard HID Keyboard:** Recognized as a native keyboard by Windows, Android, and iOS. Full HOGP-compliant BLE HID with Device Information and Battery services. Use `passkey_mode: legacy` for Windows (Just Works for Android), `passkey_mode: secure_connections` for iOS.
* **Secure Pairing:** Supports a configurable 6-digit static passkey (PIN) for secure bonding on Windows and iOS. Android uses Just Works pairing (no PIN) due to HID compatibility limitations.
* **Efficient Memory Usage:** Direct API implementation ensures stability even with complex ESPHome configurations.
* **Key Combos:** Send any modifier + key combination using hex keycodes (e.g. Win+R, Ctrl+C).
* **String Typing:** Type any string directly. The active **keyboard layout** (`us`, `uk`, `de`, `be`) controls how each character is mapped to HID keycodes. UK adds `£`, `¬`, `€`; DE adds `ä`, `ö`, `ü`, `ß`, `€`, `§`, `°`; BE adds `é`, `è`, `à`, `ç`, `ù`, `€`, `£`, `²`, `§`, `µ` plus dead-key sequences (`â ê î ô û ä ë ï ö ü` + uppercase) via UTF-8.
* **Keyboard Layouts:** Choose `us` (default), `uk`, `de`, or `be` in YAML, or switch live from the web UI (persisted to NVS). Layout is fully extensible — see [Keyboard layouts](#keyboard-layouts).
* **Press and hold (push-to-talk):** Hold a key down on the host for as long as a physical button is held, instead of sending a tap — per key, and per button per host on the web remote. See [Press and hold](#press-and-hold).
* **Pre-defined Actions:** Built-in helpers for `ctrl_alt_del`, `sleep`, `hibernate` and `shutdown`.
* **Media Keys:** Control volume, playback, mute and more via HID consumer control.
* **Power Button:** Native HID power/sleep signals — no Run dialog, clean OS-level control.
* **Consumer Control:** Send any HID consumer code directly from YAML using `consumer:0xXXXX` syntax.
* **Mouse Control:** Left, right, and middle click, cursor movement, and scroll wheel via HID mouse reports.
* **Custom Text Input:** Send any text typed in Home Assistant directly to the paired host device.
* **RSSI Sensor:** Read the signal strength (dBm) of the connected host on a configurable interval. Supports proximity-based automations via `on_rssi_above` / `on_rssi_below`.
* **Host MAC Sensor:** Expose the Bluetooth address of the connected host, so automations can act on *which* machine is connected. Reports the stable identity address, so it holds even on Android and iOS where the connection address rotates.
* **Bonded Slot Protection:** A host slot belongs to its host until you forget it. A stranger that pairs while a bonded slot is active is refused and its bond removed, instead of quietly taking the slot over.
* **Keyboard LED Feedback:** Expose host-side Num Lock, Caps Lock, and Scroll Lock LED state as ESPHome binary sensors. Updated whenever the host writes a HID output report.
* **Battery Level:** Report a real charge percentage over the BLE Battery Service, so the host's Bluetooth settings show it like any other wireless keyboard. Point `battery_level:` at any sensor reading 0–100. See [Battery level](#battery-level).

📖 [Keycode Reference](docs/keycodes.md) · [🌐 View Web Page](https://markusg1234.github.io/ESPHome-espidf_ble_keyboard)


## Usage Example

Add the following to your ESPHome YAML configuration:

> **Versioning:** tagged releases are listed on the [Releases page](https://github.com/markusg1234/ESPHome-espidf_ble_keyboard/releases). `ref: main` always tracks the latest code (re-fetched per ESPHome's [`external_components`](https://esphome.io/components/external_components.html) `refresh:` interval, default 1 day). Pin a tag like `ref: v1.0.0` to stay on a fixed release and upgrade only when you change the ref.

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
  board: esp32dev   # Tested with esp32dev, esp32-c6-devkitm-1, ESP32-C3 and ESP32-C5
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BT_ENABLED: y
      CONFIG_BT_CONTROLLER_ENABLED: y
      CONFIG_BT_BLUEDROID_ENABLED: y
      CONFIG_BT_NIMBLE_ENABLED: n
      CONFIG_BT_BLE_ENABLED: y
      CONFIG_BT_GATTS_ENABLE: y
      CONFIG_BT_BLE_42_FEATURES_SUPPORTED: y
      CONFIG_BT_BLE_50_FEATURES_SUPPORTED: n
      CONFIG_BT_BLE_42_ADV_EN: y
      CONFIG_BT_BLE_42_SCAN_EN: y
      CONFIG_BT_BLE_SMP_ENABLE: y
      CONFIG_BT_ACL_CONNECTIONS: "4"

logger:
  level: INFO

api:
  encryption:
    key: ${api_encryption_key}

ota:
  - platform: esphome
    password: ${ota_password}
    # ESPHome 2026.9.0 or newer: delete `password:` and uncomment `encryption:` to encrypt
    # uploads with the api key above (also ~3.5 KB less flash). Install once on 2026.9 with the
    # password still set first — firmware older than 2026.9 can't receive an encrypted upload.
    # encryption:

wifi:
  ssid: ${wifi_ssid}
  password: ${wifi_password}
  power_save_mode: none   # Keeps the web page quick; `light` sleeps between beacons, which adds a
                          # noticeable delay to the first load after the device has been idle
  fast_connect: true

external_components:
  - source:
      type: git
      url: https://github.com/markusg1234/ESPHome-espidf_ble_keyboard.git
      ref: main            # or pin a release tag, e.g. v1.0.0
      path: components
    refresh: 0s
    components: [ espidf_ble_keyboard ]

# Required by `web_control: true` below — it hosts the control page.
web_server:
  port: 80
  # Without this the page is open to everything on your network, and it types
  # on whatever computer is paired. See "Securing the web control page".
  # auth:
  #   username: !secret web_username
  #   password: !secret web_password

espidf_ble_keyboard:
  id: my_keyboard
  # Optional: BLE device name shown during pairing (max 29 chars, default: "ESP32 BLE KB")
  device_name: "ESP32 BLE KB"
  # Optional: per-character delay when typing strings in ms (default: 80)
  key_delay_ms: 80
  # Optional: Set a 6-digit pairing code.
  # If omitted, the device will use "Just Works" (no PIN) pairing.
  # Note: Android does not support passkey pairing for BLE HID devices.
  passkey: 123456
  # Optional pairing mode when passkey is set:
  # legacy (default, Windows-friendly) or secure_connections (iOS-required)
  passkey_mode: legacy
  # Optional: enable built-in web control page at http://<device-ip>/ble_keyboard
  # Requires web_server component. No HA cards or services needed.
  web_control: true
  # Optional: number of host slots for multi-host switching (1–10, default: 4)
  host_slots: 4
  # Optional: web mouse sensitivity settings
  mouse_sensitivity: 2.0       # base movement speed (default: 1.0)
  mouse_acceleration: 2.0     # speed-based acceleration factor (default: 0.15)
  mouse_max_speed: 4.0         # max sensitivity cap (default: 4.0)
  scroll_sensitivity: 2.0      # scroll speed multiplier (default: 2.0)
  # Optional: screen geometry for absolute pointer (mouse_abs / mouse_abs_px / mouse_abs_mon)
  screen_width: 1920           # pixel space the host maps onto (default: 1920)
  screen_height: 1080          # for multi-monitor, set to the whole virtual desktop
  # Optional: monitor regions (virtual-desktop pixels) for mouse_abs_mon:<idx>:<x%>:<y%>
  # monitors:
  #   - { name: left,  x: 0,    y: 0, width: 1920, height: 1080 }
  #   - { name: right, x: 1920, y: 0, width: 1920, height: 1080 }
  # Optional: link text entities for custom text input (shows Send button in web UI)
  custom_text_id:
    - custom_text
  # Optional: per-slot passkey, pairing mode, keyboard layout, and action overrides
  hosts:
    - slot: 0
      passkey: 111111
      passkey_mode: legacy
      layout: us           # auto-apply this layout whenever slot 0 becomes active
      actions:
        record: "combo:0x0C:0x15"   # this host is a PC: Record drives Game Bar
    - slot: 1
      passkey: 222222
      passkey_mode: legacy
      layout: uk           # ...and this one for slot 1
    - slot: 2
      passkey_mode: legacy
    - slot: 3
      passkey_mode: legacy

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
    name: "Play ⁄ Pause"
    action: "play_pause"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Open Calculator"
    action: "consumer:0x0192"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Left Click"
    action: "left_click"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Move Mouse Right"
    action: "mouse_move:50:0"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Scroll Down"
    action: "mouse_scroll:-3"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Cursor to Center"
    action: "mouse_abs:50:50"        # exact position, percent of screen

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Click Corner & Return"
    action: "mouse_abs_save | mouse_abs:5:5 | left_click | mouse_abs_restore"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Send Custom Text"
    action: "send_custom_text"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 0"
    action:
      type: switch_host
      slot: 0

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 1"
    action:
      type: switch_host
      slot: 1

  - platform: restart
    name: ${friendly_name}

text:
  - platform: template
    name: "Custom Text"
    id: custom_text
    mode: text
    optimistic: true

# Optional: lets the Home Assistant remote card mirror the web UI's
# per-host button hiding. Omit it and the card just shows every button.
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: hidden_buttons
    name: "Hidden Buttons"

binary_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "BLE Keyboard Paired"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: caps_lock
    name: "BLE Keyboard Caps Lock"

  - platform: status
    name: ${friendly_name}
```

## Configuration Variables

### `espidf_ble_keyboard`

* **id** (Required, ID): The ID used to link buttons or automations to this keyboard.
* **device_name** (Optional, string): The BLE device name advertised during pairing. Defaults to `ESP32 BLE KB`. Maximum 29 characters.
* **key_delay_ms** (Optional, int): Total delay per character when typing strings, in milliseconds. Split evenly between key-down and key-up. Defaults to `80`. Increase if characters are being dropped on slow BLE connections.
* **max_key_hold_ms** (Optional, int): Safety net for a held key whose release never arrives — a browser tab closed mid-press, or an `on_release` that didn't fire. After this many milliseconds the device releases everything it is holding, including a held mouse button. Defaults to `0` (never auto-release); otherwise 100–600000. See [Press and hold](#press-and-hold).
* **passkey** (Optional, int): A 6-digit static PIN (000000–999999). If set, the device uses static passkey pairing (legacy MITM bond) and requires this PIN during initial pairing.
* **passkey_mode** (Optional, string): Passkey security mode. `legacy` (default) uses legacy MITM bonding — tested and recommended for Windows. `secure_connections` uses LE Secure Connections MITM bonding — required for iOS passkey pairing (legacy mode does not work on iOS). Android does not support passkey pairing with BLE HID keyboards.
* **web_control** (Optional, bool): Enable a built-in web control page with keyboard and mouse UI at `http://<device-ip>/ble_keyboard`. Requires the `web_server` component. Defaults to `false`. The page and its endpoints are **open to anything that can reach the device** unless you give `web_server:` an `auth:` block — see [Securing the web control page](#securing-the-web-control-page).
* **web_host_check** (Optional, bool): Refuse requests addressed to a host name this device does not answer to. An IP address, any `.local` name, and any name with no dot in it (a short DHCP host name) all pass; add anything else with `web_allowed_hosts`. Defaults to `true`. This is what stops a website re-pointing its own domain at your device and having your browser drive the keyboard from there, so leave it on unless a reverse proxy needs otherwise. See [Securing the web control page](#securing-the-web-control-page).
* **web_allowed_hosts** (Optional, string or list): Extra host names that count as this device — a reverse proxy's domain, a DNS entry, whatever your setup uses. Names here are trusted as fully as the device's own address, so list only ones you control. Ignored when `web_host_check` is `false`.
* **web_allow_framing** (Optional, bool): Allow the page to be shown inside a frame — an `iframe` card on a dashboard, for instance. Defaults to `false`, because a framed page is still on its own origin: every same-origin check the device makes is satisfied while the click that triggered it belongs to whoever built the frame. Turn it on only for a dashboard you run yourself.
* **peers** (Optional, list): Other keyboards running this component that this one's web page can drive over Wi-Fi — see [Linking a second keyboard](#linking-a-second-keyboard). Up to 4, each with a `name` (1–15 of `a-z`, `0-9`, `_`), a `url` (`http://<IP address>[:port]` or `http://<name>.local`, no path) and, when that keyboard's `web_server:` has a login, its `username` and `password`. Needs `web_control: true`.
* **api_services** (Optional, bool): Auto-register all documented Home Assistant services (`run_action`, `run_macro`, `send_string`, `send_key`, `send_consumer`, `mouse_move`, `mouse_scroll`, `mouse_click`, `mouse_hold`, `mouse_release`, `mouse_abs`, `set_battery_level`, `switch_host`, `forget_host`) directly from the component — no `api: services:` yaml needed, and the HA cards work out of the box. Requires the `api:` component. Defaults to `false`. **Don't combine with the manual `api: services:` snippets below** — you'd register the same service names twice; delete the manual copies when enabling this. See [Home Assistant services](#home-assistant-services).
* **ha_action** (Optional, bool): Allow the `ha_action:` prefix to fire Home Assistant actions from the device — how a remote key reaches things BLE can't, such as an IR blaster's `remote.send_command`. Requires the `api:` component (`api: homeassistant_services: true` is enabled automatically) and Home Assistant's own per-device permission. Defaults to `false` — the web page is unauthenticated unless you set that up, so this is a deliberate opt-in. See [Calling Home Assistant Actions](#calling-home-assistant-actions).
* **host_slots** (Optional, int): Number of host slots for multi-host switching (1–10). Each slot can store a bonded host. Switch between hosts using buttons, HA services, or the web control page. Defaults to `4`.
* **mouse_sensitivity** (Optional, float): Web mouse base movement multiplier. Defaults to `1.0`. Range: 0.1–10.0.
* **mouse_acceleration** (Optional, float): Web mouse speed-based acceleration factor. Defaults to `0.15`. Range: 0.0–2.0.
* **mouse_max_speed** (Optional, float): Web mouse maximum sensitivity cap. Defaults to `4.0`. Range: 0.5–20.0.
* **scroll_sensitivity** (Optional, float): Web mouse scroll speed multiplier. Defaults to `2.0`. Range: 0.1–10.0.
* **screen_width** / **screen_height** (Optional, int): The pixel space the host maps the absolute pointer's `0..32767` range onto, used by `mouse_abs_px` and `mouse_abs_mon`. For a single screen, set to its resolution; for a spanned multi-monitor setup, set to the whole **virtual desktop** size. Defaults to `1920` / `1080`. Range: 1–32767. See [Absolute mouse positioning](#absolute-mouse-positioning).
* **monitors** (Optional, list): Per-monitor regions (in virtual-desktop pixels) for `mouse_abs_mon:<idx>:<x%>:<y%>`. Each entry has optional `name`, required `x`, `y`, `width`, `height`, and optional `primary` (mark the Windows primary monitor — its top-left is the Windows `0,0` origin that `mouse_goto` homes to, and the web Position Finder needs it to emit correct `mouse_goto` values). See [Absolute mouse positioning](#absolute-mouse-positioning).
* **mouse_goto_scale** (Optional, float): Calibration multiplier for `mouse_goto`'s relative step, to compensate for the host's pointer-speed / DPI scaling — sets **both** axes. Defaults to `1.0`. If the cursor travels about **twice** as far as intended, set `0.5`; tune until a `mouse_goto` lands on target. (Requires "Enhance pointer precision" **off** — acceleration is non-linear and can't be calibrated out.) Range: 0.05–20.0.
* **mouse_goto_scale_x** / **mouse_goto_scale_y** (Optional, float): Per-axis override of `mouse_goto_scale`. X and Y often need **different** values (a host can scale the axes differently), so calibrate each independently. Range: 0.05–20.0. Easiest to dial in live via the web Position Finder, which also saves the values per host. See [Absolute mouse positioning](#absolute-mouse-positioning).
* **custom_text_id** (Optional, ID or list of IDs): Link one or more ESPHome `text` entities for custom text input. Automatically registers a "Send" button in the web UI for each. Use `send_custom_text` or `send_custom_text:N` action to trigger.
* **expose_buttons** (Optional, boolean): List every non-internal ESPHome `button` in your config on the [web control page](#pressing-other-esphome-buttons), so it can reach things BLE can't — Wake-on-LAN, a relay, a restart. Defaults to `true`.
* **hide_buttons** (Optional, ID or list of IDs): Buttons to keep *off* the web page. Anything listed there can be pressed by whoever can reach the device — and unless you have set up authentication, that is anyone on the network — so use this for anything destructive (`factory_reset`, `restart`, `safe_mode`). Hidden buttons also refuse to run if their action is typed by hand.
* **sources** (Optional, list): Entities a remote style may read — to show on an [LCD panel](#lcd-panels), to branch on with [`if:`](#branching-on-real-state), or to light a button with `lit:`. Each entry names exactly one of `sensor:`, `text_sensor:`, `text:` or `binary_sensor:` by id, plus an optional `key:` (what the style calls it — defaults to the entity's id) and, for a numeric sensor, `unit:` and `decimals:` overrides. A `binary_sensor:` publishes the literal `on`/`off`. At most 8.
* **battery_level** (Optional, ID): A sensor whose value (0–100) is published over the BLE Battery Service, so the host's Bluetooth settings show the keyboard's real charge. Any sensor reading a percentage will do — an ADC with a calibration filter, a fuel-gauge IC, a template sensor. Values outside 0–100 are clamped and an unavailable reading is ignored rather than sent as 0%. Without this the service is still advertised and reports a fixed 100%. See [Battery level](#battery-level).
* **keyboard_layout** (Optional, string): Default keyboard layout. One of `us` (default), `uk`, `de`, `be`. Controls how `send_string` maps each character to USB HID keycodes — must match the *host's* keyboard layout. Can be overridden at runtime from the web UI (persisted to NVS, survives reboot). See [Keyboard layouts](#keyboard-layouts) below.
* **hosts** (Optional, list): Per-slot passkey and pairing mode overrides. Each entry has:
  * **slot** (Required, int): Host slot number (0–9).
  * **passkey** (Optional, int): 6-digit PIN for this slot (000000–999999). If omitted, the slot uses the global `passkey` setting (or Just Works if no global passkey).
  * **passkey_mode** (Optional, string): `legacy` (default) or `secure_connections`. Overrides the global `passkey_mode` for this slot.

### `button` (Platform: `espidf_ble_keyboard`)

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **action** (Required, string or mapping): The action to perform when the button is pressed. Accepts either a string or a dict with `type` key (see below).

### `binary_sensor` (Platform: `espidf_ble_keyboard`)

The binary_sensor platform supports four types via the `type` key:

#### Paired Sensor (default)

Reports whether the keyboard has completed BLE pairing with a host on the current connection.

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **type** (Optional, string): `paired` (default).
* **slot** (Optional, 0–9): Follow one host slot instead of whichever is active. ON only while that slot's host is connected and paired, so it tells you whether a host that drops Bluetooth when it sleeps (a TV or monitor in standby) is awake — as long as it is the active slot. Declare one per slot you want to watch.
* **name** (Optional, string): Friendly entity name shown in Home Assistant.

State behavior:

* **ON** = a `GAP: Pairing Successful` event occurred on the current connection (with `slot:`, only if that connection belongs to the slot).
* **OFF** = keyboard is disconnected (including host-side unpair) or not yet paired in this session.

#### LED State Sensors (Num Lock / Caps Lock / Scroll Lock)

Expose the host-side keyboard LED state, as reported by the connected host via the HID output report. Updates within one loop cycle of the host changing the lock state.

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **type** (Required, string): One of `num_lock`, `caps_lock`, `scroll_lock`.
* **name** (Optional, string): Friendly entity name shown in Home Assistant.

State behavior:

* **ON** = the corresponding lock LED is currently lit on the host.
* **OFF** = the lock is off, no host is connected, or the host hasn't sent an LED report yet.

```yaml
binary_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: num_lock
    name: "BLE Keyboard Num Lock"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: caps_lock
    name: "BLE Keyboard Caps Lock"

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: scroll_lock
    name: "BLE Keyboard Scroll Lock"
```

Note: LED state reflects what the *host* thinks the lock state is. The sensors go OFF when a host disconnects and follow the next one once it sends its LED report. The `caps_lock` sensor also lights the [keyboard card](#keyboard-control-card-for-home-assistant)'s Caps key.

#### Caps Lock and typed text

While the host reports Caps Lock on, typed text still comes out as written: letters are sent with the opposite Shift, which Windows, Android, Linux and ChromeOS turn back into the case asked for. This covers macros, `send_string`, paste, and the on-screen keyboards, whose keys also show capitals while it is on. **macOS and iPadOS** don't let Shift undo Caps Lock, so text there still comes out in capitals — start the macro with `caps_lock:off`.

### `sensor` (Platform: `espidf_ble_keyboard`)

The sensor platform supports two types via the `type` key:

#### RSSI Sensor (default)

Exposes the RSSI (signal strength) of the currently connected host as an ESPHome sensor entity.

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **type** (Optional, string): `rssi` (default).
* **name** (Optional, string): Friendly entity name shown in Home Assistant.
* **update_interval** (Optional, duration): How often to read RSSI from the connected host. Default: `10s`.

State behavior:

* Publishes the RSSI value in **dBm** (e.g. `-65`) while a host is connected.
* Publishes **unavailable** when the host disconnects.

```yaml
sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "BLE Host RSSI"
    update_interval: 15s
```

#### Active Host Sensor

Publishes the currently active host slot number (0-based). Updates instantly when the host is switched from the webserver, HA card, or YAML automation.

Optional, and only relevant if you use the [card host switchers](#host-switcher-on-the-cards). They already stay in sync by polling the device every 30 seconds; this sensor makes that instant, and becomes the *only* sync path when the cards can't reach the device directly (Home Assistant on HTTPS).

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **type** (Required, string): `active_host`.
* **name** (Optional, string): Friendly entity name shown in Home Assistant.

```yaml
sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: active_host
    name: "BLE Keyboard Active Host"
```

The cards auto-detect this entity by name pattern (`sensor.*_active_host`). If auto-detection fails, set `active_host_entity` in the card config:

```yaml
type: custom:ble-keyboard-card
device: bluetooth_keyboard
host_slots: 4
active_host_entity: sensor.bluetooth_keyboard_active_host
```

#### Proximity Automations

Use `on_rssi_above` and `on_rssi_below` on the main `espidf_ble_keyboard` component to trigger actions based on signal strength. Both fire on every RSSI sample that crosses the threshold — add your own debounce logic (e.g. a `script` or `globals` flag) if needed.

| Key | Description |
|---|---|
| `threshold` | RSSI value in dBm (−127 to 0). `on_rssi_above` fires when RSSI > threshold. `on_rssi_below` fires when RSSI < threshold. |

The automation receives a single `rssi` variable (int, dBm) you can use in lambdas.

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  on_rssi_above:
    threshold: -65      # fires when host is close (strong signal)
    then:
      - logger.log:
          format: "Host nearby (RSSI %d dBm)"
          args: [rssi]
  on_rssi_below:
    threshold: -90      # fires when host moves far away (weak signal)
    then:
      - logger.log:
          format: "Host far away (RSSI %d dBm)"
          args: [rssi]
```

> **Tip:** Typical indoor RSSI values range from around −40 dBm (very close) to −90 dBm (far/weak). A threshold of −70 to −75 is a reasonable starting point for proximity detection.

#### Action Types

| Action | Description |
|---|---|
| `"Hello\n"` | Type a string. Use `\n` for Enter. Printable ASCII is supported on all layouts; non-ASCII (e.g. `£ ¬ €` on UK) is supported via UTF-8 when a layout exposes it. Characters with no layout mapping are silently skipped. |
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
| `"play"` / `"pause"` | Play (`0x00B0`) or pause (`0x00B1`) only — one-way, so a macro can pause without risking a resume. |
| `"next_track"` | Skip to next track. |
| `"prev_track"` | Previous track. |
| `"stop"` | Stop media playback. |
| `"record"` | Start / stop recording (HID Record, `0x00B2`). Works on TV / DVR hosts; **Windows ignores it** — remap it per host with [`actions:`](#host-actions-per-host-overrides), e.g. `"combo:0x0C:0x15"` for Game Bar. |
| `"rewind"` | Rewind (`0x00B4`). |
| `"fast_forward"` | Fast forward (`0x00B3`). |
| `"remote_power"` | Remote Power key — HID consumer Power (`0x0030`). Distinct from `"power"` above, which is a System Power Down report. |
| `"up"` / `"down"` / `"left"` / `"right"` | D-pad navigation — HID Menu Up / Down / Left / Right (`0x0042`–`0x0045`). |
| `"ok"` | D-pad select — keyboard **Enter** (`0x28`). Enter is accepted by far more hosts than HID Menu Pick; if a host needs Menu Pick instead, override it per host with `"consumer:0x0041"`. |
| `"home"` | AC Home (`0x0223`). |
| `"back"` | AC Back (`0x0224`). |
| `"search"` | AC Search (`0x0221`). |
| `"info"` | AC More Info / Guide (`0x0209`). |
| `"channel_up"` / `"channel_down"` | Channel surf — Page Up / Page Down keypress. |
| `"brightness_up"` / `"brightness_down"` | Screen brightness (`0x006F` / `0x0070`) on hosts that drive their own screen — phones, tablets, laptops. An external monitor ignores it. |
| `"color_red"` / `"color_green"` / `"color_yellow"` / `"color_blue"` | Coloured remote keys — F1–F4, as most media apps expect. |
| `"app_explorer"` / `"app_browser"` / `"app_email"` / `"app_calc"` | App launch keys (`0x0194`, `0x0223`, `0x018A`, `0x0192`). |
| `"menu"` / `"exit"` | Menu (`0x0040`) and Menu Escape (`0x0046`) — the hamburger and back-out keys a set-top remote has. |
| `"guide"` / `"tv"` | Programme Guide (`0x008D`) and Media Select TV (`0x0089`). |
| `"voice"` | Voice Command (`0x00CF`) — the microphone key. |
| `"captions"` | Closed Caption (`0x0061`) — subtitles on/off. |
| `"num0"` … `"num9"` | The keypad, as plain keyboard digits — direct channel entry on a TV, typing a number on a PC. |
| `"backspace"` | Keyboard Backspace — correcting a digit or a TV search box. |
| `"caps_lock"` | Tap Caps Lock. |
| `"caps_lock:on"` / `"caps_lock:off"` / `"caps_lock:toggle"` | Set the host's Caps Lock. On and off tap it only when the host reports it the other way, then wait for the host to confirm; a host that has never reported it is left alone. |
| `"prev_host"` / `"next_host"` / `"last_host"` | Remote keys for `switch_host:prev`, `switch_host:next` and `switch_host:back` below. |
| `"next_keyboard"` / `"prev_host_all"` / `"next_host_all"` | Web page remote only: move the tab to the next [linked keyboard](#linking-a-second-keyboard), or step through every host of every linked keyboard. Anywhere else they do nothing. |
| `"spare1"` … `"spare32"` | Send **nothing** on their own. They exist as names to hang a [per-host override](#host-actions-per-host-overrides) on, for remote keys with no standard HID usage worth guessing — an app launcher, a set-top box's Input, a vendor's own menu. Pressing an unmapped one logs a hint and does nothing. |
| `"left_click"` | Mouse left click. |
| `"right_click"` | Mouse right click. |
| `"middle_click"` | Mouse middle click. |
| `"mouse_click:0x01"` | Mouse click with button mask. `0x01` = left, `0x02` = right, `0x04` = middle. Combine for simultaneous buttons. |
| `"left_click_hold"` | Press **and hold** the left button until `mouse_release`. Moving, scrolling or `mouse_goto` while held performs a drag. |
| `"right_click_hold"` | Press and hold the right button. |
| `"middle_click_hold"` | Press and hold the middle button. |
| `"mouse_hold:0x01"` | Press and hold with a button mask (same masks as `mouse_click`). |
| `"mouse_release"` | Release all held mouse buttons. A normal click also releases them. |
| `"key_hold:0x00:0x3A"` | Press **and hold** a key until `release` — the host sees it held down, not tapped. Format matches `combo:`. A keycode of `0x00` holds the modifier alone (e.g. `key_hold:0x01:0x00` holds Ctrl). See [Press and hold](#press-and-hold). |
| `"consumer_hold:0x00E9"` | Hold a consumer usage. Only one can be held at a time — the report has a single usage field. |
| `"hold:<action>"` | Hold whatever `<action>` is: `combo:`, `consumer:`, a mouse click, or a named action including one remapped by [`actions:`](#host-actions-per-host-overrides). Anything that can't be held (text, macros, `switch_host:`) runs once instead. |
| `"release"` | Release everything held — keys, consumer usage and mouse buttons. `key_release` is an alias. |
| `"mouse_move:<x>:<y>"` | Move mouse cursor. Values -127 to 127 (relative, pixels). |
| `"mouse_scroll:<wheel>"` | Scroll mouse wheel. Positive = up, negative = down (-127 to 127). |
| `"mouse_abs:<x%>:<y%>"` | Move cursor to an **exact** position, percent of screen (0–100, decimals allowed). E.g. `mouse_abs:50:50` = center. See [Absolute mouse positioning](#absolute-mouse-positioning). |
| `"mouse_abs_px:<x>:<y>"` | Move cursor to an exact position in **pixels** (uses `screen_width`/`screen_height`). |
| `"mouse_abs_mon:<idx>:<x%>:<y%>"` | Move cursor to a percent within declared `monitors[idx]` (multi-monitor). |
| `"mouse_abs_save"` | Remember the current absolute position (the one this device last set). |
| `"mouse_abs_restore"` | Jump back to the last `mouse_abs_save` position. |
| `"mouse_goto:<x>:<y>"` | Move to a **Windows virtual-desktop pixel** across **all monitors** (homes the absolute pointer to the desktop origin, then steps relatively). X/Y are Windows coordinates (primary monitor top-left = 0,0; screens left of it are negative). Use this when the absolute pointer is confined to the primary monitor. Needs "Enhance pointer precision" **off** and a fixed pointer-speed slider position (the per-axis calibration is tied to it) for pixel accuracy. |
| `"switch_host:N"` | Switch to host slot N (0–9). Reconnects to stored host or advertises for new pairing. |
| `"switch_host:next"` / `"switch_host:prev"` | Step to the next or previous host slot, wrapping around at the ends. Cycles through every configured slot, so an unpaired one is reached too (and advertises for pairing). Does nothing when only one slot is configured. |
| `"switch_host:back"` | Return to the host slot that was active before the last switch, however that switch was made. Pressed again, it goes back again. |
| `"host_action:N:<name>"` | Run host slot N's Host Action for `<name>` without switching to it. If slot N has no action for that name, it runs as an ordinary press on the active host. |
| `"peer:<name>:<action>"` | Run `<action>` on a [linked keyboard](#linking-a-second-keyboard), exactly as its own page would — `peer:bedroom:volume_up`, `peer:bedroom:switch_host:1`. |
| `"wait:connected"` / `"wait:connected:N"` | Pause a macro until the active host is connected and ready for keys, for at most N ms (default 10000, max 60000). On timeout the macro carries on. |
| `"forget_host:N"` | Remove BLE bond for host slot N (0–9) and clear the slot. |
| `"lcd:<text>"` | Put text on an [LCD panel](#lcd-panels)'s `@msg` line. Everything after the colon is the text. |
| `"if:<source>: <when on> \|\| <when off>"` | Run one branch or the other depending on a [source](#branching-on-real-state). Does nothing until the source has a state. |
| `"string:hello"` | Explicit text typing — useful in multi-step macros to distinguish text from action names. |
| `"delay:N"` | Pause for N milliseconds (max 10000). Used between steps in multi-step macros. |
| `"repeat:N:<action>"` | Run `<action>` N times (max 1000). Put it at the start of a macro to repeat the whole sequence, e.g. `repeat:3:combo:0:40 \| delay:200`. |
| `"send_custom_text"` | Send the first linked text entity's content. Requires `custom_text_id` in config. |
| `"send_custom_text:N"` | Send the Nth linked text entity (0-based). E.g. `send_custom_text:1` for the second. |

> **Some TV-remote keys do nothing on some hosts.** `menu`, `exit`, `captions`, `tv`, `guide` and `voice` are standard HID Consumer Page usages, but hosts implement that page unevenly — one that ignores a usage simply does nothing when its button is pressed. That is what [per-host overrides](#host-actions-per-host-overrides) are for, and it is the same reason `ok` sends Enter rather than Menu Pick.

**From a YAML automation, without a lambda** — any trigger (`on_press`, `on_value`, `on_time`, …) can
run an action string directly:

```yaml
- espidf_ble_keyboard.run_action:
    id: my_keyboard
    action: "macro:Copy-Paste"      # or any action above, multi-step included
```

`action:` is templatable, so a lambda can pick the string when you want one. See
[Triggering macros from YAML](#triggering-macros-from-yaml).

**Lambda helpers** (for automations already inside a lambda):

| Method | Description |
|--------|-------------|
| `execute_action("action_string")` | Run any action string from a lambda. Works with all action types above. Supports multi-step with `\|`. |
| `execute_macro("name")` | Run a web-defined macro by name. Returns `false` if no macro has that name. Prefer this over the index form — a name survives deletions. |
| `execute_macro(index)` | Run a web-defined macro by index (0-based, shown as [0], [1] in web UI). Returns `false` if index is out of range. Deleting a macro shifts every later index. |

---

### `text_sensor` (Platform: `espidf_ble_keyboard`)

Optional. Six types are available.

* **keyboard_id** (Required, ID): The ID of the `espidf_ble_keyboard` component.
* **type** (Optional, string): `hidden_buttons` (default), `host_mac`, `hold_buttons`, `repeat_buttons`, `remote_style` or `lcd`.
* **name** (Optional, string): Friendly entity name shown in Home Assistant.

#### Hidden buttons

Publishes the active host's hidden remote buttons so the [Media Remote Card](#media-remote-card-for-home-assistant) can mirror the web remote's [per-host button removal](#removing-remote-buttons-per-host). Without it the card simply shows every button; the web remote does not need it either way.

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: hidden_buttons
    name: "Hidden Buttons"
```

The state is a comma-separated list of action names — `record,app_calc,color_red` — or empty when the active host hides nothing. It republishes when you save in the Host Actions card and whenever the host is switched.

#### Press-and-hold buttons

Publishes the active host's [Press and hold](#press-and-hold-per-host) list, so the Media Remote Card holds those buttons down instead of tapping them. **Without it the card cannot do push-to-talk at all** — it has no other way to learn the per-host choice, because a dashboard served over https can't fetch the device's REST API.

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: hold_buttons
    name: "Hold Buttons"
```

Same format as the hidden list: `volume_up,ok`, or empty when nothing holds on this host.

#### Hold-to-repeat config

Publishes the active host's [Hold to repeat](#hold-to-repeat-per-host) settings, so the card repeats the same buttons at the same speed as the web remote. **Without it the card falls back to repeating volume and channel only**, ignoring whatever Host Actions says.

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: repeat_buttons
    name: "Repeat Buttons"
```

The state is `<delay>,<rate>,name,name` — `400,180,volume_up,volume_down` — or empty when this host was never configured, which tells the card to keep its own defaults. A host configured to repeat nothing publishes just `400,180`.

#### Host MAC

Publishes the Bluetooth address of the connected host, so an automation can tell *which* machine it is talking to. The [active host sensor](#active-host-sensor) only reports the slot number, which is not the same thing — see [protecting a bonded host slot](#protecting-a-bonded-host-slot).

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: host_mac
    name: "Host MAC"
```

The state is `AA:BB:CC:DD:EE:FF`, or empty while nothing is connected. Use it to gate an automation on a specific machine:

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  on_rssi_above:
    threshold: -65
    then:
      - if:
          condition:
            lambda: 'return id(host_mac).state == "04:CB:01:07:D2:24";'
          then:
            - logger.log: "My phone is nearby"
```

Where the host supplied an identity key when pairing, this is its **identity address** rather than the address it happened to connect with. That matters on Android and iOS, which connect using a private address that rotates roughly every 15 minutes: the identity address does not rotate, so a comparison like the one above keeps working. Hosts with a fixed address — Windows PCs, most TVs — report the same value either way.

The web remote's host buttons and the cards' MAC display show this same identity address, so every place an address appears agrees with the sensor and with what the phone reports for itself. A backup still records the address the host connected with, since that is what restoring a slot needs.

Each slot learns its host's identity the next time that host connects, and remembers it from then on. A host paired before this existed keeps showing its old address until it next connects — you do not need to pair it again. This matters because the identity can only be looked up while the host is connected: once a phone has rotated away from the address its slot was recorded under, nothing can map that slot back on its own.

The value is only as stable as the bond. Unpair and pair again and a phone may present a different identity, so re-check the sensor after re-pairing rather than assuming the old value still holds. Treat this as identification, not authentication — it tells one device from another, but it is not proof against a device that deliberately imitates one.

#### LCD values

Publishes the values an [LCD panel](#lcd-panels) shows, as compact JSON, so a panel in a remote style works on the Media Remote Card and not just on the web page. Only needed if a style has one.

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: lcd
    name: "LCD Values"
```

It carries the keyboard's own `@` values and anything listed in `sources:`, republished whenever one of them changes and at most once a second. Everything has to fit the 255 characters a Home Assistant state holds. Past that, whole entries are dropped rather than half of one: your `sources:` keep their place, and `@host`, `@slot` and `@mac` are first to go because the card works those out from its own host list — declaring any sources at all pushes those three out, and nothing is lost by it. The log names anything else it had to leave out. The device's own page is never cut: it reads the full set from `/status`.

> **These are diagnostic entities.** They carry machine-readable state for the cards rather than anything to read yourself, so they default to `entity_category: diagnostic` — Home Assistant files them under **Diagnostic** on the device page and leaves them out of auto-generated dashboards, instead of listing a long comma-separated repeat config across the integration screen. Set `entity_category:` on the sensor to promote one back to the main list. Add only the sensors your cards actually use; each is optional.

> ESPHome text sensors appear in Home Assistant under the **`sensor.`** domain, not `text_sensor.`. With the YAML above the entities are `sensor.<device>_hidden_buttons`, `sensor.<device>_hold_buttons`, `sensor.<device>_repeat_buttons`, `sensor.<device>_remote_style`, `sensor.<device>_lcd` and `sensor.<device>_host_mac`. Those names are what the cards auto-detect; if you give one a different `name`, set the matching `hidden_entity:` / `hold_entity:` / `repeat_entity:` / `lcd_entity:` on the card.

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

# Mouse click
action:
  type: mouse_click
  buttons: 0x01    # 0x01 = left, 0x02 = right, 0x04 = middle

# Mouse move
action:
  type: mouse_move
  x: 50            # move 50px right
  y: -20           # move 20px up

# Mouse scroll
action:
  type: mouse_scroll
  wheel: 3         # scroll up 3 notches (negative = down)

# Absolute move — exact position, percent of screen
action:
  type: mouse_abs
  x: 50            # 50% across
  y: 50            # 50% down (center)

# Absolute move — exact pixels (uses screen_width / screen_height)
action:
  type: mouse_abs_px
  x: 1280
  y: 720

# Absolute move — percent within a declared monitor
action:
  type: mouse_abs_mon
  monitor: 1       # index into the `monitors:` list
  x: 50
  y: 50

# Switch host
action:
  type: switch_host
  slot: 1             # switch to host slot 1

# Switch host — cycle
action:
  type: switch_host
  slot: next          # or prev; wraps around at the ends

# Forget host
action:
  type: forget_host
  slot: 2             # remove bond for host slot 2
```

Both formats are equivalent — the dict format is converted to the string format at compile time so there is no runtime difference.

---

## Home Assistant Services

Set `api_services: true` and the component registers every documented Home Assistant service by itself — no `api: services:` yaml to paste, and all three HA cards (mouse, keyboard, media remote) work out of the box:

```yaml
api:
  encryption:
    key: !secret api_key

espidf_ble_keyboard:
  id: my_keyboard
  api_services: true
```

Services appear in HA under **Developer Tools → Actions** as `esphome.<device_name>_<service>`:

| Service | Variables | Description |
|---------|-----------|-------------|
| `run_action` | `action: string` | Run any [action string](#usage-example) — single or multi-step with `\|`. Reaches everything below plus every named action. |
| `run_macro_name` | `name: string` | Run a stored web macro by name. The stable choice — unlike an index, a name doesn't move when another macro is deleted. |
| `run_macro` | `index: int` | Run a stored web macro by its index ([0], [1], … in the web UI). Deleting a macro shifts every later index. |
| `send_string` | `keys: string` | Type text (used by the keyboard and remote cards). |
| `send_key` | `modifier: int`, `keycode: int` | Send a key combination (HID modifier + keycode). |
| `send_consumer` | `code: int` | Send a HID consumer control code. |
| `mouse_move` | `x: int`, `y: int` | Relative cursor move (−127…127). |
| `mouse_scroll` | `amount: int` | Scroll wheel (−127…127). |
| `mouse_click` | `btn: int` | Click with button mask (1 = left, 2 = right, 4 = middle). |
| `mouse_hold` | `btn: int` | Press and hold for dragging — release with `mouse_release`. |
| `mouse_release` | — | Release all held mouse buttons. |
| `mouse_abs` | `x: float`, `y: float` | Move cursor to an exact position, percent of screen (0–100). |
| `set_battery_level` | `level: int` | Set the battery percentage the host sees (0–100). See [Battery level](#battery-level). |
| `switch_host` | `slot: int` | Switch to host slot N (0–9). |
| `forget_host` | `slot: int` | Remove the bond for host slot N (0–9). |

Requirements and notes:

* Requires the `api:` component (config validation fails with a clear error without it). The component automatically enables `api: custom_services: true` for you (needed by ESPHome 2025.11+ for dynamically registered services).
* **Don't combine with manual `api: services:` definitions of the same names** — if you previously pasted the per-card snippets below, delete them when enabling `api_services: true`, or the service names collide.
* The manual snippets in the card sections below remain fully supported as the "custom" path — use them if you want different service names, extra validation, or only a subset.

---

## Multi-Host Switching

The keyboard supports up to 10 bonded hosts and can switch between them on the fly — like commercial keyboards with a host-switch button. Each host slot stores the bonded device address in NVS (persistent across reboots).

### How It Works

1. **Pair your first host** — it is automatically saved to slot 0.
2. **Switch to an empty slot** (e.g. slot 1) — the keyboard disconnects and starts advertising. Pair a new host; it is saved to that slot.
3. **Switch back** — the keyboard disconnects from the current host and uses directed advertising to reconnect to the stored host. The target host reconnects automatically (no re-pairing needed).

Each host slot uses a unique BLE address, so other bonded hosts won't interfere during pairing.

Switching takes 1–3 seconds depending on the host OS.

### YAML Configuration

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  host_slots: 4          # 1–10, default: 4

button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 0"
    action:
      type: switch_host
      slot: 0

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 1"
    action:
      type: switch_host
      slot: 1

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 2"
    action:
      type: switch_host
      slot: 2

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Host 3"
    action:
      type: switch_host
      slot: 3

  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Forget Host 0"
    action:
      type: forget_host
      slot: 0
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Forget Host 1"
    action:
      type: forget_host
      slot: 1
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Forget Host 2"
    action:
      type: forget_host
      slot: 2
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Forget Host 3"
    action:
      type: forget_host
      slot: 3
```

String action format is also supported: `"switch_host:0"`, `"forget_host:2"`.

**Cycling instead of naming a slot.** `"switch_host:next"` and `"switch_host:prev"` step one slot forward or back and wrap around at the ends, so one button rotates through the hosts:

```yaml
button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Next Host"
    action:
      type: switch_host
      slot: next          # or prev
```

The rotation covers every slot up to `host_slots`, including ones nothing is paired to yet — landing on an empty slot advertises for new pairing, exactly as `switch_host:N` on that slot would. With `host_slots: 1` it does nothing rather than dropping the link and re-advertising.

From Home Assistant, the `switch_host` service takes a slot number only; reach the cycling form with `run_action` and the action string `switch_host:next`.

**Visiting another host and coming back.** `switch_host:back` returns to whichever slot was active before the last switch, and `wait:connected` holds a macro until the new host has actually reconnected — which takes a few seconds and varies by host, so a fixed `delay:` is a guess:

```
switch_host:3 | wait:connected | play_pause | switch_host:back
```

Keys sent before a host is ready are lost, so put `wait:connected` after every switch that is followed by keys. If the host never connects, the wait gives up after 10 seconds (`wait:connected:20000` for longer) and the rest of the macro still runs, so the `switch_host:back` at the end brings the keyboard home either way. A slot with Bluetooth turned off never connects, so the wait returns straight away there.

**The remote keeps its style throughout.** A host switch made inside an action — a macro, a per-host override, a remote key — re-skins the remote only once that action has finished, so one that comes back leaves the [style](#remote-style-per-host) and the hidden, hold and repeat lists exactly as they were, and one that stays on the new host re-skins when it ends. The host bar still marks the host keys are going to while it runs. Switching by hand, from the host bar or Home Assistant's `switch_host` service, re-skins straight away.

> Run such macros from the web page, a remote key or Home Assistant's `run_action`, which all use the keyboard's own action task. The YAML `espidf_ble_keyboard.run_action` automation action runs on ESPHome's main loop, which stalls for as long as the wait lasts.

### A Slot That Never Advertises

Not every slot has to be a Bluetooth host. Untick **Advertise over Bluetooth** in the [Host Actions](#host-actions-per-host-overrides) card and the slot keeps everything that makes a slot useful — its own remote style, per-host overrides, hidden/hold/repeat lists, a place in the host switcher — but the radio stays silent on it. Nothing advertises, nothing can find it, nothing can connect.

What you get is a remote page with no host behind it: a universal-remote page whose buttons drive Home Assistant instead. Override them with `ha_action:` and they reach an IR blaster, a media player, a scene — anything HA can call. Or leave the keyboard where it is and [show the slot's page on one tab](#remote-style-per-host) — a tablet whose keys run this slot's Host Actions whichever host is active.

> **HID actions on such a slot go nowhere.** There is no host to send them to, so a button you have not overridden — Volume Up, the D-pad, a `string:` step — does nothing at all. Every button that should work on the page needs an override that leaves via Home Assistant.

* The setting is stored per slot on the device, so changing it needs no reflash, and it travels in [Backup & Restore](#backup-and-restore).
* Switching *to* the slot disconnects whichever host was live. That is the point: otherwise keys would keep reaching the old host while the screen says you are on the IR page.
* `switch_host:next` does **not** skip it — reaching the page is why it exists.
* It reports **No BLE** wherever a connection state is shown: the web page's host bar, the dashboard cards, and an `@state` line on an [LCD panel](#lcd-panels). "Disconnected" would read as a fault.
* Tick it back on and the slot is an ordinary host again; it advertises straight away, and a host bonded to it earlier reconnects.

### Protecting a Bonded Host Slot

Once a slot is bonded to a host, only that host can hold it. A different device that pairs while that slot is active is turned away: its bond is removed, it is disconnected, and the slot keeps its original host. To hand a slot to a different machine, forget it first.

This matters most on Android, which [cannot use a passkey with a BLE HID keyboard](#pairing-with-android) — pairing is unauthenticated, so anyone who scans for Bluetooth devices can attempt to pair. Without this, whoever paired last took over the active slot, and any automation keyed on the [active host sensor](#active-host-sensor) would have been none the wiser, because the slot number does not change when the device behind it does.

A few things worth knowing:

* **The refusal happens just after pairing, not instead of it.** The other device briefly shows as paired on its own screen before being dropped. That is expected — it is only at that point that the keyboard learns who connected. It ends up with no usable bond and cannot reconnect.
* **Your own host is not locked out.** Hosts are matched by identity, so a phone returning on a rotated address, or re-pairing after you unpaired it, is recognised as the slot's owner and let straight back in. No forget needed.
* **A slot with no live bond is still free to take.** After restoring a backup, or after a stale bond is cleared, the slot holds an address but no pairing keys — it accepts a new host as before, since refusing would leave no way back in. The slot picker marks these; such a host needs pairing again from its own side.
* **Rejections are logged.** A warning naming the refused address and the slot it tried to take is the only notification you get, so check the logs if a device unexpectedly will not pair.

### Host Switching from Home Assistant

Easiest: set `api_services: true` on the component — it auto-registers `switch_host` and `forget_host` (see [Home Assistant services](#home-assistant-services)). To define them manually instead:

```yaml
api:
  services:
    - service: switch_host
      variables:
        slot: int
      then:
        - lambda: |-
            id(my_keyboard).switch_host(slot);
    - service: forget_host
      variables:
        slot: int
      then:
        - lambda: |-
            id(my_keyboard).forget_host(slot);
```

### Web Control

When `web_control: true` is enabled, a full control page is available at `http://<device-ip>/ble_keyboard` with **keyboard, mouse, Position Finder, remote, buttons, macro, and host action** sections. Section toggle buttons in the toolbar let you show/hide (and reorder) each section. When `host_slots` > 1, a host bar appears below the toolbar showing all slots. Click a slot to switch. The active slot is highlighted. Occupied slots show the stored Bluetooth address. The **Position Finder** (locked by default; click **Edit** to use) sends the cursor to an exact spot and calibrates `mouse_goto` per host — see [Absolute mouse positioning](#absolute-mouse-positioning).

<img src="docs/web_server.png" width="427" alt="Web Control Page">

The page is stored and served gzip-compressed, the same way ESPHome's own web UI serves its
dashboard, so it costs about 73 KB of flash instead of 243 KB and loads a good deal faster. Every
browser handles that transparently; a command-line client has to ask for it, so use
`curl --compressed http://<device-ip>/ble_keyboard` if you want to read the page as text.

**Keys on the on-screen keyboard repeat when held**, the same way they do on a real keyboard: hold a
key — about a fifth of a second with a mouse, about half a second with a finger, since a tap by hand
is much longer than a click — and the device leaves it down on the host, which then applies its own
repeat delay and rate. So the speed follows whatever that machine's keyboard settings say rather
than anything set here, and it differs per host — which is usually what you want. A shorter press is
ordinary typing and behaves exactly as it always has. Three things do not repeat: Shift, Ctrl, Alt,
Win and AltGr (they are sticky toggles here, and a long press on Shift locks it instead), Caps Lock,
and dead keys such as `^` or `` ` `` on the German layout — those are two keystrokes, so there is no
single key to hold down. They still type once.

**Caps Lock follows the host.** When the host turns Caps Lock on — from this page, a card or a real
keyboard — a **CAPS** tag appears beside the connection badge, the Caps key lights and the letters
show capitals. What gets typed matches the labels either way; see
[Caps Lock and typed text](#caps-lock-and-typed-text).

> If the browser vanishes mid-press — a phone locking, a tab killed — the page normally still lifts
> the key, and a lost press is released after 15 seconds. `max_key_hold_ms` is the device-side
> backstop for the rest; it defaults to `0`, meaning a held key is never released on its own.

### Host Actions (Per-Host Overrides)

Paired hosts rarely agree on what a key should do. The clearest example is **Record**: the `record` action sends HID consumer usage `0x00B2`, which an Android TV box or DVR handles natively — but **Windows ignores it completely**. Windows routes Play/Pause/Next/Prev through SystemMediaTransportControls (which is why media keys work on a YouTube tab) and volume straight to the audio endpoint, but nothing subscribes to Record. On a PC the working route is a key combo: Game Bar's `Win+Alt+R`, or whatever global hotkey you bind in OBS or Audacity.

Rather than pick one behaviour, remap the action **per host**. Add an `actions:` mapping to any entry in the `hosts:` list — the same Record button then does the right thing on whichever host is active:

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  host_slots: 4
  hosts:
    - slot: 0                            # Windows PC
      actions:
        record: "combo:0x0C:0x15"        # Win+Alt+R — Game Bar start/stop
    - slot: 1                            # Android TV — no actions:, keeps HID 0x00B2
```

The replacement can be any action string, including a multi-step chain: `record: "combo:0x0C:0x15 | delay:500 | string:recording"`.

**A second real case — the OK button.** Hosts disagree about how "select" arrives. `ok` sends keyboard Enter, which most hosts accept, but some set-top boxes want HID Menu Pick instead, and a few Samsung/Tizen inputs respond to Space. The symptom is distinctive: the D-pad arrows navigate perfectly while OK does nothing, because the arrows and OK use different HID pages. Fix it on the host that needs it:

```yaml
    - slot: 2                            # set-top box that wants Menu Pick
      actions:
        ok: "consumer:0x0041"            # or "combo:0:44" for Space
```

**Rules and limits:**

- **Every button on both remotes is remappable.** All of them — D-pad, Power, Channel, Rewind/FF, the colour keys and app launchers — fire named actions for exactly this reason. The one exception is the number pad, which types digits rather than sending a fixed HID code.
- Only **named** actions can be overridden (`record`, `up`, `channel_up`, `play_pause`, …) — the ones in the [Action Types](#action-types) table with no `:` parameter. Parametric forms like `combo:` and `consumer:` are dispatched before the override lookup, so they always mean exactly what they say. That's deliberate: `consumer:0x00B5` must never silently become something else.
- Resolution order is **web-UI override → YAML `actions:` → built-in behaviour**.
- Max 48 overrides per host slot, and 8000 characters of overrides across all hosts together — they are kept in memory, and the shared limit stops a few full hosts from running the keyboard out of it. Names are max 31 characters and may not contain `=`, `|`, or whitespace; replacements are max 255 characters.
- An override body is executed with overrides disabled, so `record: "record"` safely runs the built-in Record rather than looping.
- Overrides apply everywhere the named action is used — remote buttons, macros, YAML `button` actions, and the `run_action` HA service — not just the remote.

**A second action on a long press.** Name an override `<key>@long` — `back@long` → `home` — and holding that key for half a second on the web remote or the remote card runs it, while a tap still does the key's usual job. Keys with one show a small dot. They send when released rather than when pressed, so the two can be told apart, and they don't repeat; a key in the [Press and Hold](#press-and-hold-per-host) list keeps holding, and its long action is ignored. It works on a tab showing another host's page and on a linked keyboard, which uses its own. The remote card learns these keys from the `hold_buttons` sensor, where they appear as `<key>@long`. Spares take one like any other key (`spare5@long`), and an [LCD panel](#lcd-panels)'s `@last` and `@station` show a long press as the key's label marked *(long)*.

**Reusing a macro:** picking a macro from the Host Actions preset dropdown inserts `macro:<name>` — a **reference**, not a copy. Edit the macro afterwards and every override pointing at it follows automatically. Because the link is by name, renaming a macro breaks it: the override then logs "no macro named …" and does nothing rather than silently running something else. Macro names must be unique and cannot contain `|`.

A dangling reference is flagged: any override row pointing at a macro that no longer exists gets a red **⚠** whose tooltip names the missing macro. It updates live, so renaming or deleting a macro immediately marks the rows that referred to it.

> Overrides created before v1.5.0 hold a *copy* of the macro's text and keep working unchanged — they just don't track edits. Re-pick the macro from the dropdown to turn one into a reference.

**Editing without a reflash:** the web UI has a **Host Actions** card. Pick a host slot, then add or edit overrides for it; they persist to NVS and win over the YAML value. The action-name box offers every overridable name as you type, and the replacement box has the same preset dropdown as the macro editor, so neither has to be typed from memory. Rows tagged `YAML` come from your config and are read-only — set an override of the same name to shadow one, and delete that override to fall back. You can edit a slot other than the one currently active.

<img src="docs/host_actions.png" width="420" alt="Host Actions card in the web UI">

**Forget Host** sits next to the slot picker and removes the BLE bond for whichever slot the picker shows — including one that isn't the active host. It takes two taps: the first turns it red and reads `Confirm?`, the second does it, and it disarms itself after three seconds or if you change slot.

**Advertise over Bluetooth** is a tick under the picker, and it too applies to the slot shown rather than the active one. Untick it to turn that slot into [a remote page with no host behind it](#a-slot-that-never-advertises).

**When this host connects** holds one action the slot runs each time its host connects and is ready for keys — after a switch, a reboot or waking from sleep. Anything an override can do works here, chains and macros included, e.g. `consumer:0x00E9 | delay:500 | macro:Open Kodi`. It doesn't run again for the same host within 30 seconds, nor while a macro is visiting that host, so two hosts whose actions switch to each other stop after one round. Saved on the device like the overrides and included in [Backup and restore](#backup-and-restore); a slot that doesn't advertise never connects, so its box is disabled. Fixed automations belong in YAML instead: the `paired` binary sensor and the `active_host` sensor already fire on every connect and switch.

A line under the card's heading shows the keyboard's free RAM — now, the lowest since it started, and the largest block — and how much storage is left for macros, styles, icons and pairing keys. It updates once a minute.

The same card also hosts the [Backup & Restore](#backup-and-restore) buttons, the [Remote Buttons](#removing-remote-buttons-per-host) hiding panel, the [Hold to Repeat](#hold-to-repeat-per-host) panel and the [Press and Hold](#press-and-hold-per-host) panel.

### Removing Remote Buttons Per Host

Most hosts need only a fraction of the remote. A TV box has no use for Explorer/Calc/Email, a PC has no use for the coloured DVB keys. Open **Remote Buttons** in the Host Actions card, pick the host, and untick whatever that host doesn't need — the buttons disappear from the remote for that host only.

This is **presentation only**. The action still runs from macros, YAML `button` actions and the `run_action` service, so a macro calling `record` still records even with the Record button hidden. Removing a button declutters the remote; it does not disable anything.

The number pad on the Home Assistant card is the one thing not listed, since its digits type characters rather than firing a named action. Note also that hiding `search` removes both the magnifier and the app row's Search button, because both fire the same action.

Hidden sets are stored per host on the device (max 96 buttons per slot) and are included in [Backup and restore](#backup-and-restore).

**Making the Home Assistant card follow too** is optional and needs one extra entity — the card can't read the device's HTTP API, so the list travels through Home Assistant:

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: hidden_buttons
    name: "Hidden Buttons"
```

The card picks up `sensor.<device>_hidden_buttons` automatically (override with `hidden_entity:` on the card) and hides the same buttons, following host switches live. Without the text sensor the card simply shows everything.

> Home Assistant caps entity states at 255 characters. Hiding nearly every button on a host exceeds that, so the device truncates the published list at a whole-name boundary and logs a warning — a couple of buttons would stay visible on the card, though the web remote is unaffected. Normal-sized sets are nowhere near the limit.

### Hold to Repeat Per Host

Holding a button on the **web remote** makes it fire again and again, the way a real remote ramps the volume or scrolls a menu. A quick tap still sends exactly one press.

Out of the box the D-pad, Volume, Channel, Rewind and Fast Forward repeat; everything else fires once. Open **Hold to Repeat** in the Host Actions card to change that per host — tick the buttons that should repeat, set how long a button must be held before it starts (**Start after**, default 400 ms) and how fast it repeats after that (**Repeat every**, default 180 ms). **Reset** returns the host to the defaults above.

Different hosts want different answers: a TV wants a fast volume ramp, while a PC may want only the D-pad repeating and volume left alone so a long press can't run away with the mixer.

| | Range | Default |
|---|---|---|
| Start after | 100–2000 ms | 400 ms |
| Repeat every | 50–2000 ms | 180 ms |

Values outside those ranges are clamped rather than rejected. The 50 ms floor is not arbitrary — the device drops an identical press that arrives within 30 ms of the last one (the duplicate guard that stops Home Assistant's double-delivered service calls from firing twice), so a faster repeat would silently lose events.

Each repeat is a normal press, so per-host overrides apply to it. One consequence worth knowing: if a button is overridden to something that already loops, such as `repeat:3:volume_up`, holding it multiplies the two.

Settings are stored per host on the device (max 96 buttons per slot), not in the browser, so they follow the host rather than the phone that set them, and they are included in [Backup and restore](#backup-and-restore). The Home Assistant [Media Remote Card](#media-remote-card-for-home-assistant) follows the same settings if you add the [`repeat_buttons` text sensor](#hold-to-repeat-config); without it that card repeats volume and channel only.

A button set to **Press and Hold** below cannot also repeat: holding and repeating are the same gesture, so each panel greys out what the other has taken.

### Press and Hold Per Host

Open **Press and Hold** in the Host Actions card and tick the buttons that should stay **held down** on the host for as long as you hold them on the web remote, instead of sending a tap — what push-to-talk needs. Stored per host slot (max 96 buttons), so the PC running the voice app can hold while a TV slot keeps tapping, and included in [Backup and restore](#backup-and-restore).

Nothing holds by default. The Home Assistant [Media Remote Card](#media-remote-card-for-home-assistant) honours the same list once the [`hold_buttons` text sensor](#press-and-hold-buttons) is added — it is the only way that card can learn the choice. See [Press and hold](#press-and-hold) for the physical-button equivalent, the action strings, and `max_key_hold_ms`.

### Remote Style Per Host

The web remote can be drawn in a different **style** per host, so switching to a media box brings up a compact remote shaped for it and switching back to the PC brings back the full one. Open **Remote Style** in the Host Actions card, pick the host, and step through the styles with **−** and **+**. Each press saves straight away, and changing the active host's style redraws the remote as you go — so the quickest way to find the one you want is to press **+** until it looks right. The list wraps, so you can reach the far end from either direction. The name between the buttons is a **dropdown**: open it to see every style at once and pick one directly, which is quicker once there are a few of your own. The **This tab** row below works the same way.

| Style | What it shows |
|---|---|
| **Full remote** | Everything, exactly as before styles existed. The default for every host. |
| **Style 1** | Slab: power, search, D-pad, back/home/menu, transport, volume, channel and an app row. |
| **Style 2** | Power, D-pad, back/home/play, volume and channel rockers, mute. Suits a television. |
| **Style 3** | Compact strip: power, mute, volume and the full transport row. Good for a headless box. |
| **Style 4** | The full set-top shape: number pad, colour keys, nav ring, back/home/TV, one-piece VOL·mute·CH rockers and four app pills. Dark body. |
| **Style 5** | Style 4's layout on a pale body — the only light style, and easier to read on a bright screen. |
| **Style 6** | A slab with a nav ring, rockers, a **[screen](#lcd-panels)** and eight **[logo](#logos-on-buttons)** keys. The screen names the host and says whether it is connected, which needs nothing configured. Each spare names an icon — `netflix`, `youtube`, `prime`, `disney`, `spotify`, `plex`, `kodi` — plus the built-in `tv`; until you import an icon under one of those names, its key shows the label. |

Styles 4 and 5 take their key arrangement from [HA-Firemote](https://github.com/PRProd/HA-Firemote) (GPL-3.0). The buttons, icons and renderer are this project's own.

**Light and dark mode.** A style's colours do **not** follow the page's light/dark toggle; only **Full remote**, which sets none, does. Styles 4 and 5 are the same remote in dark and light. In your own styles, **set all colours or none** — any you leave out follow the page and flip underneath the rest.

> Styles 4 and 5 carry [spare buttons](#action-reference) for the keys that have no standard code — Input, Mark, Set and the four app pills. They send nothing until you give them a per-host override, which is what lets one App pill launch something different on each machine.

The built-in styles are numbered rather than named after particular devices: the shapes are generic, and a number can't suggest a tie to anyone's product. Your own styles can be called whatever you like.

The style is stored on the device against the host slot, not in the browser, so it follows the host rather than the phone that set it — and every browser watching the page re-skins within a few seconds of a host switch, whoever made it. It is **presentation only**: the actions a style leaves out still run from macros, YAML buttons and Home Assistant, and the **Remote Buttons**, **Hold to Repeat** and **Press and Hold** panels always list every action regardless of which style is showing, so they can be set for a host before you ever look at its remote. Styles are included in [Backup and restore](#backup-and-restore).

**Keeping one tab on one style.** The **This tab** row in **Remote Style** keeps just the tab you are in on one style, whatever host is active — a wall tablet showing the same remote all day while a PC tab beside it follows the host. It saves nothing on the device: it adds `?style=` and the style's id to the tab's address, so a reload keeps it, and bookmarking the address (or adding it to a tablet's home screen) makes it stick. You can type the address yourself too, e.g. `http://<device>/ble_keyboard?style=style6#remote` — the id is `default`, `style1`…`style6` or one of your own, and a popped-out remote takes it along. The active host's hidden, repeat and hold settings still apply, and the host bar still switches hosts. It is a view, not a restriction: anyone at that tab can step — or pick — it back to **Follow the host**.

**Showing a host's page.** The same row lists your hosts too. Pick one and the tab shows that host's *page*: its style, its Remote Buttons, Hold to Repeat and Press and Hold settings, and its [Host Actions](#host-actions-per-host-overrides) on every key — while the keyboard stays on whichever host is active. A key that host has no action for goes to the active host as usual, so a volume rocker beside the programmed keys still turns the TV up. This is what a [slot that never advertises](#a-slot-that-never-advertises) is for on a tablet: give it a style and program its keys under Host Actions, then open `http://<device>/ble_keyboard?host=<slot>` there (slots count from 0, as in `switch_host:`). Other tabs are unaffected, and a host switch made anywhere leaves the tablet's keys doing what you set.

The [Media Remote Card](#media-remote-card-for-home-assistant) draws from the same style definitions, so a layout looks the same in both places — see [Remote styles on the card](#remote-styles-on-the-card) for how a style travels there.

#### Making your own

Press **Export** to drop the style currently shown into the box below as JSON, edit it, and press **Import**. Importing over an id that already exists replaces it; a new id adds a style. The device holds **6** custom styles of up to **1500 characters** each.

**Import also takes a whole Export all list**, so a set of styles moves between keyboards in one paste — see [Copying styles to another keyboard](#copying-styles-to-another-keyboard). Every style in the list is checked, and given its slot, before any of them is written: one that would not render, or a list with nowhere to put its new styles, leaves the device untouched and says which style is the problem.

The remote card **redraws as you type**, so the layout is visible before it is saved — the preview lives in the browser, and nothing reaches the device until you press Import. A trailing comma is forgiven; any other syntax error names the line it is on and puts the cursor there.

```json
{
  "id": "lounge",
  "name": "Lounge box",
  "theme": { "bg": "#12161c", "radius": "28px", "maxw": "240px", "btn_bg": "#1e242e" },
  "sections": [
    ["row", "remote_power", "|", "search", "mute"],
    ["dpad"],
    ["row", "back", "home"],
    ["media", "rewind", "play_pause", "fast_forward"],
    ["strip", ["Vol", "volume_up", "volume_down"]]
  ]
}
```

`id` is 1–15 characters of `a-z`, `0-9` or `_` and cannot be one of the built-in ids. Each entry in `sections` is an array starting with its kind:

| Kind | Renders |
|---|---|
| `["row", …]` | A centred row of round buttons. `"\|"` inserts a stretching gap, which is what pushes Power to the far left. |
| `["dpad"]` | A square D-pad cluster. List five actions — `["dpad","up","left","ok","right","down"]` — to substitute your own. |
| `["ring"]` | The same five keys as a **circular navigation ring** with a centre button, which is what most modern remotes have. Takes the same optional five actions. An optional settings object first sizes it: `["ring", {"size": 200, "center": 96}]` — the ring's diameter, 120–320 px (168 by default), and the centre button's, 40–200 px (84), leaving 40 px each side for the arrows. Arrows and centre take button options such as `sm` or `md` and stay centred |
| `["strip", ["Vol","volume_up","volume_down"], …]` | Labelled vertical columns side by side. The first entry of each group is its label; `""` for none. |
| `["rocker", ["Vol","volume_up","volume_down"], …]` | **One-piece rocker keys** — a tall pill with two halves and the label between them, as a remote carries volume and channel. A two-entry group, `["","mute"]`, is a single key at the same height, which is how mute sits between two rockers. |
| `["media", …]` | A row of the smaller transport-sized buttons. |
| `["apps", …]` | A row of wide pill buttons. |
| `["grid", {"cols":2,"h":56,"opts":"sq"}, …]` | **Equal rectangles in columns**, the layout said once so each key is just `["spare1","Copy"]` — a page of 32 named keys fits a stored style easily. The settings are optional: `cols` 1–8 (2 if left out), `h` 24–160 pixels, and `opts`, the [button options](#making-your-own) every key takes. A key's own options add to those; `"|"` leaves a cell empty. |
| `["lcd", ["Room","temp"], …]` | **A small screen** showing live values — see [LCD panels](#lcd-panels). Each line is a label and the value to show; up to four per panel. |
| `["-"]` | A horizontal divider. |

**Colouring and sizing a button.** A third element carries appearance tokens, space-separated:

```json
["row", ["spare1","Netflix","#e50914 wide"], ["keyboard","Kbd","light sm"]]
```

| Token | Effect |
|---|---|
| `#rrggbb` | the button's own colour |
| `light` | an inverted key — light face, dark glyph, as remotes use for Home and app buttons |
| `sm` / `md` / `lg` / `xl` | 36 / 42 / 56 / 64 px instead of the usual 48 |
| `wide` | an auto-width pill |
| `sq` | square-ish corners |
| `squircle` | an app tile — a rounded square whose corners flow into its sides, as TV launchers draw apps. Pairs with a size: `"lg squircle"` |
| `fill` | share the row's width equally with the other `fill` keys in it, so a grid of keys lines up in columns whatever the labels say. The width comes from the style's `maxw` |
| `h:<px>` | the key's height in pixels, 24–160. With `fill` it makes rectangles: `["row", ["spare1","TV on","fill h:80 sq"], ["spare2","TV off","fill h:80 sq"]]` |
| `ih:<px>` | the height of the key's icon in pixels, 8–160, instead of the size the key gives it. Never taller than the key; a wide key grows to fit a long logo |
| `lit:<source>` | Light the key while that [source](#configuration-variables) reads `on` — e.g. a power key that goes green while the TV is on. Takes the style's `lit_bg`/`lit_fg`; `lit:<source>:#43a047` colours this one button instead. |
| `icon:<name>` | Put an icon on the key — a logo you imported, or one of the remote's own by its id (`icon:home`). See [Logos on buttons](#logos-on-buttons). |

An unknown token is refused on import rather than ignored, so a typo shows up rather than silently doing nothing.

**Giving a button its own label.** Write it as `["action", "Label"]` instead of a bare action name and the label replaces the button's face, while the tooltip still names the action underneath. Pair that with a per-host override and a key both reads and does what you want:

```json
["row", ["spare1", "Netflix"], ["spare2", "iPlayer"], "voice"]
```

Labels are 1–16 characters. A round key fits about four; the wide app pill fits more. Spares are the natural partner here — they send nothing until you give them an override on that host.

Buttons are named by action — any name from the [Action Reference](#action-reference) table below that the remote knows (`remote_power`, `search`, `info`, `mute`, `home`, `back`, the D-pad five, `volume_*`, `channel_*`, `brightness_*`, the seven transport keys plus `play` and `pause`, `color_*`, `app_*`, `menu`, `guide`, `voice`, `captions`, `tv`, `num0`–`num9`, `backspace`, `prev_host`, `next_host`, `last_host`, `next_keyboard`, `prev_host_all`, `next_host_all`, `spare1`–`spare32`). An unknown name is refused on import rather than rendering a dead button.

**Shaping the body.** `theme` is optional. Colours: `bg`, `border`, `btn_bg`, `btn_fg`, `btn_border`, `ok_bg`, `ok_fg`, `ring_bg`, `ring_fg`, `light_bg`, `light_fg`, `label`, `divider`, for a [panel](#lcd-panels) `lcd_bg`, `lcd_fg`, `lcd_label`, `lcd_border`, and for a `lit:` button `lit_bg`, `lit_fg`. Geometry: `pad`, `maxw`, `radius`, `btn_radius`, `shadow`, `clip`, `zoom`, `lcd_radius`. Anything else is ignored, so an imported style cannot restyle the rest of the page.

**Making the buttons bigger or smaller.** `zoom` scales the whole remote — buttons, their icons and labels, the gaps between them, the d-pad and the rockers — by one factor: `"zoom": "1.25"` for a quarter larger, `"0.8"` for smaller. It is the only size control, deliberately: the buttons come in several sizes that are tuned against each other and against the gaps, so scaling them as a set keeps a layout that was designed to fit still fitting.

> `maxw` is measured before the zoom is applied, so it scales with everything else — a `maxw` of `250px` at `"zoom": "1.25"` draws 312px wide. Divide it by the zoom if you want to keep the same overall width and let the buttons grow into it instead: `"maxw": "200px"` at `"zoom": "1.25"` is 250px overall with buttons a quarter larger.

Three of those do more than they look:

- **`bg` takes a gradient**, not just a colour — it feeds the CSS `background` shorthand, so `"linear-gradient(180deg,#2b2b33,#141418)"` gives the body depth. Same for `btn_bg`.
- **`radius` takes the whole CSS grammar**, including the two-axis `/` form. That is what makes a rounded-end stick or a teardrop pebble: `"46% 46% 26% 26% / 24% 24% 8% 8%"`.
- **`clip`** accepts a `polygon()` for a genuinely tapered body, e.g. `"polygon(0% 0%, 100% 0%, 82% 100%, 18% 100%)"`. It is validated as a polygon and nothing else.

`url()` is refused anywhere in a theme — an imported style must not be able to make the page fetch from another host.

> `ring_fg` exists because a nav ring is often the opposite tone to the rest of the remote — a white ring on a black body, a black one on alloy. Without it the arrows inherit `btn_fg` and disappear.

Deleting a custom style leaves the hosts using it on the full remote; re-importing it under the same id puts them all back.

#### Logos on buttons

A key can show a logo instead of a word. Open **Button Icons** in the Host Actions card and paste the **text** of the logo's `.svg` file — open it in Notepad and copy all of it, from `<svg` to `</svg>` — or drag the file onto the box. Give it a name and press **Import**. The page previews it on a dark and a light key, round and wide, before anything is saved. Then name it in a button's tokens:

```json
["apps", ["spare1", "Netflix", "icon:netflix lg sq"], ["spare2", "Spotify", "icon:spotify"]]
```

`lg sq` gives the logo a bigger, squarer key; `wide` suits a long wordmark. A logo that still reads small takes `ih:<px>` to draw it larger without changing the key — `icon:ten xl ih:52`. The label stays as the tooltip, and is what the key shows if the icon is ever missing — so a style never draws a blank key.

**Style 6** is this written out: pick it, then **Export** to see every key's `icon:` token. Import logos named `netflix`, `youtube`, `prime`, `disney`, `spotify`, `plex` or `kodi` and they appear on its keys.

The SVG is converted in the browser into plain shapes before it is stored, and only numbers, path commands and colours survive. Nothing in a pasted file can run or load anything: scripts, event handlers, embedded images and links are discarded, and the stored shapes are checked again every time they are drawn.

* **Kept:** paths and basic shapes, fills and strokes, transforms, even-odd holes, line caps and joins, opacity, and `<use>` references.
* **Simplified:** a gradient becomes one of its colours. The preview shows the result, so check it before importing.
* **Dropped:** text (convert it to outlines first), embedded pictures, shadows and other filters, clipping and masks.
* **A shape with no colour of its own** draws in the key's text colour, which is right for a one-colour glyph.

The device holds **16** icons of up to **4200 characters** each once converted. Tested against 415 streaming-service logos, 393 fitted and 22 were too detailed; the importer rounds coordinates to make one fit before refusing it. Icons are stored apart from styles, and only their names are kept in memory — each one is read from storage when a page asks for it.

**Export takes the icons along.** A style exported with Export or Export all carries the icons it uses, so pasted into the Home Assistant card it draws its logos without the card needing to reach the device. Imported on another keyboard, it adds them to that keyboard's icons.

Deleting an icon leaves its keys showing their labels; importing one under the same name brings the logos back.

> Imported logos are your own copies. None are shipped with the firmware.

#### LCD panels

A style can carry a small screen alongside its buttons. Each line is a label and the value to show:

```json
["lcd", ["Room", "temp"], ["Host", "@host", "lg"], ["Now Playing", ""]]
```

A panel holds up to eight lines, and a style can have more than one. A line with no value is a title. Labels are up to 16 characters.

**Sizing the panel.** Put a settings object in front of the lines and the panel is specified the way a real character display is — so many characters across, so many lines down:

```json
["lcd", {"cols": 16, "rows": 2}, ["Room", "temp"], ["", "now_playing"]]
```

| Key | Effect |
|---|---|
| `cols` | Width in **characters**, 4–40. The panel becomes exactly that many characters of its own monospace face, and a value too long for it is cut off with an ellipsis rather than wrapping — as a real display does. |
| `rows` | Height in **lines**, 1–8. The panel reserves that height whether or not the lines are there, so it stops changing size as its values do. It is also how many lines that panel may hold. |
| `fg` `bg` `label` `border` | `#rrggbb` colours for this panel alone, overriding the style's `lcd_*` theme keys — which is how one style carries a green screen and an amber one. |

Every key is optional. Without `cols` the panel fills the remote's width and wraps long values; without `rows` it grows with its content and holds up to eight lines.

```json
["lcd", {"cols": 20, "rows": 2, "fg": "#ffb000", "bg": "#1a1206"},
        ["Temp", "temp"], ["", "@state"]]
```

**Sizing and justifying a line.** A third element carries tokens, space-separated, the same way a button's do — and an unknown one is refused on import rather than ignored:

| Token | Effect |
|---|---|
| `sm` / `lg` / `xl` | 11 / 18 / 24 px instead of the usual 14 |
| `left` / `centre` / `right` | Push the line's text to that side. `center` spells the same thing. |
| `#rrggbb` | the value's own colour |

Left to itself a line **spreads**: the label sits against the left edge and the value against the right, which is how a real panel reads. An alignment token packs the label and value together and moves the pair as one — so `centre` on a labelled line centres the pair, not the value inside the panel. On a line with no label there is nothing to spread against, so it sits left until you say otherwise:

```json
["lcd", ["Room", "temp"], ["", "now_playing", "centre"], ["", "@state", "sm right"]]
```

A title line centres by default, since it has no value to sit opposite; give it `left` or `right` to move it.

**Values beginning `@` are the keyboard's own and need nothing configured:**

| Key | Shows |
|---|---|
| `@host` | The active host's name — its `switch_host:` button's name if it has one, otherwise "Host 1", "Host 2"… |
| `@slot` | The active slot number, counting from 0, the same number `switch_host:N` takes. |
| `@mac` | The active host's address, or nothing if the slot is empty. |
| `@state` | `Paired`, `Connected` or `Disconnected`. |
| `@rssi` | Signal strength in dBm. Dashes until the first reading arrives. |
| `@battery` | The percentage the host sees. See [Battery level](#battery-level). |
| `@layout` | The active keyboard layout id. |
| `@last` | The last button pressed, named as the current style labels it — a spare labelled `Netflix` shows "Netflix", not `spare1`. Changes on every press. |
| `@station` | The same, but only the spares update it, so a volume tap doesn't wipe which station you chose. |
| `@msg` | Whatever an `lcd:` action last wrote. |

All three are held in memory only and start blank, so a panel shows `--` until something is pressed.

**Writing your own text.** The `lcd:<text>` action puts anything you like on an `@msg` line — everything after the colon is the text, colons included. Chain it with whatever the key really does:

```
consumer:0x0223 | lcd:Netflix
```

Put that in a spare's [per-host override](#host-actions-per-host-overrides) and the panel names where you are. Up to 64 characters.

**Anything else names an entity you list on the component.** The device reads it and formats it — unit and decimals included — so `21.4 °C` needs no format string anywhere:

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  sources:
    - sensor: lounge_temperature     # key defaults to the entity's id
    - key: temp                      # or name it yourself
      sensor: lounge_temperature
      unit: "°C"                     # optional; defaults to the sensor's own
      decimals: 1                    # optional; defaults to the sensor's own
    - text_sensor: now_playing
    - key: msg
      text: my_text_input
    - key: monitor
      binary_sensor: monitor_on     # publishes "on" / "off"
```

Each entry names exactly one of `sensor:`, `text_sensor:` or `text:`, by id. At most **8** sources, and a key is 1–16 characters of `a-z`, `0-9` or `_`. A key nothing answers draws as `--` rather than failing, which is also what a value shows before its entity has published anything.

> They are declared here rather than picked up from the style because the device never reads a style — it stores the JSON and hands it to the browser untouched, which is what lets a new kind of section be a page change alone. This list is the device's only statement of what is worth reading.

**On the Home Assistant card** the same panel needs the values to reach it, and a dashboard on https cannot fetch the device. Add the text sensor:

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: lcd
    name: "LCD Values"
```

The `@` values the card can work out for itself — the host's name, slot and address — need no sensor at all. Everything travels in one Home Assistant state, which holds 255 characters; past that, whole entries are dropped and the log says so. The card can also read a key straight from Home Assistant instead, which is how a panel shows something the keyboard's node knows nothing about:

```yaml
type: custom:ble-remote-card
device: bluetooth_keyboard
remote_style: auto
lcd_entities:
  temp: sensor.lounge_temperature
```

A key named there wins over the device's value for the same key.

#### A complete example

This is **Style 6**, the built-in with a screen and logo keys, written out so you can see how one is put together — and copy it as the starting point for your own. The screen shows which host the keyboard is on and whether it is connected: both `@` values, so **it needs no `sources:` at all**. Add the `lcd` text sensor only if you want the panel filled on the Home Assistant card too.

The `id` below is deliberately not `style6` — a built-in's id is reserved, so an import has to use its own. Change the name and it joins the stepper beside the built-ins.

```json
{
  "id": "media_lcd",
  "name": "Media box + screen",
  "theme": {
    "bg": "#17181d",
    "border": "#2a2c33",
    "radius": "34px",
    "pad": "22px 12px",
    "maxw": "280px",
    "btn_bg": "#232630",
    "btn_fg": "#e8e8ec",
    "btn_border": "#303341",
    "ring_bg": "#232630",
    "ring_fg": "#e8e8ec",
    "ok_bg": "#454a5c",
    "ok_fg": "#ffffff",
    "label": "#8a8d99",
    "divider": "#303341"
  },
  "sections": [
    ["row","remote_power","|","search","mute"],
    ["ring"],
    ["row","back","home","menu"],
    ["media","rewind","play_pause","fast_forward"],
    ["rocker",["Vol","volume_up","volume_down"],["Ch","channel_up","channel_down"]],
    ["-"],
    ["lcd",{"cols":16,"rows":2,"fg":"#7fd4ff","bg":"#0e1014","label":"#5a6b78","border":"#303341"},
      ["","@host","centre"],
      ["","@state","sm centre"]],
    ["apps",["spare1","Netflix","icon:netflix lg squircle"],["spare2","YouTube","icon:youtube lg squircle"],
      ["spare3","Prime Video","icon:prime lg squircle"],["spare4","Disney+","icon:disney lg squircle"]],
    ["apps",["spare5","Spotify","icon:spotify lg squircle"],["spare6","Plex","icon:plex lg squircle"],
      ["spare7","Kodi","icon:kodi lg squircle"],["spare8","TV","icon:tv lg squircle"]]
  ]
}
```

The eight logo keys are app tiles — `lg squircle` — and [spare actions](#action-reference): they send nothing until you give each one a per-host override, which is what lets the same key launch a different app on each machine. Each names an icon with `icon:` — see [Logos on buttons](#logos-on-buttons) — and shows its label until that icon is imported. Rename them to whatever you point them at.

The panel is a deliberate 16 characters wide, so it sits inside the 280px body as a screen rather than a banner, and `rows: 2` holds its height steady as the host name changes length. Its colours are its own, overriding the style's — swap `fg` to `#ffb000` on `#1a1206` for an amber display instead. Compact, this style is 1066 characters of the 1500 a custom style may use.

### Action Reference

| Action | Description |
|---|---|
| `"switch_host:N"` | Switch to host slot N (0–9). If the slot has a stored host, uses directed advertising to reconnect. If empty, starts normal advertising for new pairing. |
| `"switch_host:next"` / `"switch_host:prev"` | Step one slot forward or back, wrapping at the ends — the same cycling the host switcher arrows on the cards do, but on the device, so a single remote key or macro can rotate through hosts. Empty slots are included in the rotation. |
| `"switch_host:back"` | Return to the slot active before the last switch. See [Visiting another host and coming back](#multi-host-switching). |
| `"prev_host"` / `"next_host"` / `"last_host"` | The same three as remote keys a style can place, remappable per host like any other. |
| `"host_action:N:<name>"` | Run slot N's [Host Action](#host-actions-per-host-overrides) for `<name>` whichever host is active — `host_action:6:spare1`. A name slot N has no action for runs as an ordinary press. What a tab [showing a host's page](#remote-style-per-host) sends for every key. |
| `"peer:<name>:<action>"` | Run an action on a [linked keyboard](#linking-a-second-keyboard). What a tab driving that keyboard sends for every key. |
| `"wait:connected"` | Hold a macro until the active host is ready for keys, up to 10 s (`wait:connected:N` for N ms). |
| `"forget_host:N"` | Remove the bond for host slot N (0–9). Clears the stored address and removes the BLE bond from the ESP32. If the forgotten host is currently connected, it is disconnected. |
| `"press_button:<object_id>"` | Press another ESPHome button — e.g. `press_button:samsung_43_m70f_wol`. See [Pressing other ESPHome buttons](#pressing-other-esphome-buttons). |
| `"alternate:<a> \|\| <b> \|\| …"` | Run **one branch** per press, advancing each time. Branches split on `\|\|`; a single `\|` still means "next step", so a branch can be a whole sequence. See [Toggling one button between two actions](#toggling-one-button-between-two-actions). |
| `"macro:<name>"` | Run a stored [web macro](#web-macros) by name — a live reference, so editing the macro updates everything pointing at it. Macros may call each other (nesting is capped). |
| `"if:<source>: <when on> \|\| <when off>"` | Branch on something the device actually knows, instead of `alternate:`'s blind counter. Branches split on `\|\|` and each may be a whole sequence. Nothing runs until the source has a state. See [Branching on real state](#branching-on-real-state). |
| `"lcd:<text>"` | Write text to an [LCD panel](#lcd-panels)'s `@msg` line, so a key can name where it just took you — `consumer:0x0223 \| lcd:Netflix`. Up to 64 characters. |
| `"ha_action:<domain>.<action>;<key>=<value>;…"` | Ask Home Assistant to run one of its own actions — e.g. an IR blaster's `remote.send_command`. Needs `ha_action: true`. See [Calling Home Assistant Actions](#calling-home-assistant-actions). |

---

## Pressing Other ESPHome Buttons

Some things a keyboard simply can't do over BLE. The clearest case is power: a monitor or PC can be told to sleep with a HID consumer code, but nothing can wake it over Bluetooth once it's off — that needs Wake-on-LAN.

So every non-internal `button:` in your config is listed on the web control page automatically, next to the component's own buttons. **No configuration is needed** — define the button as usual and it appears:

```yaml
button:
  - platform: wake_on_lan
    name: "Samsung 43 M70F WOL"
    target_mac_address: "04:CB:01:07:D2:24"
    id: button_wake_on_lan_m70f
```

The page shows it as **Samsung 43 M70F WOL** (the `name`, not the `id`). Because it goes through the normal action system as `press_button:samsung_43_m70f_wol`, it works everywhere an action does — in [web macros](#web-macros), in [per-host overrides](#host-actions-per-host-overrides), and via the REST API:

```bash
curl -X POST http://<device-ip>/api/ble_keyboard/press -d 'action=press_button:samsung_43_m70f_wol'
```

Actions are keyed by **object id** (the slugified name), not by position, so adding or reordering buttons never repoints a saved macro or override.

**Powering a monitor both ways.** The two directions need different transports, so map them to separate buttons — off over BLE, on over the network:

```yaml
espidf_ble_keyboard:
  id: my_keyboard

button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Monitor Off"
    action: "consumer:0x30"
  - platform: wake_on_lan
    name: "Monitor On"
    target_mac_address: "04:CB:01:07:D2:24"
    id: button_wake_on_lan_m70f
```

> **Unless you have set up authentication, anyone who can reach the page can press any listed button** — and the button listing itself hands out the names to press. Keep destructive ones such as `restart`, `factory_reset` or `safe_mode` off it with `hide_buttons`. Hidden buttons are also rejected if their action is typed into a macro by hand. See [Securing the web control page](#securing-the-web-control-page).
>
> ```yaml
> espidf_ble_keyboard:
>   id: my_keyboard
>   hide_buttons:
>     - button_factory_reset
>     - button_restart
> ```
>
> `expose_buttons: false` turns the whole listing off. Buttons marked `internal: true` are never listed. The component's own `espidf_ble_keyboard` buttons are unaffected — they appear exactly as before, once.

### Toggling One Button Between Two Actions

Having "off" and "on" as separate buttons is fine on a web page but wrong on a remote, where power is one button. The catch: **HID is one-way.** The keyboard sends reports and never receives anything back, so it cannot tell whether the monitor is currently on. A toggle must therefore either *assume* the state or *read* it from somewhere else. Both are supported.

**Assumed state — the `alternate:` action.** It runs one **branch** per press and advances each time. Branches are separated by `||`, while a single `|` keeps its usual meaning of "next step" — so each branch can be a whole sequence:

```
alternate:consumer:0x30 | delay:1000 | ok || press_button:samsung_43_m70f_wol
         └──────── branch 1: press, wait, confirm ────────┘    └─ branch 2 ─┘
```

That matters for real hardware. The M70F won't sleep from the power code alone — it puts a confirmation prompt on screen, so the off sequence is three steps that must run together on one press. Using a single `|` between them would spread them across three presses.

Put it in **Host Actions** as the replacement for `remote_power` on the monitor's slot and the remote's power button sleeps it, then wakes it, then sleeps it. No reflash — Host Actions persist to NVS, so this can be edited from the web UI at any time. It also works in macros, YAML `actions:`, and the REST API.

In the web UI, build the first branch with the preset dropdown as usual, then pick **Alternate — one branch per press** from the *Other* group. Then pick **— New branch (||) —** and build the second branch, and finally **Alternate (one per press)** to wrap the lot. Unlike every other preset, Alternate **wraps** what's already in the box rather than appending, because `alternate:` takes the whole chain.

> **Wake-on-LAN often needs more than one packet.** Magic packets are unacknowledged UDP and get dropped, and some displays ignore the first one while their network interface wakes. Branches are ordinary action strings, so `repeat:` composes:
>
> ```
> alternate:consumer:0x30 | delay:1000 | ok || repeat:3:press_button:samsung_43_m70f_wol | delay:100
> ```
>
> Still one press per branch — press once to sleep, once to send three wake packets. Raise the count if your display needs more.

The counter is keyed on the action text, so the same string driven from the web remote, the HA card and a macro stays in step — they're all working one physical device.

> **It's a guess, and guesses drift.** Turn the monitor off with its own button and the sequence is inverted until you press through once more. The device has no way to detect that, which is what the next option is for. The position also resets on reboot.

Two smaller limits: `alternate:` must be the **whole** action, not one step inside a longer chain, and at most 16 distinct alternate sequences are tracked at once. With no `||` at all there's just one branch, which runs in full on every press.

<a id="branching-on-real-state"></a>
**Real state — the `if:` action.** When something can actually tell you the state, branch on it instead of counting presses. Declare it as a `sources:` entry and the same name works in the action, on a panel, and as a button's `lit:` colour:

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  sources:
    - key: monitor
      binary_sensor: monitor_on

binary_sensor:
  - platform: homeassistant          # needs api:
    id: monitor_on
    entity_id: binary_sensor.samsung_m70f_power
```

```
if:monitor: consumer:0x30 | delay:1000 | ok || press_button:samsung_43_m70f_wol
```

Same `||` grammar as `alternate:`, so switching an existing button over is a one-word edit — first branch while the source reads `on`, second while it reads `off`. Turn the monitor off with its own remote and the next press still does the right thing, which is the case `alternate:` gets wrong.

**Until the source has a state, the button does nothing** — no guess at boot before Home Assistant has connected, which matters when the off-branch sends Wake-on-LAN. A single branch means "do this when on, nothing when off". Give the key `lit:monitor` and it lights up while the monitor is on, so the remote shows the state as well as following it.

If the monitor is one of the keyboard's hosts and drops Bluetooth in standby, the keyboard can tell for itself: a [paired sensor with `slot:`](#paired-sensor-default) set to the monitor's slot is on only while it is connected. Some Samsung monitors report standby to Home Assistant while lit, which sends the Wake-on-LAN branch to a monitor that is already on — the slot sensor doesn't have that problem. It reads off while another host is active, so the press then falls to the wake branch.

**Real state — a template button.** For a toggle that can't drift, let a template button hold the decision. It appears on the web page automatically, so it works exactly like any other button:

```yaml
globals:
  - id: monitor_on
    type: bool
    restore_value: yes

button:
  - platform: template
    name: "Monitor Power"
    id: monitor_power
    on_press:
      - lambda: |-
          if (id(monitor_on)) {
            id(my_keyboard).execute_action("consumer:0x30");   // sleep over BLE
          } else {
            id(button_wake_on_lan_m70f).press();               // wake over the network
          }
          id(monitor_on) = !id(monitor_on);

espidf_ble_keyboard:
  id: my_keyboard
  hosts:
    - slot: 0
      actions:
        remote_power: "press_button:monitor_power"
```

As written this still assumes — but `restore_value: yes` carries the state across reboots, and swapping the global for a real `binary_sensor` (a ping probe, a power monitor, anything that actually knows) makes it correct. That's the advantage over `alternate:`; the cost is that changing it needs a reflash.

A button cannot trigger *itself* — that's refused and logged. Chaining to a *different* button is fine, so the lambda above may equally call `id(my_keyboard).execute_action("press_button:samsung_43_m70f_wol")` instead of `.press()`.

**Two directions instead of a toggle — a template switch.** Where the two directions are genuinely different things, a switch says so: Home Assistant gets an on/off entity, and "turn the monitor off" stops meaning "press whatever is next". No lambda needed, because [`espidf_ble_keyboard.run_action`](#action-types) works in `turn_on_action` and `turn_off_action` like any other automation action:

```yaml
switch:
  - platform: template
    name: "Monitor Power"
    optimistic: true
    turn_off_action:
      - espidf_ble_keyboard.run_action:
          id: my_keyboard
          action: "consumer:0x30 | delay:1000 | ok"   # sleep over BLE, confirm the prompt
    turn_on_action:
      - button.press: button_wake_on_lan_m70f          # wake over the network
```

`optimistic: true` still *assumes* — it believes whatever it was last told, exactly like `alternate:`. The difference is that a wrong guess only makes the entity's icon wrong, not its next action: "off" always sleeps and "on" always wakes. Swap `optimistic:` for a `lambda:` reading a real sensor and even the icon is right.

> A switch is a Home Assistant and YAML entity only. It is **not** listed on the web control page and cannot back a `remote_power` [host override](#host-actions-per-host-overrides) — `press_button:` reaches buttons, not switches. For the remote's power key, use `alternate:` or the template button above.

---

## Calling Home Assistant Actions

BLE reaches the paired host and nothing else. An infrared-only TV behind a Broadlink blaster, a script, a scene — those belong to Home Assistant. The `ha_action:` prefix hands an action to HA over the native API, so a remote key can fire them like anything else:

```
ha_action:<domain>.<action>;<key>=<value>;<key>=<value>
```

`ha_action:remote.send_command;entity_id=remote.living_room_ir;command=power` sends the blaster's learned `power` command. Put it in [Host Actions](#host-actions-per-host-overrides) as the replacement for `remote_power` on the TV's slot, and the power key fires IR on that host from **both** the Home Assistant [Media Remote Card](#media-remote-card-for-home-assistant) and the web remote — the cards need no changes, because the name is resolved on the device. Host Actions and Macros offer a preset for it once enabled.

Enabling takes two deliberate switches:

```yaml
api:

espidf_ble_keyboard:
  id: my_keyboard
  ha_action: true
```

and **Allow the device to perform Home Assistant actions** in the device's ESPHome integration options in HA. Without the HA-side permission the call is dropped *silently*; without `ha_action: true` the device refuses it and logs why. The component enables `api: homeassistant_services: true` for you.

Syntax rules:

* Records split on `;` — the first is the action name (`domain.action`), the rest are data pairs.
* Pairs split at the **first** `=`, so values may contain `=`. Keys and values are trimmed of surrounding spaces; inner spaces survive.
* `|` (the chain separator) and `;` cannot appear inside a value, and there is no escaping — the same limitation as every other action string.
* It chains and alternates like any action: `alternate:consumer:0x30 || ha_action:remote.send_command;entity_id=remote.tv;command=power` sleeps over BLE one press and wakes over IR the next. One caveat: within a single chain, `ha_action` steps are handed to HA at the **end** (`delay:` blocks the loop they queue on), so space out repeated IR commands with the action's own data — `num_repeats`, `delay_secs`, `hold_secs` — not with `delay:` between two `ha_action` steps.
* Overrides and macros cap at 255 characters, so a raw `b64:` IR payload doesn't fit — teach the blaster the command and call it by name instead.

> `ha_action:` reaches whatever HA lets the device call, and the web page is open to the network unless you have given it a password — which is why this is off by default. Enable it on a trusted network, or alongside [authentication](#securing-the-web-control-page).

---

## Installing the Cards via HACS

The three Lovelace cards (mouse, keyboard, media remote) can be installed and kept up to date with [HACS](https://hacs.xyz). This repository isn't in the HACS default store, so add it as a **custom repository**:

1. In Home Assistant, open **HACS**.
2. Three-dot menu (top right) → **Custom repositories**.
3. Repository: `https://github.com/markusg1234/ESPHome-espidf_ble_keyboard`
   Type/Category: **Dashboard**
4. **Add**, then find **ESPHome BLE Keyboard Cards** in HACS and click **Download**.
5. Reload your browser. All three cards now appear in the dashboard's **Add card** picker.

HACS registers the dashboard resource for you — there's no need to add anything under *Settings → Dashboards → Resources*. When a new version is released, HACS offers the update in the usual way.

In a **sections** dashboard, all three cards support the resize handles and the card editor's **Layout** tab. Each keeps its natural height by default and offers a sensible width and height range; each keeps its proportions at whatever size you pick and scrolls if the card is smaller than the controls need — use the `zoom` option to change how big those controls are. HA's height slider stops at 8 rows, which is shorter than the media remote needs with every section on, so set `grid_options: {rows: N}` in the card's YAML if you want it taller than that. Masonry dashboards are unaffected.

> **HACS installs the cards only — not the firmware.** There is no HACS category for ESPHome external components, so the `espidf_ble_keyboard` component is still added to your device YAML with `external_components:` (see [Usage Example](#usage-example)) and updated by re-flashing the device. A HACS update for this repository updates the dashboard cards and nothing else.

---

## Mouse Control Card for Home Assistant

A custom Lovelace card is included that provides a touchpad, 3 mouse buttons, and scroll controls. It requires ESPHome services to be defined so Home Assistant can call the mouse functions with parameters.

### 1. Add ESPHome services

Easiest: set `api_services: true` on the component — it auto-registers all the services this card needs (see [Home Assistant services](#home-assistant-services)) and you can skip to step 2. To define them manually instead:

```yaml
api:
  encryption:
    key: ${api_encryption_key}
  services:
    - service: mouse_move
      variables:
        x: int
        y: int
      then:
        - lambda: |-
            id(my_keyboard).send_mouse_move(x, y);
    - service: mouse_scroll
      variables:
        amount: int
      then:
        - lambda: |-
            id(my_keyboard).send_mouse_scroll(amount);
    - service: mouse_click
      variables:
        btn: int
      then:
        - lambda: |-
            id(my_keyboard).send_mouse_click(btn);
    - service: mouse_hold           # press & hold for dragging — release with mouse_release
      variables:
        btn: int
      then:
        - lambda: |-
            id(my_keyboard).send_mouse_click_start(btn);
    - service: mouse_release
      then:
        - lambda: |-
            id(my_keyboard).send_mouse_click_release();
    - service: mouse_abs            # move cursor to exact position, percent of screen
      variables:
        x: float
        y: float
      then:
        - lambda: |-
            id(my_keyboard).execute_action("mouse_abs:" + to_string(x) + ":" + to_string(y));
```

### 2. Install the card

Install the cards with HACS — see [Installing the cards via HACS](#installing-the-cards-via-hacs). One download installs all three and keeps them updated.

### 3. Add to a dashboard

Add it from the dashboard UI (**Add card** → search for the card) and fill in the fields — the card has a **visual editor**, so no YAML is required. To edit as YAML instead, use the card's three-dot menu → **Edit** → **Show code editor**:

```yaml
type: custom:ble-mouse-card
device: bluetooth_keyboard    # your ESPHome device name (underscored)
```

Example with all optional overrides:

```yaml
type: custom:ble-mouse-card
device: bluetooth_keyboard
name: Living Room Mouse       # card title (auto-detected from HA if omitted)
zoom: 1                       # scale the whole card (default: 1)
sensitivity: 2.0              # base cursor speed (default: 1.5)
mouse_acceleration: 0.2       # speed-based acceleration factor (default: 0.15)
mouse_max_speed: 6.0          # max sensitivity cap (default: 4.5)
scroll_sensitivity: 3         # faster scroll (default: 2)
tap_to_click: false           # disable tap-to-click (default: true)
host_slots: 4                 # show host switcher (default: 0 = hidden)
host_names:                   # custom names for each slot (optional)
  - TV
  - Phone
  - Laptop
  - Tablet
```

Optional configuration:

| Option | Default | Description |
|---|---|---|
| `name` | Auto from HA | Card title. Auto-detected from HA device registry if omitted. |
| `peer_hosts` | — | Add a [linked keyboard](#linking-a-second-keyboard)'s hosts to this card's switcher, after this keyboard's own, so the arrows step through both and each press follows whichever host is selected. Each entry takes `peer` (its name from `peers:`), `slots` — a count or a list like `'1-3, 5'`, same as `host_slots` — `names` for the hosts it shows, in that order, and an optional `label`. Leave `host_slots` at 0 and the card drives nothing but the keyboards listed here. In the visual editor this is one line per keyboard — `bedroom \| 2 \| Bed TV, Bed PC \| Bedroom`. Tapping the host name jumps a whole keyboard. |
| `zoom` | `1` | Scales the whole card — touchpad, buttons and text together. `0.25`–`3`; values outside that are clamped. The card's height follows the zoom, and everything scales by the same factor in both directions so the controls keep their shape. |
| `sensitivity` | `1.5` | Base cursor speed multiplier. |
| `mouse_acceleration` | `0.15` | Speed-based acceleration factor. Higher = more acceleration on fast swipes. |
| `mouse_max_speed` | `4.5` | Maximum sensitivity cap. Limits how fast the cursor can move. |
| `scroll_sensitivity` | `2` | Scroll speed multiplier. |
| `tap_to_click` | `true` | Tap the touchpad for a left click (5px dead zone prevents accidental clicks). |
| `host_slots` | `0` | Which hosts the switcher offers. A number is a count — `4` is the first four — and a string picks them out by the numbers the switcher shows, e.g. `'1-3, 5, 7-10'`, for a keyboard whose other slots don't belong on this card. Needs at least two hosts; `0` hides the switcher. See [Host switcher on the cards](#host-switcher-on-the-cards). |
| `host_names` | `[]` | Names for the hosts the switcher shows, in the order it shows them (e.g., `["TV", "Phone"]`). With a count that is slot 0, slot 1 and so on; with a list like `'1,5'` the first name is host 1 and the second host 5. Falls back to `switch_host` button names from the ESP32, then "Host N". |
| `active_host_entity` | Auto | Entity ID of the [active host sensor](#active-host-sensor). Auto-detected by name pattern (`sensor.*_active_host`). Set explicitly if auto-detection fails. |
| `show_mac` | `true` | Show the active host's MAC address to the left of the switcher. |
| `host_url` | Auto | Address of the ESP32 (e.g. `http://192.168.1.50`), used to read slot MACs. Auto-detected from the device's HA registry entry. |

Features:
- **Touchpad** — 16:9 aspect ratio, drag to move cursor, tap for left click, mouse wheel/trackpad scroll.
- **Mouse acceleration** — slow movements are precise, fast swipes cover more ground.
- **Buttons** — Left, Middle, Right click; long-press to hold for dragging (needs the `mouse_hold`/`mouse_release` services), tap the held button to release.
- **Scroll** — Scroll Up / Scroll Down buttons (hold to repeat).
- **Host switcher** — optional prev/next buttons in the header to change the active BLE host, with its name and MAC address. See [Host switcher on the cards](#host-switcher-on-the-cards).
- **Remote styles** — the card draws the same [remote styles](#remote-style-per-host) the web page does, and can follow the one set for the active host.
- **Auto device name** — card title is auto-detected from Home Assistant's device registry.

![Mouse HA Card](docs/mouse_ha_card.png)

---

## Absolute Mouse Positioning

The touchpad / `mouse_move` actions are **relative** — they nudge the cursor by a
delta, like a real mouse. To move the cursor to an **exact** location, use the
absolute-pointer actions, which report a fixed coordinate that the host maps onto
the screen:

| Action | Coordinates |
|---|---|
| `mouse_abs:<x%>:<y%>` | Percent of the screen, `0`–`100` (decimals allowed). `mouse_abs:50:50` = center, `mouse_abs:0:0` = top-left, `mouse_abs:100:100` = bottom-right. **Resolution-independent — start here.** |
| `mouse_abs_px:<x>:<y>` | Exact pixels, converted using `screen_width` / `screen_height`. Set those to your host's resolution first. |
| `mouse_abs_mon:<idx>:<x%>:<y%>` | Percent within the region defined by `monitors[idx]` (see Multi-monitor below). |
| `mouse_abs_save` / `mouse_abs_restore` | Remember the current position and jump back later — e.g. `mouse_abs_save \| mouse_abs:5:5 \| left_click \| mouse_abs_restore`. |

Web/REST: `curl -X POST "http://<device-ip>/api/ble_keyboard/mouse_abs?x=50&y=50"`
(add `&unit=px` for pixels, `&monitor=1` for a monitor, `&btn=1` to click after moving).

### Save / restore — what it can and can't do

`mouse_abs_save` records the **last position the device itself commanded**, and
`mouse_abs_restore` jumps back to it. HID is a one-way input channel — the host
**never tells the device where the real cursor is** (the only host→device data is
the keyboard LED lock state). So restore is exact only when the ESP32 is the sole
thing moving the pointer; if you move a physical mouse in between, the device
still restores to *its* last value, not the true cursor. True capture of the OS
cursor would require a helper app on the host (`GetCursorPos`/`SetCursorPos`).

### Multi-monitor

Whether absolute coordinates can reach a second monitor is decided by the
**host**, not the firmware — the device cannot detect your monitor layout. The
host maps the `0..32767` range onto either the **primary monitor** or the **whole
virtual desktop**:

- On hosts that span the **virtual desktop**, set `screen_width` / `screen_height`
  to the total desktop size and (optionally) declare each monitor's region, then
  address a specific screen with `mouse_abs_mon`:

  ```yaml
  espidf_ble_keyboard:
    screen_width: 3840          # two 1080p monitors side by side
    screen_height: 1080
    monitors:
      - { name: left,  x: 0,    y: 0, width: 1920, height: 1080 }
      - { name: right, x: 1920, y: 0, width: 1920, height: 1080 }
  # mouse_abs_mon:1:50:50  -> center of the RIGHT monitor
  # mouse_abs:75:50        -> 75% across the whole desktop (also the right monitor)
  ```

- On hosts that confine the absolute pointer to the **primary monitor** (a common
  Windows default for a generic absolute mouse), `mouse_abs` / `mouse_abs_mon`
  only reach the primary monitor regardless of configuration. **Use
  `mouse_goto:<x>:<y>` instead** — it homes the absolute pointer to the desktop
  origin (primary top-left = Windows 0,0) and then steps *relatively*, and
  relative movement spans the whole virtual desktop, so it reaches every monitor.
  Feed it Windows virtual-desktop coordinates (read a spot's with the bundled
  [`docs/cursorpos.bat`](docs/cursorpos.bat)); for pixel accuracy turn "Enhance
  pointer precision" off and set the pointer-speed slider to a fixed position you then
  calibrate to (don't move it afterward — even one notch loses accuracy).

### Cross-monitor positioning with `mouse_goto`

`mouse_goto:<x>:<y>` is the **reliable way to hit an exact pixel on any monitor**
when the host confines the absolute pointer to the primary screen (the usual
Windows case). `x`/`y` are **Windows virtual-desktop coordinates** — the primary
monitor's top-left is `0,0`, and monitors to its left are negative. Read a spot's
coordinates by hovering it with the bundled [`docs/cursorpos.bat`](docs/cursorpos.bat):

```yaml
button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Click that button on monitor 2"
    action: "mouse_goto:4394:42 | left_click"
```

It works by homing the absolute pointer to the desktop origin, then stepping the
cursor there with **relative** moves (relative movement crosses monitors), routed
mid-screen so it doesn't jam at a monitor corner.

**Requirements for accuracy:**
1. **Re-pair** the host after first flashing the absolute-mouse feature (Windows
   caches the old HID descriptor, so the absolute report stays invisible until a
   fresh pairing — `mouse_abs`/`mouse_goto` do nothing otherwise).
2. Mark the Windows primary monitor with **`primary: true`** in `monitors:`.
3. In **Mouse Properties → Pointer Options**: turn **"Enhance pointer precision" off**
   (acceleration is non-linear and can't be calibrated out; vendor mouse software
   such as Logitech Options+ can re-enable it), and set the **pointer-speed slider to a
   fixed position and leave it there**. Each step is a different fixed multiplier and the
   per-axis calibration is tied to the one you pick, so moving it even a single notch
   makes `mouse_goto` land a few pixels off. (On Windows 11's 1–20 slider this rig is
   dialed in at **14** — 13 and 15 are visibly inaccurate.)
4. **Calibrate** the per-axis scale (next section). X and Y usually need different
   values. A residual of ~1–2 px is the integer mouse-count grid — host-side limit.

### Calibrating with the Position Finder

Enable the web UI (`web_control: true` + `web_server`) and open
`http://<device-ip>/ble_keyboard`. The **Position Finder** card makes calibration
a few taps — it's **locked by default** (so a stray tap can't move the cursor or
change the scale); click **Edit** to use it:

1. **Tap the desktop map** (or use the **nudge target** ±1 px buttons) to send the
   cursor to a spot. Start with a **small target near the primary's top-left** so
   an uncalibrated move can't fly off-screen.
2. Read where the cursor **actually** landed, type it into **"landed at" X/Y**, and
   hit **Auto-calibrate** — it computes the X/Y scale (`new = current × target ÷
   actual`), applies it live, and **saves it to the current host**. Re-tap and
   repeat to converge; aim more central if a reading hits a screen edge.
   - To read the live cursor position, use the bundled
     **[`docs/cursorpos.bat`](docs/cursorpos.bat)** — double-click it on the target
     PC for a live **physical-pixel** readout (no install; uses `GetCursorPos` under
     `SetProcessDPIAware` so the numbers match the Finder).
3. Fine-tune with the **goto scale ±** buttons (0.0001 steps, 4-dp), or **Reset**
   to the YAML default.

Each **host slot keeps its own** X/Y scale in NVS, so a 4K/scaled host and a 1080p
host each remember their own calibration. It **survives firmware updates** (NVS is
a separate partition; only a full chip erase wipes it). For belt-and-braces, copy
the dialed values into YAML as `mouse_goto_scale_x` / `_y` too. (Note: a saved
per-host value **overrides** the YAML default — use the Finder's **Reset** to push
a new YAML default onto a host.)

The Finder can't read the real cursor (HID is one-way), so it's "send and read the
value back," not a live cursor display. To capture and put back the *real* cursor,
see [Saving and restoring the cursor](#saving-and-restoring-the-cursor).

### Host support

Absolute pointers are reliable on **Windows** and **Linux**. **macOS and iOS**
frequently ignore or mishandle absolute USB/BLE pointers — treat as best-effort.
The relative mouse (touchpad / `mouse_move`) is unaffected and keeps working on
all hosts.

### Saving and restoring the cursor

The named actions `mouse_abs_save` / `mouse_abs_restore` only remember the **last
position the device itself commanded** — not where a physical mouse left the cursor,
and not a `mouse_goto` target (HID is one-way, so the device can never read the real
cursor). For a *real* save/restore, run a tiny **host-side** helper and trigger it
from the keyboard with a **shortcut key**:

1. Copy **[`docs/cursor_saverestore.bat`](docs/cursor_saverestore.bat)** to the PC
   (e.g. `C:\Tools\`). It reads/writes the live cursor with `GetCursorPos` /
   `SetCursorPos` (physical px, DPI-aware — same coordinates as `mouse_goto`) and has
   **no BLE/ESP32 link**. Run it as `cursor_saverestore.bat save` / `... restore`.
2. Make two **Windows shortcuts** to it and give each a **Shortcut key**:
   - Right-click the `.bat` → *Create shortcut*; set the shortcut's **Target** to
     `cmd /c "C:\Tools\cursor_saverestore.bat" save`; put it on the **Desktop or Start
     Menu** (required for the hotkey to be global); then Properties → **Shortcut key**
     = e.g. `Ctrl+Alt+C` (and **Run: Minimized** to hide the console flash).
   - A second shortcut the same way with `... restore` and `Ctrl+Alt+R`.
3. Fire those combos from the keyboard (Ctrl+Alt = `0x01 + 0x04` = `0x05`; `c` = `0x06`,
   `r` = `0x15`): **save** → `combo:0x05:0x06`, **restore** → `combo:0x05:0x15`.

Because combos and `mouse_goto` chain, **one button** can do the whole round-trip:

```yaml
button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Click monitor 2, then put the cursor back"
    action: "combo:0x05:0x06 | mouse_goto:4394:42 | left_click | combo:0x05:0x15"
```

The keyboard never touches the cursor file or the ESP32 — it only sends the shortcut
keys; **Windows** runs the helper. (Windows requires Ctrl+Alt or Ctrl+Shift in a
shortcut key — a bare letter won't register.)

---

## Web Control (Standalone — No Home Assistant)

A built-in web page with full keyboard and mouse control, served directly from the ESP32. Access it from any browser on the same network — no Home Assistant required.

### Setup

1. Add `web_server` and enable `web_control` in your YAML:

```yaml
web_server:
  port: 80
  # Strongly recommended — without it the page is open to your whole network.
  # auth:
  #   username: !secret web_username
  #   password: !secret web_password

espidf_ble_keyboard:
  id: my_keyboard
  web_control: true
```

2. Flash and open `http://<device-ip>/ble_keyboard` in any browser or phone.

The page can drive the computer you have paired, so read
[Securing the web control page](#securing-the-web-control-page) before you leave it open.

### Web Control Link in Home Assistant

Add this sensor to your YAML to get a clickable link in HA that opens the web control page:

```yaml
text_sensor:
  - platform: wifi_info
    ip_address:
      id: wifi_ip
      internal: true
  - platform: template
    name: "Web Control"
    icon: "mdi:keyboard"
    lambda: |-
      return {"http://" + id(wifi_ip).state + "/ble_keyboard"};
    update_interval: 60s
```

In Home Assistant, the sensor value will be a URL like `http://192.168.1.100/ble_keyboard`. Click it to open the web control page directly.

### Features

- **Full QWERTY keyboard** — letters, numbers, symbols, F-keys, modifiers, arrows
- **Paste bar** — paste or type text in the keyboard header field and press Send to type the whole thing at once, line breaks included. Tick **auto** to type text the moment it is pasted. When the page is reached over HTTPS a clipboard button appears that reads and sends the clipboard in one tap (browsers don't allow clipboard reading over plain HTTP — pasting into the field works everywhere)
- **Mouse touchpad** — 16:9 aspect ratio, drag to move cursor, tap for left click (5px dead zone prevents accidental clicks)
- **Mouse acceleration** — slow movements are precise, fast swipes cover more ground (up to 4x)
- **Mouse buttons** — Left, Middle, Right click; long-press a button to hold it for dragging (drag the touchpad or run `mouse_goto` while held), tap the held button to release
- **Scroll controls** — buttons + mouse wheel on the touchpad
- **Remote control** — D-pad navigation (Up/Down/Left/Right/Enter), Power, Home, Back, Search, Volume +/-, Mute, media transport including Record, red/green/yellow/blue colour keys (F1–F4), and app launchers (Explorer, Browser, Email, Calc, Search). Every button is a named action, so any of them can be remapped per host — see [Per-host action overrides](#host-actions-per-host-overrides)
- **Hold to Repeat** — the D-pad, volume, channel and scan buttons fire repeatedly while held; which buttons repeat and how fast is set per host — see [Hold to repeat per host](#hold-to-repeat-per-host)
- **Press and Hold** — pick buttons that stay held down on the host while you hold them, per host, for push-to-talk — see [Press and hold per host](#press-and-hold-per-host)
- **Keyboard operable** — the remote's buttons are ordinary buttons, so clicking one (or tabbing to it) leaves it focused and space or enter presses it again without reaching for the mouse. Holding the key repeats and holds just as holding the button does, following the same per-host settings — handy for the popped-out remote sitting beside whatever you are working in
- **Host Actions** — remap a named action per host slot (e.g. Record → Game Bar on a PC, HID Record on a TV), saved on the device — see [Per-host action overrides](#host-actions-per-host-overrides)
- **Backup & Restore** — download every runtime setting as a JSON file and re-apply it later or on another board — see [Backup and restore](#backup-and-restore)
- **Remove buttons per host** — untick the remote buttons a host doesn't need; they disappear for that host only — see [Removing remote buttons per host](#removing-remote-buttons-per-host)
- **Section toggles** — show/hide Keyboard, Mouse, Remote, and Buttons sections individually (state saved in browser)
- **Pop out the remote** — **Pop out** in the Remote heading moves the remote into a window of its own, so it stays in reach while you scroll the page or work in another app; **Pin back** returns it to where it was. See [Popping the remote out](#popping-the-remote-out)
- **Remote shapes stand on their own once popped out** — a style that draws its own remote body (Style 1, 2, 4, 5 and most imported ones) loses the card from behind it in the popped-out window, so what floats there is the remote's shape, shadow and taper rather than a slab inside a slab. In the page it keeps its card, like the sections around it. Styles that draw no body of their own keep theirs everywhere
- **Zoom controls** — resize keyboard and mouse with +/- buttons in 5% steps (50%–200%), zoom level saved in browser
- **Light/dark theme** — toggle between dark and light mode, preference saved in browser
- **BLE connection status** — live indicator shows Connected, Paired, or Disconnected (polls every 3s)
- **Device name display** — shows the configured `device_name` in the toolbar and browser tab title
- **Programmed buttons** — any buttons defined in YAML appear as clickable buttons on the web page
- **Zero dependencies** — no HA, no custom cards, no JS files to install
- **Works from any phone** — just open the URL in a mobile browser

### Popping the remote out

**Pop out** in the Remote heading moves the remote into a window of its own, sized to the remote and showing nothing else. Close the window, or press **Pin back** on the placeholder it leaves, to put the remote back. The window follows the active host and resizes when a host's style is a different size; its position is remembered.

The same remote-only view is at `http://<device>/ble_keyboard#remote` — bookmark it or add it to a phone's home screen. Put `?style=<id>` before the `#remote` to [keep that view on one style](#remote-style-per-host).

**Keeping it on top.** There are two ways:

- **The page's own on top tick box.** It needs Chrome or Edge on a **secure page**, so it is hidden on a plain `http://` device address. Two ways to get one:
  - Forward a local port — browsers treat `localhost` as secure. On Windows, in an Administrator PowerShell:

    ```powershell
    netsh interface portproxy add v4tov4 listenaddress=127.0.0.1 listenport=8080 connectaddress=<device-ip> connectport=80
    ```

    Then browse `http://localhost:8080/ble_keyboard`. Remove it with `delete` and the two `listen` arguments.
  - Put the device behind an HTTPS reverse proxy with a trusted certificate.

  The `unsafely-treat-insecure-origin-as-secure` Chrome flag does **not** work: the window never appears. The remote detects this, falls back to an ordinary window and stops offering on top for that address.

  An on-top window only resizes during a click, so it may open with a small margin that goes on your first press.
- **Let the operating system pin it** — works on any address and browser. On Windows, PowerToys' **Always on Top**: focus the window and press `Win+Ctrl+T`. Most Linux desktops offer it in the title-bar menu; macOS needs a third-party tool.

If the browser refuses the on-top window for any other reason, the remote opens as an ordinary window and the placeholder shows why.

### Linking a second keyboard

When one keyboard can't reach every host — Bluetooth range, a second room — put another ESP32 running this component near the rest, and let the first one's page drive both. List the second under `peers:`:

```yaml
espidf_ble_keyboard:
  web_control: true
  peers:
    - name: bedroom
      url: http://192.168.1.36
      username: !secret bedroom_web_user      # its web_server auth, if it has one
      password: !secret bedroom_web_password
```

The page then shows a bar of that keyboard's hosts below its own. Tap one and that keyboard switches to it, and the remote, keyboard, paste box and mouse all drive it — the remote in that host's style with its hidden, hold and repeat lists. Tap one of this keyboard's hosts to come back. The choice belongs to the tab (`?peer=bedroom` in the address), so one tab can drive the bedroom while another drives the lounge.

- The Position Finder and Host Actions stay on this keyboard, and the linked one's settings stay on its own page.
- Everything goes by way of this keyboard, so it is only as quick as the Wi-Fi between them: mouse movement is gathered up and sent a piece at a time rather than streamed.
- Styles are not copied between keyboards by the link itself, but **Export all** on one page and **Import** on the other copies the lot in one paste — see [Copying styles to another keyboard](#copying-styles-to-another-keyboard). Until a style is there, that host draws the full remote and its bar says which style is missing.
- Both keyboards need firmware with this feature.
- Give the address as an IP address or the keyboard's `.local` name.
- A macro or button reaches it the same way, with [`peer:bedroom:<action>`](#action-reference); the preset lists in Macros and Host Actions offer the common ones. That switches the other keyboard, not the tab.
- A Home Assistant card can drive a linked keyboard too: list its hosts under `peer_hosts` and they join the card's switcher after this keyboard's own, so the arrows carry on from one keyboard's hosts into the other's and every press follows whichever host is selected. What those hosts are called and which style to draw are the card's own settings — the sensors and the direct read describe the keyboard the card points at, not the linked one:

```yaml
type: custom:ble-remote-card
device: bluetooth_keyboard
host_slots: 2
peer_hosts:
  - peer: bedroom
    slots: 2
    names: [Bed TV, Bed PC]
    label: Bedroom
    remote_style: style3      # remote card only: a linked keyboard's style can't be read from here
```

  The selected host shows as `Bedroom: Bed TV`, and tapping that name jumps to the next keyboard rather than stepping host by host. With `host_slots: 0` the card drives nothing but the linked keyboards. The visual editor takes the same thing as one line per keyboard: `bedroom | 2 | Bed TV, Bed PC | Bedroom | style3`.
- To move the tab from the remote, give a style the `next_keyboard`, `prev_host_all` or `next_host_all` keys: the next keyboard on whatever host it is on, or one step through every host of every keyboard, in the order the bars show them.

**Anyone who can use this keyboard's page can drive the linked one**, because its login is stored in this keyboard's firmware.

### Backup and restore

Macros, host actions and `mouse_goto` calibration only exist in the device's NVS — none of it is in your YAML. An NVS erase, a re-flash that clears storage, or a board swap loses the lot, and the calibration in particular is tedious to redo. The **Backup** and **Restore** buttons next to the **Host Actions** heading save and re-apply all of it as a single JSON file.

**Backup** downloads `ble-kb-backup-<device>.json` containing:

- Macros
- Saved per-host action overrides (the `SAVED` rows — YAML-defined ones are deliberately excluded, since writing them back as saved overrides would shadow later YAML edits)
- The keyboard layout chosen in the web UI
- Per-host `mouse_goto` calibration
- Per-host hidden remote buttons
- Per-host hold-to-repeat settings (a host left on the defaults is simply absent, and restores as "reset to defaults")
- Per-host press-and-hold buttons
- Per-host on-connect actions
- Per-host remote styles, and any custom styles stored on the device
- Imported button icons
- Occupied host slots — address, address type, and whether the device still holds a Bluetooth bond for it
- This browser's interface preferences: theme, zoom, and which sections are shown and in what order

**Restore replaces, it doesn't merge.** Existing macros are deleted and saved overrides cleared before the file is applied, so the device ends up exactly as the backup describes. You'll get a confirmation dialog listing what's about to change.

> **Learned hosts restore addresses, not pairing.** A Bluetooth bond is the peer address *plus* the pairing keys, and the keys live inside the ESP32's Bluetooth stack where no API can export them. Restoring host slots is therefore a separate opt-in prompt: it brings back which host was in which slot, but any host whose bond is no longer on the device **must be paired again**. Until you do, the slot will look occupied and simply fail to connect. The restore summary names the slots this applies to, and the backup file records a `bonded` flag for each so you can tell in advance.

Not included: the pairing passkey and the generated per-slot Bluetooth addresses — those are device identity rather than settings, and come from your YAML.

Restore is **not atomic**. It replays the file through the same API endpoints the UI uses, one step at a time; if a step fails it stops and tells you where, leaving the device partly restored. Re-running a restore from the same file is safe and will bring it the rest of the way.

### REST API

The web control page uses these local HTTP endpoints (useful for custom integrations):

> **Send POST parameters in the request body, not the query string**, for anything longer than a few
> characters: `curl -X POST http://<device-ip>/api/ble_keyboard/macro_add -d 'name=tv' -d 'action=home | delay:500 | ok'`.
> The device reads either, and the body form is what the page itself uses. The reason is that the
> request line and *all* the request headers share one 1024-byte buffer on the device, so a long
> query string competes with the caller's own headers for it — cross the total and the request is
> refused with `431 Header fields are too long` before any endpoint sees it. A request body has its
> own budget, also 1024 bytes.

| Endpoint | Method | Parameters | Description |
|---|---|---|---|
| `/api/ble_keyboard/string` | POST | `keys` (string) | Type text |
| `/api/ble_keyboard/key` | POST | `modifier` (int), `keycode` (int) | Send key combo |
| `/api/ble_keyboard/mouse_move` | POST | `x` (int), `y` (int) | Move cursor |
| `/api/ble_keyboard/mouse_click` | POST | `btn` (int) | Click button |
| `/api/ble_keyboard/mouse_hold` | POST | `btn` (int, default 1) | Press and hold button(s) — release with `mouse_release` |
| `/api/ble_keyboard/mouse_release` | POST | — | Release all held mouse buttons |
| `/api/ble_keyboard/mouse_scroll` | POST | `amount` (int) | Scroll wheel |
| `/api/ble_keyboard/mouse_abs` | POST | `x`, `y` (percent; `unit=px` for pixels; `monitor=<idx>`; optional `btn`) | Move cursor to an exact position (absolute) |
| `/api/ble_keyboard/press` | POST | `action=mouse_goto:<x>:<y>` | Cross-monitor exact move (any action string) |
| `/api/ble_keyboard/screen` | GET | — | Desktop geometry (size, origin, monitors, goto scales) for the Position Finder |
| `/api/ble_keyboard/goto_scale` | POST | `v` / `vx` / `vy` (scale), `save=1`, `reset=1` | Set `mouse_goto` calibration live (persist per host with `save`) |
| `/api/ble_keyboard/goto_last` | GET | — | Last `mouse_goto` target (Windows coords) |
| `/api/ble_keyboard/status` | GET | — | Returns `{"connected":bool,"paired":bool,"device_name":"..."}` |
| `/api/ble_keyboard/state` | GET | — | `/hosts`, `/status` and the drawn host's `/hidden`, `/repeat` and `/hold` replies in one object — what a [linked keyboard](#linking-a-second-keyboard) reads |
| `/api/ble_keyboard/peers` | GET | — | Each linked keyboard's last `/state`, with `ok` and its `age` in seconds. Only on a keyboard with `peers:` |
| `/api/ble_keyboard/peer_forward` | POST | `peer`, `ep`, and that endpoint's own parameters | Pass one keyboard or mouse request (`string`, `key`, `hold_key`, `release`, `mouse_move`, `mouse_click`, `mouse_hold`, `mouse_release`, `mouse_scroll`) on to a linked keyboard |
| `/api/ble_keyboard/buttons` | GET | — | Returns JSON array of programmed buttons |
| `/api/ble_keyboard/press` | POST | `action` (string) | Trigger a programmed button action |
| `/api/ble_keyboard/hosts` | GET | — | Returns `{"active":N,"style_slot":N,"slots":[{"slot":N,"occupied":bool,"addr":"XX:XX:...","bonded":bool,"tpl":"style1"},...]}`. `tpl` is that host's [remote style](#remote-style-per-host) and is absent when it uses the default. `bonded` is false when the slot has no pairing key. `style_slot` is the slot the remote is drawn for — `active`, except while an action that switched host is still running |
| `/api/ble_keyboard/irk` | GET | `slot` (int, default active) | That host's Identity Resolving Key: `{"slot":N,"irk":"<32 hex chars>"}`, or `"irk":null` when the slot is empty or the host sent no key. **Refuses cross-site requests** — see [Identity key](#identity-key-irk) |
| `/api/ble_keyboard/switch_host` | POST | `slot` (int) | Switch to host slot 0–9 |
| `/api/ble_keyboard/forget_host` | POST | `slot` (int) | Remove bond for host slot 0–9 |
| `/api/ble_keyboard/macro_add` | POST | `name`, `action` | Add a new macro (max 16) |
| `/api/ble_keyboard/macro_update` | POST | `index`, `name`, `action` | Update an existing macro |
| `/api/ble_keyboard/macro_delete` | POST | `index` (int) | Delete a macro by index |
| `/api/ble_keyboard/overrides` | GET | `slot` (int, default active) | Per-host action overrides: `{"slot":N,"active":M,"items":[{"name":"record","action":"combo:0x0C:0x15","src":"nvs"\|"yaml"}]}` |
| `/api/ble_keyboard/override_set` | POST | `slot`, `name`, `action` | Set a per-host action override (max 8 per slot); persists to NVS |
| `/api/ble_keyboard/override_clear` | POST | `slot`, `name` | Delete a saved override, falling back to YAML / built-in |
| `/api/ble_keyboard/hidden` | GET | `slot` (int, default `style_slot`) | Buttons removed from the remote for that host: `{"slot":N,"hidden":["record"]}` |
| `/api/ble_keyboard/hidden_set` | POST | `slot`, `names` (comma-separated) | Replace a host's hidden-button set; empty `names` clears it (max 96) |
| `/api/ble_keyboard/repeat` | GET | `slot` (int, default `style_slot`) | That host's hold-to-repeat config: `{"slot":N,"set":bool,"delay":400,"rate":180,"buttons":["volume_up"]}`. `set:false` means the host is on the page defaults |
| `/api/ble_keyboard/repeat_set` | POST | `slot`, `delay`, `rate`, `names` (comma-separated), or `reset=1` | Replace a host's repeat config; empty `names` means nothing repeats, `reset=1` returns it to the defaults. Timings are clamped (delay 100–2000, rate 50–2000) |
| `/api/ble_keyboard/hold` | GET | `slot` (int, default `style_slot`) | That host's press-and-hold set, plus its repeat set so a UI can grey out conflicts: `{"slot":N,"buttons":["ok"],"repeat":["volume_up"]}` |
| `/api/ble_keyboard/hold_set` | POST | `slot`, `names` (comma-separated) | Replace a host's press-and-hold set; empty `names` clears it (max 96). Rejected with `400` if a name is already in that host's repeat set |
| `/api/ble_keyboard/remote_style_set` | POST | `slot`, `id` | Set that host's [remote style](#remote-style-per-host); empty `id` returns it to the full remote. The id is stored, never interpreted, so a style only the page knows about still round-trips |
| `/api/ble_keyboard/remote_templates` | GET | — | The custom styles held on the device: `{"max":6,"len":1500,"items":[{"index":0,"tpl":"{…}"}]}`. Each `tpl` is the style's JSON as a string |
| `/api/ble_keyboard/remote_tpl_chunk` | POST | `seq` (int), `data` (string) | Upload one piece of a custom style; `seq=0` starts a fresh upload. Chunked because a request carries only ~512 bytes of URL |
| `/api/ble_keyboard/remote_tpl_save` | POST | `index` (int) | Commit the uploaded chunks into custom style slot 0–5 |
| `/api/ble_keyboard/remote_tpl_delete` | POST | `index` (int) | Delete a custom style. Hosts pointing at it fall back to the full remote |
| `/api/ble_keyboard/hold_action` | POST | `action` (string) | Press and hold an action now — `400` if it isn't something that can be held |
| `/api/ble_keyboard/release` | POST | — | Release everything held: keys, consumer usage and mouse buttons |
| `/api/ble_keyboard/backup` | GET | — | All runtime settings as JSON: macros, saved overrides, layout, per-host calibration, and occupied host slots (with a `bonded` flag). **Refuses cross-site requests** — see [Securing the web control page](#securing-the-web-control-page) |
| `/api/ble_keyboard/goto_scale_slot` | POST | `slot`, `x`, `y` | Write `mouse_goto` calibration for any slot (`goto_scale` only writes the active one) |
| `/api/ble_keyboard/set_host_slot` | POST | `slot`, `addr`, `type` | Restore a host slot's address. Returns `OK-NOBOND` if the BLE bond is missing, meaning that host must be re-paired |

Example: `curl -X POST "http://<device-ip>/api/ble_keyboard/string?keys=Hello"`

**If `web_server:` has an `auth:` block, every example here needs credentials** — `-u user:pass`,
plus `--digest` if that is the scheme you configured. A mismatch is answered with a `401` that does
not say which was expected, so check the scheme in your YAML rather than guessing. The same applies
to Home Assistant `shell_command` and `rest_command` automations calling these endpoints.

```bash
curl --digest -u user:pass -X POST "http://<device-ip>/api/ble_keyboard/string?keys=Hello"
```

### Securing the web control page

**This page types on a computer you are logged into. Treat reaching it as reaching that keyboard.**

The first item below is the one that decides *who* may use the device, and it is the only one you
have to set up. The rest are on by default and stop a website turning your own browser against the
device — worth understanding, but none of them is a substitute for the first.

**Give it a username and password.** Add an `auth:` block to `web_server:` — see ESPHome's
[`web_server`](https://esphome.io/components/web_server.html) documentation for the options and what
each authentication scheme costs you. The part that matters here is that **it covers this
component too**: the handler is registered through the web server, so `/ble_keyboard` and every
`/api/ble_keyboard/…` endpoint sit behind the same login as the rest of the ESPHome UI.

Two consequences worth knowing before you turn it on:

- **The Home Assistant cards keep working.** They read the device directly only to populate the
  host bar, and they already fall back to this component's sensors when that read fails.
- **Scripts and automations need credentials**, matching whichever scheme you chose — see
  [REST API](#rest-api).

**Requests from other websites are refused.** These `POST` endpoints take their parameters in the
query string, which is exactly the shape a browser will send to another host without asking
permission first — so any page you happened to have open could have typed on your paired computer.
The device refuses a `POST` that the browser marks as coming from somewhere else, using a header
the browser sets and page scripts cannot fake. Requests that carry no such header — `curl`,
scripts, Home Assistant automations — are allowed, so every example on this page still works. That
is deliberate: those callers could already reach the device directly, and refusing them would break
documented usage without protecting anything.

**The page cannot be put in a frame,** which is the other way that check could be walked around: a
page inside a frame is on its own origin, so its requests look entirely legitimate while the clicks
that trigger them belong to whoever built the frame. If you deliberately embed the page in a
dashboard of your own, `web_allow_framing: true` lifts this — knowing that it lifts it for every
site, not only yours.

**Requests addressed to a name the device doesn't answer to are refused.** That closes the third
way: a site can point its own domain at your device's address, after which the browser treats its
requests as same-origin because, as far as it can tell, they are. The name it was addressed by is
what gives it away — an attacker's has to be a domain they own, and every one of those has a dot in
it. So an IP address, a `.local` name, and a short host name with no dot all pass; if something
else fronts the device — a reverse proxy, a DNS entry of your own — name it in
`web_allowed_hosts`, or turn the check off with `web_host_check: false`. A refusal is logged with
the name that was rejected.

**Reads are more open than writes.** The Home Assistant cards fetch the host list from the device
across origins, which only works because the web server allows it for every `GET` — so the host
list, and the rest of the device's read-only endpoints, can be read by any page in any tab. Two
exceptions are gated like the writes: the [identity key](#identity-key-irk), and `/backup`, which
would otherwise hand over every macro, override and paired address in one document. Authentication
is what closes the rest.

**Don't forward a port to this device.** If you need it from outside the house, reach it over a VPN
or through Home Assistant's own remote access.

### Identity key (IRK)

**Host Actions → Identity Key** shows the Identity Resolving Key a paired host handed over
when it bonded. Phones don't advertise a fixed address — they broadcast a random one that
changes every few minutes, and the real address is only ever sent over the encrypted link at
pairing time. The IRK is what turns one into the other: given a random address, it tells you
whether that address belongs to this host. That is what makes it useful for presence
detection, and it is why the MAC already shown in the host bar can't do the same job.

The ESP32 can't act on it itself. Presence detection means `ble_presence`, which pulls in
`esp32_ble_tracker` and `esp32_ble`, and that component initialises the Bluetooth controller
— which this one already does. Only one of them can own it, so they can't share a firmware.
(Same reason a Bluetooth proxy won't run alongside this component.)

> [!CAUTION]
> Adding `esp32_ble_tracker` to **this** device does not fail loudly — it compiles, boots, and
> logs nothing wrong. `esp32_ble` sets up first (priority `BLUETOOTH`, against this component's
> `-200`) and claims the controller; this component's own init calls then return
> `ESP_ERR_INVALID_STATE` and are discarded, and it goes on to replace the single GAP callback
> Bluedroid allows. The tracker stops receiving scan results from that moment, so a
> `ble_presence` sensor sits at "away" forever with nothing to explain it. Put the tracker on a
> different device.

Take the key elsewhere:

- **Home Assistant's Private BLE Device integration** takes an IRK directly and uses whatever
  Bluetooth receivers HA already has. No firmware changes needed.
- **A second BLE-capable ESPHome device** running a `ble_presence` binary sensor with the `irk:`
  option. It has to be a separate device — this one's Bluetooth controller is already spoken for,
  as above. (ESPHome 2026.8.0 made `ble_presence` platform-neutral; before that it needed an ESP32
  running `esp32_ble_tracker`.)

If you only need "is it in the house", you may not need the key at all: a
[binary sensor](#paired-sensor-default) with `type: paired` goes on when a bonded host
connects and off when it drops. Bluetooth range is roughly 10–30 m, and how eagerly a phone
reconnects to a HID device it isn't actively using varies between iOS and Android.

> [!WARNING]
> Treat the IRK like a password. Anyone holding it can identify that device from its random
> address for as long as the pairing lasts, which is exactly the tracking the random address
> exists to prevent. Don't paste it into an issue, a forum post or a shared config.
>
> The device won't hand it out to another website: `/api/ble_keyboard/irk` refuses requests
> carrying a cross-site `Sec-Fetch-Site` header, because ESPHome's web server allows any
> origin to read its responses by default. The key is also deliberately kept out of
> `/hosts` and out of backups. Non-browser clients such as `curl` send no such header and
> still work.

Only hosts that use address privacy send a key at all — but most do, Windows included, since
it uses a rotating address for Bluetooth LE just as a phone does. A host that pairs with a
fixed address has none to send, and that slot reports no key.

A key being present doesn't make a host worth tracking, mind: a desktop that never leaves the
house tells you nothing. The key is only useful for something that comes and goes.

---

## Keyboard Control Card for Home Assistant

A custom Lovelace card that provides a full on-screen QWERTY keyboard. It requires ESPHome services to be defined so Home Assistant can send keystrokes and text.

### 1. Add ESPHome services

Easiest: set `api_services: true` on the component — it auto-registers all the services this card needs (see [Home Assistant services](#home-assistant-services)) and you can skip to step 2. To define them manually instead (alongside any existing mouse services):

```yaml
api:
  encryption:
    key: ${api_encryption_key}
  services:
    - service: send_string
      variables:
        keys: string
      then:
        - lambda: |-
            id(my_keyboard).send_string(keys);
    - service: send_key
      variables:
        modifier: int
        keycode: int
      then:
        - lambda: |-
            id(my_keyboard).send_key_combo(modifier, keycode);
```

### 2. Install the card

Install the cards with HACS — see [Installing the cards via HACS](#installing-the-cards-via-hacs). One download installs all three and keeps them updated.

### 3. Add to a dashboard

Add it from the dashboard UI (**Add card** → search for the card) and fill in the fields — the card has a **visual editor**, so no YAML is required. To edit as YAML instead, use the card's three-dot menu → **Edit** → **Show code editor**:

```yaml
type: custom:ble-keyboard-card
device: bluetooth_keyboard    # your ESPHome device name (underscored)
```

Example with all optional overrides:

```yaml
type: custom:ble-keyboard-card
device: bluetooth_keyboard
name: Living Room Keyboard    # card title (auto-detected from HA if omitted)
zoom: 1                       # scale the whole card (default: 1)
show_fkeys: true             # hide F1-F12 row (default: true)
show_paste: true             # hide the paste bar in the header (default: true)
layout: us                    # us (default), uk, de, or be — match the ESP's keyboard_layout
host_slots: 4                 # show host switcher (default: 0 = hidden)
host_names:                   # custom names for each slot (optional)
  - TV
  - Phone
  - Laptop
  - Tablet
active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
show_mac: true                # show the host's MAC address (default: true)
```

Minimal UK layout example:

```yaml
type: custom:ble-keyboard-card
device: bluetooth_keyboard
layout: uk
```

Optional configuration:

| Option | Default | Description |
|---|---|---|
| `name` | Auto from HA | Card title. Auto-detected from HA device registry if omitted. |
| `peer_hosts` | — | Add a [linked keyboard](#linking-a-second-keyboard)'s hosts to this card's switcher, after this keyboard's own, so the arrows step through both and each press follows whichever host is selected. Each entry takes `peer` (its name from `peers:`), `slots` — a count or a list like `'1-3, 5'`, same as `host_slots` — `names` for the hosts it shows, in that order, and an optional `label`. Leave `host_slots` at 0 and the card drives nothing but the keyboards listed here. In the visual editor this is one line per keyboard — `bedroom \| 2 \| Bed TV, Bed PC \| Bedroom`. Tapping the host name jumps a whole keyboard. |
| `zoom` | `1` | Scales the whole card — keys, labels and spacing together. `0.25`–`3`; values outside that are clamped. The card's height follows the zoom, and everything scales by the same factor in both directions so the keys keep their shape. |
| `show_fkeys` | `true` | Show the F1–F12 function key row. |
| `show_paste` | `true` | Show the paste bar above the keys. Paste or type text there and press Send to type the whole thing at once; the **auto** checkbox types pasted text immediately. On HTTPS a clipboard button sends the clipboard in one tap. |
| `layout` | `us` | Keyboard layout for the on-screen card: `us`, `uk`, `de`, or `be`. UK draws the ISO shape (extra `\|` key, `£` on Shift+3); DE draws QWERTZ (Y/Z swapped, `ü`/`ö`/`ä`/`ß` keys, German modifier labels); BE draws AZERTY (A↔Q and Z↔W swapped, `M` on home row, `é è à ç ù` on the digit row). Set this to match the ESP's `keyboard_layout` option so the visual matches what gets typed. |
| `host_slots` | `0` | Which hosts the switcher offers. A number is a count — `4` is the first four — and a string picks them out by the numbers the switcher shows, e.g. `'1-3, 5, 7-10'`, for a keyboard whose other slots don't belong on this card. Needs at least two hosts; `0` hides the switcher. See [Host switcher on the cards](#host-switcher-on-the-cards). |
| `host_names` | `[]` | Names for the hosts the switcher shows, in the order it shows them (e.g., `["TV", "Phone"]`). With a count that is slot 0, slot 1 and so on; with a list like `'1,5'` the first name is host 1 and the second host 5. Falls back to switch_host button names from the ESP32, then "Host N". |
| `active_host_entity` | Auto | Entity ID of the [active host sensor](#active-host-sensor). Auto-detected by name pattern (`sensor.*_active_host`). Set explicitly if auto-detection fails. |
| `show_mac` | `true` | Show the active host's MAC address to the left of the switcher. |
| `host_url` | Auto | Address of the ESP32 (e.g. `http://192.168.1.50`), used to read slot MACs. Auto-detected from the device's HA registry entry. |
| `caps_lock_entity` | Auto | Entity ID of the [Caps Lock sensor](#led-state-sensors-num-lock--caps-lock--scroll-lock). Auto-detected by name pattern (`binary_sensor.*_caps_lock`). |

Features:
- **Full QWERTY layout** — letters, numbers, punctuation, all standard keys.
- **Modifier keys** — Ctrl, Alt, Win, Shift are sticky (toggle on, auto-release after next key).
- **Keys repeat when held** — the same as on the device's own page: hold a key (about a fifth of a second with a mouse, half a second with a finger) and the device leaves it down on the host, which repeats it at whatever delay and rate *that machine's* keyboard settings use. A shorter press types once. Modifiers and Caps Lock don't repeat, and nor do dead keys — they're two keystrokes, so there's no single key to hold; those still type once. Needs `api_services: true`, like the rest of the card.
- **Caps Lock** — sends a real Caps Lock to the host. With the device's `caps_lock` sensor configured, the key and a **CAPS** tag in the header follow the host's own Caps Lock, however it was turned on.
- **Function keys** — F1–F12 (can be hidden with `show_fkeys: false`).
- **Paste bar** — paste or type text in the field above the keys and send it as one piece, line breaks included (can be hidden with `show_paste: false`).
- **Arrow keys** — Up, Down, Left, Right + Delete.
- **Shift labels** — key labels update to show shifted characters when Shift is active.
- **Host switcher** — prev/next buttons to switch hosts, shows current host name and MAC address (requires `host_slots` and the `switch_host` ESPHome service). Also available on the mouse and remote cards — see [Host switcher on the cards](#host-switcher-on-the-cards).
- **Auto device name** — card title is auto-detected from Home Assistant's device registry.
- **Keyboard layouts** — `layout: us` (default), `layout: uk`, `layout: de`, or `layout: be` renders the matching ANSI/ISO/QWERTZ/AZERTY shape with the correct shifted labels.

![Keyboard HA Card](docs/keyboard_ha_card.png)

---

## Media Remote Card for Home Assistant

A custom Lovelace card that provides a modern media remote control with power, navigation D-pad, volume, media playback, and app launch buttons.

### 1. Add ESPHome services

Easiest: set `api_services: true` on the component — it auto-registers all the services this card needs (see [Home Assistant services](#home-assistant-services)) and you can skip to step 2.

Every button on this card fires a **named action** through `run_action`, so any of them can be remapped per host — see [Per-host action overrides](#host-actions-per-host-overrides). `send_string` is only used by the optional number pad. To define the two services manually instead (alongside any existing keyboard/mouse services):

```yaml
api:
  encryption:
    key: ${api_encryption_key}
  services:
    - service: run_action
      variables:
        action: string
      then:
        - lambda: |-
            id(my_keyboard).execute_action(action);
    - service: send_string
      variables:
        keys: string
      then:
        - lambda: |-
            id(my_keyboard).send_string(keys);
```

> If you previously pasted only the older `send_key` / `send_consumer` snippets, the card's buttons will silently do nothing until `run_action` exists.

### 2. Install the card

Install the cards with HACS — see [Installing the cards via HACS](#installing-the-cards-via-hacs). One download installs all three and keeps them updated.

### 3. Add to a dashboard

Add it from the dashboard UI (**Add card** → search for the card) and fill in the fields — the card has a **visual editor**, so no YAML is required. To edit as YAML instead, use the card's three-dot menu → **Edit** → **Show code editor**:

```yaml
type: custom:ble-remote-card
device: bluetooth_keyboard    # your ESPHome device name (underscored)
```

Example with all optional overrides:

```yaml
type: custom:ble-remote-card
device: bluetooth_keyboard
name: Living Room Remote      # card title (auto-detected from HA if omitted)
zoom: 1                       # scale the whole remote (default: 1)
show_numpad: true             # show number pad (default: false)
show_apps: true               # show app launch row (default: true)
show_color: true              # show color buttons (default: false)
host_slots: 4                 # show host switcher (default: 0 = hidden)
host_names:                   # custom names for each slot (optional)
  - TV
  - Phone
  - Laptop
  - Tablet
```

Optional configuration:

| Option | Default | Description |
|---|---|---|
| `name` | Auto from HA | Card title. Auto-detected from HA device registry if omitted. |
| `peer_hosts` | — | Add a [linked keyboard](#linking-a-second-keyboard)'s hosts to this card's switcher, after this keyboard's own, so the arrows step through both and each press follows whichever host is selected. Each entry takes `peer` (its name from `peers:`), `slots` — a count or a list like `'1-3, 5'`, same as `host_slots` — `names` for the hosts it shows, in that order, and an optional `label`, plus `remote_style` — the style a linked keyboard draws cannot be read from here. Leave `host_slots` at 0 and the card drives nothing but the keyboards listed here. In the visual editor this is one line per keyboard — `bedroom \| 2 \| Bed TV, Bed PC \| Bedroom \| style3`. Tapping the host name jumps a whole keyboard. |
| `zoom` | `1` | Scales the whole remote — buttons, text and spacing together. `0.25`–`3`; values outside that are clamped. The card's height follows the zoom, so `0.55` fits the full remote into roughly 8 grid rows, the shortest HA's height slider offers. Zooming past about `1.1` makes the remote wider than a 500px section, and the card scrolls sideways. |
| `remote_style` | `auto` | Which layout to draw: `auto` follows the style the device has for the active host, or pin one of `default`, `style1`…`style5`, or `custom` to use your own. |
| `remote_style_json` | — | The style to draw when `remote_style: custom`. Paste it from the web page's **Remote Style → Export**. |
| `remote_style_entity` | `sensor.<device>_remote_style` | Text sensor carrying the active host's style id. Needed for `auto` — see the note below. |
| `show_numpad` | `false` | Show the number pad. Filters whichever style is drawn, so turning it off removes the keypad from Style 4 and 5 too. |
| `show_apps` | `true` | Show the app launcher row. Filters whichever style is drawn. |
| `show_color` | `false` | Show red/green/yellow/blue color buttons (mapped to F1–F4). |
| `hidden_entity` | `sensor.<device>_hidden_buttons` | Text sensor carrying the active host's hidden buttons, so the card mirrors the web remote's [per-host hiding](#removing-remote-buttons-per-host). Optional — without the entity every button is shown. |
| `hold_entity` | `sensor.<device>_hold_buttons` | Text sensor carrying the active host's [Press and hold](#press-and-hold-per-host) buttons. Required for push-to-talk on the card — without it no button holds. |
| `repeat_entity` | `sensor.<device>_repeat_buttons` | Text sensor carrying the active host's [Hold to repeat](#hold-to-repeat-per-host) config. Without it the card repeats volume and channel only, at 400/180 ms. |
| `lcd_entity` | `sensor.<device>_lcd` | Text sensor carrying the values an [LCD panel](#lcd-panels) shows. Only needed if the style has one. |
| `lcd_entities` | — | Map of panel key to Home Assistant entity, e.g. `temp: sensor.lounge_temperature`. Read from HA directly, so it reaches things the keyboard's node never sees — and wins over the device's value for the same key. |
| `host_slots` | `0` | Which hosts the switcher offers. A number is a count — `4` is the first four — and a string picks them out by the numbers the switcher shows, e.g. `'1-3, 5, 7-10'`, for a keyboard whose other slots don't belong on this card. Needs at least two hosts; `0` hides the switcher. See [Host switcher on the cards](#host-switcher-on-the-cards). |
| `host_names` | `[]` | Names for the hosts the switcher shows, in the order it shows them (e.g., `["TV", "Phone"]`). With a count that is slot 0, slot 1 and so on; with a list like `'1,5'` the first name is host 1 and the second host 5. Falls back to `switch_host` button names from the ESP32, then "Host N". |
| `active_host_entity` | Auto | Entity ID of the [active host sensor](#active-host-sensor). Auto-detected by name pattern (`sensor.*_active_host`). Set explicitly if auto-detection fails. |
| `show_mac` | `true` | Show the active host's MAC address to the left of the switcher. |
| `host_url` | Auto | Address of the ESP32 (e.g. `http://192.168.1.50`), used to read slot MACs. Auto-detected from the device's HA registry entry. |

#### Remote styles on the card

The card draws its remote from the same style definitions the device's web page uses, so a layout looks the same in both places. Pick one in the card editor's **Remote style** dropdown.

**`auto`** mirrors whatever style the device has for the active host, so switching hosts re-skins the card just as it re-skins the web page. That needs the `remote_style` text sensor:

```yaml
text_sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: remote_style
    name: "Remote Style"
```

The style id is also on the device's `/hosts` response, which the card already polls — but a dashboard served over **https** cannot fetch a plain-http device, which is why the sensor exists. It is the same reason the hidden, hold and repeat lists travel as sensors.

**Styles travel one way: web page → card.** Build a style on the device's web page, where it is stored and named. Then copy its JSON from **Remote Style → Export** into the card's paste box, and it **joins the card's style list** under the name you gave it — selectable in the dropdown beside the built-ins, and drawn automatically when `auto` sees the device report that host's style. The card never writes back; the device stays the one place a style is defined.

#### Copying styles to another keyboard

Two keyboards, each with its own storage: a style made on one is not on the other. To copy the lot,
press **Export all** on the first keyboard's page, copy the JSON out of the box, open the second
keyboard's page, paste it into the same box and press **Import**.

- Styles are matched by `id`: one the second keyboard already has is **replaced**, the rest are
  **added**. Nothing is deleted — a style that keyboard has and the list does not stays where it is.
- Icons travel with the styles, so the logos arrive too.
- Nothing is written until every style in the list has been checked and given a slot, so a list that
  cannot fit leaves that keyboard as it was.
- Per-host assignments are not copied: which style a host uses is that keyboard's own setting.

**To bring several across at once, use Export all.** The plain **Export** button copies the one style the stepper is showing — that one is for editing. **Export all** copies *every* custom style on the device as a single JSON list, which is what the card's box wants — and what another keyboard's **Import** takes:

```yaml
remote_style_json: '[{"id":"lounge","name":"Lounge box","sections":[["dpad"],["media","play_pause","stop"]]},{"id":"study","name":"Study","sections":[["ring"],["row","volume_up","volume_down","mute"]]}]'
```

Paste that in and every custom remote joins the card's dropdown together. There is no need to send them all — one style is enough if that is all your hosts use, and six of them is roughly 6 KB in the card's YAML.

A style that uses [imported logos](#logos-on-buttons) exports with them attached, so the card draws them straight from the paste — nothing to fetch from the device, and it works on an https dashboard. Each logo adds one to four KB.

> A custom style the card has **not** been given still resolves to an id it has no definition for, so `auto` falls back to the full remote for that host. Paste it in and the fallback goes away.

> **The card's styles are a snapshot** taken when the card files were built. Flash newer firmware with a new built-in style and the installed card won't know that id until you update the cards too — which is what the version tags on the card imports are for.

Features:
- **Power button** — HID power signal for clean OS-level power control.
- **D-pad navigation** — arrow keys + Enter, ideal for media apps and menus.
- **Back & Home** — Escape and Windows key for quick navigation.
- **Volume** — up and down with hold-to-repeat, plus mute.
- **Channel** — Page Up/Down with hold-to-repeat for channel surfing.
- **Media playback** — play/pause, stop, previous, next, rewind, fast forward, record.
- **App launchers** — quick launch Explorer, Browser, Email, Calculator, Search.
- **Number pad** — optional 0–9 keypad for channel/PIN entry. The digits are named actions like every other key, so they are remappable and hideable per host.
- **Color buttons** — optional red/green/yellow/blue (F1–F4).
- **Per-host remapping** — every button is a named action, so any of them can do something different on each paired host.
- **Host switcher** — optional prev/next buttons in the header to change the active BLE host, with its name and MAC address. Switching here also repaints the card's [per-host hidden buttons](#removing-remote-buttons-per-host). See [Host switcher on the cards](#host-switcher-on-the-cards).
- **Auto device name** — card title is auto-detected from Home Assistant's device registry.

<img src="docs/remote_ha_card.png" height="560" alt="Remote HA card, full remote"> <img src="docs/remote_style6.png" height="560" alt="Remote in Style 6, with logo keys and a screen">

---

## Host Switcher on the Cards

All three Lovelace cards can show a host switcher in their header — prev/next arrows around the active host's name, with its MAC address to the left:

```
🖱  Mouse Control          AA:BB:CC:DD:EE:FF  ◀  Office PC  ▶
```

Add `host_slots` to any card to enable it (it needs at least two hosts; `0` hides it):

```yaml
type: custom:ble-mouse-card
device: bluetooth_keyboard
host_slots: 4
host_names: [TV, Phone, Laptop, Tablet]   # optional
show_mac: true                            # optional, default true
```

**Or name the hosts you want.** A number is a count, so `4` is the first four. A string picks hosts
out by the numbers the switcher shows, so a keyboard with ten paired machines can put three of them
on the kitchen dashboard and the rest nowhere:

```yaml
host_slots: '1-3, 5, 7-10'                # hosts 1, 2, 3, 5, 7, 8, 9 and 10
host_names: [TV, Phone, Laptop, Study, Shed, Garage, Loft, Pi]
```

The names line up with the hosts shown, in that order — the first name belongs to host 1 and the
fourth to host 5. Anything left unnamed falls back to the name the device reports for that host.
Switching still reaches the device as its own slot number, so a host is the same host wherever it is
listed, and [a linked keyboard's `peer_hosts`](#linking-a-second-keyboard) takes the same `slots`
spec.

**The cards stay in sync with each other.** Switch the host on the remote card and the mouse and keyboard cards follow — as do switches made from the [web control page](#web-control), a physical `switch_host:N` button, or a YAML action. There are two paths for this, and the cards use whichever is available:

| | How it works | Speed |
|---|---|---|
| [Active-host sensor](#active-host-sensor) | The firmware publishes the slot number on every switch; cards watch the entity | Instant |
| `/hosts` poll | Cards read the active slot from the device directly | Up to 30 s |

The poll alone is enough for most setups, so **the sensor is optional** — adding it just removes the lag:

```yaml
sensor:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    type: active_host
    name: "Active Host"
```

It stops being optional when the poll can't run — when Home Assistant is served over **HTTPS** (see the note below), or when the card can't work out the device's address. In those cases the sensor is the only thing keeping the cards in sync. If the MAC address is showing on your cards, the poll is working and the sensor is purely a speed-up.

**Where the name and MAC come from.** The name is `host_names[slot]` if you set one, otherwise the name of the matching `switch_host:N` button on the ESP32, otherwise "Host N". The MAC is read from the device's `/api/ble_keyboard/hosts` endpoint — the card finds the ESP32's address automatically from its Home Assistant device entry, or you can set `host_url: http://192.168.1.50` explicitly. An unpaired slot shows `Empty`.

> **If the MAC doesn't appear:** the card reads it directly from the ESP32 over plain HTTP, so it needs firmware **v1.5.0 or newer** — earlier builds sent a duplicated CORS header that browsers reject, blocking the read. Beyond that, when Home Assistant itself is served over **HTTPS** (Nabu Casa remote access, or a TLS reverse proxy) the browser blocks the request as mixed content and the MAC line hides itself. The switcher works regardless — it goes through Home Assistant, not the browser. Set `show_mac: false` to hide the line deliberately.
>
> To check quickly, open `http://<device-ip>/api/ble_keyboard/hosts` in a browser tab: JSON means the device is fine, and anything else points at the firmware or the address.

---

## Web Macros

When `web_control: true` is enabled, macros can be created, edited, and deleted directly from the web UI at `/ble_keyboard` — no reflash needed. Macros are stored in NVS flash and persist across reboots. Up to 16 macros are supported.

The web UI provides:
- **Add form** with name, action textarea, and a preset dropdown (media, system, clipboard, consumer HID, text, delays). Your YAML-defined `espidf_ble_keyboard` buttons also appear here under a **Buttons** group — pick one to reuse its action. The Host Actions card shares this dropdown and additionally lists your saved macros under a **Macros** group, so an override can reuse a macro's actions.
- **Combo builder** — toggle Ctrl/Shift/Alt/Win modifier buttons, then pick a key (F1-F12, arrows, letters, numbers, etc.) to insert `combo:mod:key`
- **Edit/Delete** controls on each macro (pencil and X buttons)
- **Macro index** shown as `[0]`, `[1]`, etc. next to each macro name — for the legacy `execute_macro(N)` form. Hovering a macro also shows its `macro:<name>` reference, which is what to use in YAML and automations: indices shift when a macro above them is deleted, names don't
- YAML-defined buttons appear alongside macros but are not editable
- Selecting a preset or key appends to the action field with `|`, making it easy to build multi-step macros

### Recording a Macro

Rather than typing an action string, you can perform it. Press **Record** in the Macros card header,
then use the remote, the on-screen keyboard and the mouse pad as you normally would — every press is
appended to the action box as a step. Press **Stop** and the recording is ready to name and save.
The read-out under the form shows the step count and how much of the 255-character limit is left.

Recording captures what you do **on this page**: remote buttons, typed text, keyboard combos, and
mouse clicks, holds and scroll. It reads the actions the page is already sending, so what you record
is exactly what will replay.

A few things it does on your behalf, so the result is usable rather than literal:

| Behaviour | Why |
|---|---|
| Consecutive typing merges into one `string:` step, however slowly you type | The keyboard sends one character per keypress, so typing `hello` would otherwise be five steps with delays wedged between the letters. Text keeps accumulating until you press something that isn't text |
| A held button records once | Held buttons auto-repeat; you want the press, not each repeat |
| Every press is separated by `delay:200`, or `delay:1000` after Home | Recording your own hesitation gives ragged values; one predictable number is easier to keep or edit. Home gets longer because a TV takes about a second to open its home screen |
| Consecutive scrolling accumulates into one step | Scrolling is one continuous gesture, not separate presses. Ten clicks of a 3-notch scroll record as `mouse_scroll:-30`, not ten steps eating 160 of the 255 characters. Reversing direction starts a new step rather than cancelling out |
| Cursor movement is not recorded | Dragging the mouse pad streams position continuously and would fill the whole macro |

So performing a monitor power-off — press HID Power, then press OK — records as
`consumer:0x30 | delay:200 | ok`. Edit any delay afterwards if a step needs longer; this monitor
wants `delay:1000` to give its confirmation dialog time to appear.

Notes and limits:
- Recording **extends** whatever is already in the action box, so you can record part of a macro,
  edit it, and record more onto the end.
- It stops on its own at 255 characters, keeping the last step that fits.
- A `|` typed on the on-screen keyboard is skipped — it would split the macro into two steps when it
  ran. Add one by hand if you actually want a step break there.
- A step can't begin or end with a space; leading and trailing spaces are trimmed when the macro runs.
- The popped-out remote records too. It's a separate browser window, so its presses are handed back
  to the page that opened it — watch the action box there fill as you press. If the pop-up was
  blocked and you opened the remote in an ordinary tab instead, that tab has no link back and won't
  record.
- Pressing an existing macro's button records its actions inline rather than as a `macro:<name>`
  reference, so the new macro stands on its own.

### Multi-Step Macros

Macros support multiple commands separated by `|`. A 50ms delay is automatically inserted between steps. Use `delay:N` for explicit pauses (max 10000ms). Prefix a macro with `repeat:N:` to run the whole sequence N times (max 1000).

Examples:
| Action string | Description |
|---------------|-------------|
| `combo:2:6 \| delay:100 \| combo:2:25` | Copy, wait 100ms, Paste |
| `combo:2:4 \| delay:50 \| combo:2:6` | Select All, Copy |
| `play_pause \| delay:500 \| next_track` | Play/Pause, wait 500ms, Next Track |
| `combo:0:40 \| delay:200 \| combo:0:40` | Enter twice with 200ms gap |
| `repeat:3:combo:0:40 \| delay:200` | Press Enter 3 times, 200ms apart |
| `combo:2:4 \| delay:50 \| string:hello` | Select All, type "hello" |
| `mouse_abs_save \| mouse_abs:90:10 \| left_click \| mouse_abs_restore` | Click top-right corner, then return the cursor |

Multi-step actions work everywhere: web macros, YAML buttons, `execute_action()`, and the `/api/ble_keyboard/press` endpoint.

### Triggering Macros from YAML

No lambda needed. `macro:<name>` is an ordinary [action string](#action-types), so a `button:` entry
runs a macro directly — and that button appears in Home Assistant as a named entity:

```yaml
button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Copy-Paste"
    action: "macro:Copy-Paste"
```

For any *other* trigger — a GPIO key, a sensor threshold, a time schedule — use the
`espidf_ble_keyboard.run_action` automation action. It takes the same strings, so a macro name, a
named action or a whole multi-step sequence all work:

```yaml
binary_sensor:
  - platform: gpio
    pin: GPIO0
    name: "Macro Button"
    on_press:
      then:
        - espidf_ble_keyboard.run_action:
            id: my_keyboard
            action: "macro:Copy-Paste"
    on_release:
      then:
        - espidf_ble_keyboard.run_action:
            id: my_keyboard
            action: "combo:2:6 | delay:100 | combo:2:25"
```

`action:` is templatable, so a lambda can still choose the string at runtime if you need it to:

```yaml
        - espidf_ble_keyboard.run_action:
            id: my_keyboard
            action: !lambda 'return id(pick_macro).state ? "macro:Work" : "macro:Home";'
```

The lambda helpers `execute_action("…")` and `execute_macro("name")` remain available for automations
that are already inside a lambda. `execute_macro(0)` also still works and runs the macro shown as
`[0]` in the web UI, but deleting a macro moves every macro below it up a slot, so a stored index can
end up on a different macro. A name can't drift that way, and a name that no longer exists logs a
warning instead of running the wrong thing.

### Triggering Macros from Home Assistant

Web macros are created at runtime (stored in NVS), so they don't appear as individual Home Assistant entities — ESPHome entities are fixed at compile time. To reach them from HA, set `api_services: true` on the component — it auto-registers `run_macro_name`, `run_macro` and `run_action` (see [Home Assistant services](#home-assistant-services)). Or define them manually:

```yaml
api:
  services:
    # Run a stored web macro by name — unaffected by deleting other macros
    - service: run_macro_name
      variables:
        name: string
      then:
        - lambda: |-
            id(my_keyboard).execute_macro(name);
        - delay: 0ms   # keeps the string arg linkable across ESPHome upgrades, see note

    # Same, by index ([0], [1], … shown in the web UI) — indices shift on delete
    - service: run_macro
      variables:
        index: int
      then:
        - lambda: |-
            id(my_keyboard).execute_macro(index);

    # Run any action string directly (single or multi-step with "|")
    - service: run_action
      variables:
        action: string
      then:
        - lambda: |-
            id(my_keyboard).execute_action(action);
        - delay: 0ms   # keeps the string arg linkable across ESPHome upgrades, see note
```

> **Note:** the no-op `- delay: 0ms` works around an ESPHome 2026.5+ quirk: a fully synchronous yaml service with a `string` variable is code-generated as a zero-copy `StringRef`, and after an ESPHome upgrade a stale cached object file can miss that symbol, failing the link with `undefined reference to get_execute_arg_value<StringRef>`. The delay flips codegen back to the long-supported `std::string` path. Services auto-registered with `api_services: true` are pure C++ and don't need this.

Then call them from a HA automation, script, or **Developer Tools → Actions**:

```yaml
# Run a stored web macro by name
action: esphome.<device_name>_run_macro_name
data:
  name: "Copy-Paste"

# Or by index, if you'd rather ([0] in the web UI)
action: esphome.<device_name>_run_macro
data:
  index: 0

# Or run an ad-hoc action string (no stored macro needed)
action: esphome.<device_name>_run_action
data:
  action: "mouse_abs_save | mouse_abs:0:0 | left_click | mouse_abs_restore"
```

> **Tip:** For a permanent, *named* clickable button in HA, define a `button:` platform entry instead (those auto-appear in HA and accept the same action strings, including multi-step). Web macros are best for ad-hoc, web-managed actions reached via `run_macro_name` / `run_action`.

### Macro REST API

| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/ble_keyboard/buttons` | — | Returns all buttons and macros as JSON. Macros have `"editable":true` and `"index":N`. |
| POST | `/api/ble_keyboard/macro_add` | `name`, `action` | Add a new macro (max 16). |
| POST | `/api/ble_keyboard/macro_update` | `index`, `name`, `action` | Update an existing macro. |
| POST | `/api/ble_keyboard/macro_delete` | `index` | Delete a macro by index. |

---

## Press and Hold

**Example use: push-to-talk.** Normally every key this component sends is a **tap**: key down, brief pause, key up — which is no good for Discord, Teams, TeamSpeak or a game, all of which need the key held down for exactly as long as you hold the physical button. `key_hold` sends the key down and leaves it there until `key_release`.

Other keys keep working while a key is held: the held key rides along in every keyboard report, so you can type or press other macropad keys mid-transmission without dropping it.

### From a physical button

An ESPHome `button` entity has a press and no release, so a push-to-talk key is wired from a `binary_sensor` instead — the same GPIO or matrix key you already use, with the hold on `on_press` and the release on `on_release`:

```yaml
binary_sensor:
  - platform: gpio
    pin: GPIO4
    name: "Push to talk"
    on_press:
      - espidf_ble_keyboard.key_hold:
          id: my_keyboard
          modifier: 0x00
          key: 0x3A        # F1 — see docs/keycodes.md
    on_release:
      - espidf_ble_keyboard.key_release:
          id: my_keyboard

  # every other key on the macropad is unchanged
  - platform: gpio
    pin: GPIO5
    on_press:
      - button.press: mute_key
```

Three automation actions are available:

| Action | Parameters | Description |
|---|---|---|
| `espidf_ble_keyboard.key_hold` | `id`, `modifier`, `key` | Hold a key and/or modifier. Both templatable. |
| `espidf_ble_keyboard.hold_action` | `id`, `action` | Hold anything holdable by name — `consumer:0x00E9`, `volume_up`, `left_click`, or a per-host remapped action. Templatable. |
| `espidf_ble_keyboard.key_release` | `id` | Release everything held. |

The same thing works from a lambda or any action string source (`run_action`, macros, the REST API): `id(my_keyboard).execute_action("key_hold:0x00:0x3A")` and `"release"`.

### From the web remote

Open **Host Actions → Press and hold** and tick the remote buttons that should stay down while held on that host, rather than sending a tap. It is stored per host slot, so a PC used for voice chat can hold while a TV slot does not.

The same list drives the Home Assistant Media Remote card, provided the `hold_buttons` text sensor is configured — see [Press-and-hold buttons](#press-and-hold-buttons). The card reaches the device through Home Assistant, so a lost release is likelier there than on the web page; `max_key_hold_ms` is worth setting if you use push-to-talk from a dashboard.

A button set to **Hold to Repeat** cannot also be set to hold, and vice versa — both claim the same gesture, so each panel greys out the buttons the other has taken.

### Keeping keys from sticking

A held key stays down until something releases it. It is released automatically when the host disconnects and when you switch host slots, and the web remote releases on pointer-up, pointer-cancel and when its tab is hidden. For the cases nothing catches — a browser killed mid-press, an `on_release` that never fires — set `max_key_hold_ms` on the component and the device releases on its own after that long. It is off by default, because a push-to-talk key has no natural maximum. Note that it also caps a mouse button held for dragging, since `release` covers those too.

> **Note:** only one **consumer** usage can be held at a time (`consumer_hold`, or holding a named media/remote action) — that HID report carries a single usage. Keyboard keys have no such limit; up to six can be held at once. Sending a different consumer action while one is held interrupts it briefly, then puts it back.

---

## Custom Text Input

You can send arbitrary text from Home Assistant to the paired host device without hardcoding it in the YAML. Link text entities to the keyboard component with `custom_text_id`, then use the `send_custom_text` action:

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  custom_text_id:
    - custom_text          # links the text entity below
    # - username_text      # add more text entities as needed

text:
  - platform: template
    name: "Custom Text"
    id: custom_text
    mode: text
    optimistic: true

button:
  - platform: espidf_ble_keyboard
    keyboard_id: my_keyboard
    name: "Send Custom Text"
    action: "send_custom_text"       # sends first text entity (index 0)
    # action: "send_custom_text:1"   # sends second text entity (index 1)
```

This adds a text input field and a send button to both Home Assistant and the web UI (via auto-registered buttons). A single ID also works: `custom_text_id: custom_text`.

You can also drive it from a Home Assistant automation — for example, updating the text entity from an `input_text` helper and then pressing the button:

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

> **Note:** Printable ASCII and Tab are supported on every layout. Non-ASCII characters work when they're part of the active layout's Unicode table (e.g. `£`, `¬`, `€` on `uk`). Unmapped characters and most control characters are silently skipped.

---

## Battery level

The device has always advertised the BLE Battery Service — a host reads it to show a battery icon
beside the keyboard in its Bluetooth settings. Until now it had nothing to report and sat at a
fixed 100%.

> **This is the ESP32's own battery, not the host's.** The direction is device → host: your phone
> or PC learns how charged *the keyboard* is. It cannot work the other way — the device is a BLE
> peripheral, not a client, and a phone doesn't expose its battery to a keyboard it's paired with.
> To get a *host's* battery into Home Assistant, use the HA Companion app on the phone. On a
> USB-powered build, leave `battery_level:` unset.

Point `battery_level:` at any sensor that reads a percentage and the host sees the real value:

```yaml
sensor:
  - platform: adc
    # ADC1 only (GPIO32–39 on a classic ESP32). ADC2 pins validate fine but
    # read nothing once Wi-Fi is up, and this component always has Wi-Fi.
    pin: GPIO35
    id: battery_pct
    name: "Keyboard Battery"     # gives Home Assistant its own entity
    device_class: battery
    unit_of_measurement: "%"
    state_class: measurement
    accuracy_decimals: 0
    attenuation: 12db            # without this the ADC tops out near 1.1 V
    update_interval: 60s
    filters:
      # Divider and cell curve are yours to work out — what reaches the
      # keyboard just has to be 0–100. With a 2:1 divider a full 4.2 V cell
      # reads about 2.10 V at the pin and an empty 3.3 V one about 1.65 V.
      - calibrate_linear:
          - 1.65 -> 0.0
          - 2.10 -> 100.0
      - clamp:
          min_value: 0
          max_value: 100

espidf_ble_keyboard:
  id: my_keyboard
  battery_level: battery_pct
```

The `name:` is what makes it a Home Assistant sensor in its own right; `battery_level:` is
separately what makes the *host* see it. The two are independent — you can have either without
the other.

A reading outside 0–100 is clamped, and an unavailable one (NAN) is ignored rather than sent as a
flat battery. Only a change is transmitted, so a sensor that republishes the same number on every
update costs nothing.

**For a level that doesn't come from a plain sensor** — computed in a lambda, or pushed from Home
Assistant — use the action instead:

```yaml
    on_...:
      - espidf_ble_keyboard.set_battery_level:
          id: my_keyboard
          level: !lambda "return id(some_calculation);"
```

With `api_services: true` the same thing is reachable from Home Assistant as
`esphome.<device>_set_battery_level` with a `level` variable, which is also the easiest way to
check the plumbing without a battery attached.

> **A USB-powered build should leave this unset.** Reporting a made-up 100% is what it already
> does, and a host has no use for a percentage that never moves.

Hosts differ in when they refresh the reading: some poll, some rely on the notification and update
within seconds, and some only re-read on reconnect. The device notifies subscribers on every
change and re-sends the current level when a host subscribes, so anything slower than that is the
host's own caching.

---

## Keyboard layouts

The component supports multiple keyboard layouts. The active layout affects how characters in `send_string` are translated into USB HID `(modifier, keycode)` pairs. It **must match the host PC's keyboard setting** — typing `@` from the ESP under `us` while the host is set to UK produces `"`, since the host reinterprets the same physical key under its own layout.

### Supported layouts

| ID | Name | Notes |
|---|---|---|
| `us` | English (US) | Default. ANSI shape. |
| `uk` | English (UK) | ISO shape. Adds `£`, `¬`, `€` via UTF-8 (AltGr for `€`). |
| `de` | German (QWERTZ) | ISO shape. Y/Z swapped. Adds `ä`, `ö`, `ü`, `ß`, `€`, `§`, `°`, `µ`, `²`, `³` via UTF-8. Dead keys (`^`, `` ` ``, `~`, `´`) auto-completed with a trailing space so they type as bare characters via `send_string`. |
| `be` | Belgian (AZERTY) | ISO shape. A↔Q, Z↔W swapped, `M` moves to home row right of `L`. Digits 0–9 require Shift (unshifted digit row is `& é " ' ( § è ! ç à`). Direct accented chars: `é è à ç ù € £ ² ³ § µ`. **Dead-key + vowel sequences** auto-composed by `send_string` for `â ê î ô û` (circumflex), `Â Ê Î Ô Û` (uppercase circumflex via Shift on the 2nd stroke), `ä ë ï ö ü` (diaeresis), and `Ä Ë Ï Ö Ü`. Literal `^` `` ` `` `~` use dead-key + space (DE-style). AltGr layer: `@ # { } [ ] | \`. |

#### German (DE) AltGr characters

A standard German keyboard prints these characters in the lower-right corner of certain keys. The on-screen keyboards (both the device's web UI and the HA Lovelace card) render the same hint labels. To type them: toggle **AltGr** (right Alt) on the on-screen keyboard, then click the key. `send_string` resolves all of them directly via UTF-8 — no AltGr toggle needed.

| Combo | Char | Combo | Char |
|---|---|---|---|
| AltGr+q | `@` | AltGr+8 | `[` |
| AltGr+e | `€` | AltGr+9 | `]` |
| AltGr+m | `µ` | AltGr+0 | `}` |
| AltGr+2 | `²` | AltGr+ß | `\` |
| AltGr+3 | `³` | AltGr++ | `~` |
| AltGr+7 | `{` | AltGr+< | `\|` |

### Setting the layout

**YAML (default at boot):**

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  device_name: "ESP32 BLE KB"
  keyboard_layout: uk
```

**Web UI (overrides YAML, persisted to NVS):** open `http://<device-ip>/ble_keyboard` and use the layout dropdown in the Keyboard card header. The choice is saved and survives reboot. Erasing NVS reverts to the YAML default.

> **Precedence note:** if you change `keyboard_layout` in YAML and reflash, the new value takes effect on the next boot — any previous web-UI override is automatically cleared. Web-UI overrides only persist across reboots while the YAML value stays the same. No factory reset needed to "see" a YAML edit.

**Per host slot (YAML, auto-applied on switch):** add `layout:` to any entry in the `hosts:` list to bind a layout to that slot. When you switch to that host (via service, button, or web UI), the device flips to its layout automatically. This is ephemeral — it does not overwrite a manual web-UI pick in NVS, and switching to a slot with no `layout:` keeps whatever was active.

```yaml
espidf_ble_keyboard:
  id: my_keyboard
  host_slots: 4
  hosts:
    - slot: 0
      layout: us
    - slot: 1
      layout: uk
```

### Matching the host's layout

The device layout only sets how the ESP turns characters into HID codes — the host then re-interprets those codes under its own layout. If they don't agree you'll see wrong symbols (e.g. `#` arriving as `\` when the ESP is on `uk` but the host is on `us`).

- **Windows:** *Settings → Time & language → Language & region →* pick the language (e.g. *English (United Kingdom)*) *→ Options → Keyboards →* leave *United Kingdom*. Switch with `Win+Space`.
- **Android:** *Settings → System → Languages & input → Physical keyboard →* tap the BLE keyboard's name *→ Set up keyboard layouts →* enable *English (UK)*. Android defaults every BLE keyboard to US until you do this. (Samsung / OneUI path: *Settings → General management → Physical keyboard*.)
- **iOS / iPadOS:** *Settings → General → Keyboard → Hardware Keyboard →* tap the layout name *→* pick *British*.
- **Linux (Wayland / GNOME):** *Settings → Keyboard → Input Sources →* add *English (UK)*, then move it to the top, or use `setxkbmap gb` on X11. For German use *Deutsch* / `setxkbmap de`.

### Adding a new layout

The layout system is intentionally small. Adding a new layout (e.g. French AZERTY) touches just three places:

1. **`components/espidf_ble_keyboard/keyboard_layouts.cpp`** — add `HID_ASCII_MAP_XX[128]` + (optionally) `UNICODE_MAP_XX[]` and append one entry to the `LAYOUTS[]` registry array.
2. **`components/espidf_ble_keyboard/__init__.py`** — append `"xx"` to `SUPPORTED_LAYOUTS`.
3. **`components/espidf_ble_keyboard/web_page.html`** — append an `xx: { ROWS: [...] }` entry to the JS `LAYOUTS` object. If you also ship the HA keyboard card, mirror the entry into `dist/keyboard-card.js`.

No header changes, no `send_string` changes, no NVS code changes. The web UI dropdown, `/api/ble_keyboard/status` JSON, and YAML validation pick the new layout up automatically.

**Dead keys** (characters that wait for a follow-up on the host, e.g. `^`, `` ` ``, `~`, `´` on German): set the optional third field `followup_keycode` in `HidKeyMapping`/`UnicodeKeyMapping` to the HID scan code for space (`0x2C`). `send_string` will emit the dead key followed by space, which composes to the bare character on the host.

### Notes

- Characters with no mapping in the active layout are skipped silently (a debug log is emitted).
- A layout switch in the middle of a typing operation can't corrupt in-flight text — keystrokes are pre-resolved at enqueue time using whatever layout was active then.
- `combo:` actions (raw HID `(modifier, keycode)` pairs) are layout-independent by design. Macros built from `combo:` keep working unchanged after a layout change.
- The web "Keyboard" card visual reflects the active layout (US shows ANSI, UK shows ISO with the extra `\|` key and `£` on Shift+3, DE shows QWERTZ with `ü/ö/ä/ß` keys and German modifier labels).

---

## Pairing with Windows

When you first flash the device or change the `passkey`:

1. Open **Bluetooth & other devices** on Windows.
2. If your device name (default: "ESP32 BLE KB") is already listed, **Remove Device**.
3. Click **Add device** -> **Bluetooth**.
4. Select your device name (default: "ESP32 BLE KB").
5. Windows will prompt you to enter the PIN. Type your configured `passkey` (e.g., `123456`) and click **Connect**.

---

## Pairing with Android

Android does not support passkey pairing with BLE HID keyboards. For reliable pairing:

1. **Do not set a `passkey`** in `espidf_ble_keyboard` (omit the passkey option entirely).
2. Use `passkey_mode: legacy` (the default).
3. In Android Bluetooth settings, remove any previous entry for your device name (default: **ESP32 BLE KB**) before re-pairing.
4. Start pairing - it should connect instantly without prompting for a PIN.

Android uses Just Works pairing for BLE HID devices. Attempting to use passkeys will result in pairing failures or automatic fallback to Just Works.

---

## Pairing with iOS

For iOS using passkey pairing:

1. Set `passkey` and `passkey_mode: secure_connections` in `espidf_ble_keyboard`.
2. Remove any previous bond for your device name (default: **ESP32 BLE KB**) from iOS Bluetooth settings.
3. Reboot the ESP32 (or reflash), then pair again from iOS.
4. Enter the configured passkey when prompted.

For Just Works pairing (no passkey), use `passkey_mode: secure_connections` for best compatibility.

After pairing, you should see all CCC subscriptions in the log (keyboard, consumer, system) confirming iOS has fully enumerated the HID service.

Notes:

* `passkey_mode: secure_connections` is the tested and recommended mode for iOS.
* The component includes Device Information and Battery services required by iOS for HOGP (HID over GATT Profile) compliance.
* macOS is expected to work the same way but has not been explicitly tested.

---

## Known Working Pairing Notes

The current implementation has been validated on Windows, Android, and iOS.
Tested on Windows 11, Android 16, and iOS.
For first-time pairing, Android may require more than one attempt while it refreshes BLE cache and bond state.

Recommended pairing modes:

* **Fastest pairing (recommended for Android/Windows):** Omit `passkey` (Just Works) with `passkey_mode: legacy`. Pairs instantly on Android and Windows. For iOS, use `passkey_mode: secure_connections`.
* **Windows with passkey:** Set `passkey` with `passkey_mode: legacy`. Pairs quickly with PIN entry.
* **iOS with passkey:** Set `passkey` with `passkey_mode: secure_connections` (legacy mode does not work on iOS).

Recommended order:

1. Turn off Bluetooth on other nearby hosts (especially Windows) to avoid auto-connect races.
2. Remove old keyboard entries from the phone/PC.
3. Retry pairing from the target host.

After the first successful bond, reconnect behavior is typically stable.

---

## Troubleshooting

* **Not appearing in search:** Ensure no other device is currently connected. The ESP32 stops advertising once a connection is established.
* **PIN prompt not appearing:** Windows often caches old security profiles. Fully "Remove" the device from Windows Bluetooth settings and try again.
* **Windows needs multiple pairing attempts:** Remove old Bluetooth entries first, then retry pairing after the first failed attempt. The component now avoids duplicate advertising restarts and keeps existing bonds unless the auth failure is a known `0x51` mismatch.
* **Android says "can't connect":** Android often keeps stale BLE bonds. Remove the device from Bluetooth settings, reboot the ESP32, then pair again. If still failing, toggle phone Bluetooth off/on and retry.
* **Android pairing issues:** Android does not support passkey pairing with BLE HID keyboards. Ensure no `passkey` is configured in your YAML - use Just Works pairing with `passkey_mode: legacy`. Remove old bonds and try again.
* **Wrong symbols on Android (`#` shows as `\`, `"` shows as `@`, etc.):** Android defaults connected BLE keyboards to US layout. Change it under *Settings → System → Languages & input → Physical keyboard → [device name] → Set up keyboard layouts → English (UK)*. See [Matching the host's layout](#matching-the-hosts-layout).
* **iOS not pairing:** Set `passkey_mode: secure_connections`, remove old Bluetooth bonds on both devices, then pair again.
* **iOS pairs but no typing/control:** Ensure you are using `passkey_mode: secure_connections`. Remove the bond on both the iOS device and the ESP32 (reboot/reflash), then pair again. After pairing, check the log for `Consumer CCC=0x0001` and `System CCC=0x0001` — if these are missing, iOS has not fully subscribed to the HID reports. Reflash and re-pair from a clean state.
* **Typed text in the wrong case:** Caps Lock is on at the host, and the host is a Mac or iPad, where Shift can't undo it — start the macro with `caps_lock:off`. See [Caps Lock and typed text](#caps-lock-and-typed-text).
* **Typing speed / dropped characters:** The default `key_delay_ms: 80` (40ms key-down + 40ms key-up) suits most connections. If characters are dropped on a slow BLE connection, increase this value (e.g. `key_delay_ms: 120`). If typing feels too slow, it can be reduced.
* **Hibernate not working:** Hibernate uses the Windows Run dialog. Ensure the PC is not in a state where it is blocked (e.g., fullscreen app or UAC prompt). Also ensure hibernate is enabled: run `powercfg /hibernate on` in an admin command prompt.
* **PC not waking from sleep:** Check that **USB Wake Support** (or similar) is enabled in your BIOS/UEFI Power Management settings.
* **Re-pair after firmware update:** If the HID descriptor changes (e.g. after adding media keys), you must remove and re-pair the device in Windows Bluetooth settings.
* **Web page slow to load or stalls on refresh, with `httpd_accept_conn: error in accept (23)` in the log:** the device has run out of network connections. Loading the page opens several at once and each finished request holds its connection briefly afterwards, so a refresh can use up the pool. With `web_control: true` the component reserves what the page needs, which raises the pool on its own — no config change required. If you still see it, set a larger value explicitly (it wins over the component's): add `CONFIG_LWIP_MAX_SOCKETS: "16"` (16 is the maximum) to the `sdkconfig_options` block. A page that is slow **only after the device has been idle**, with no such error in the log, is a different thing — that is Wi-Fi power saving. The example config above uses `power_save_mode: none` for this reason; if yours is set to `light` or `high`, the radio sleeps between beacons and the first load after a quiet spell pays for waking it up (measured here: 3.6s idle versus 1.3s warm). `none` costs a little more current, so it is the wrong trade on a battery device.
