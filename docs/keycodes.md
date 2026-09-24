# ⌨️ BLE Keyboard Keycodes Reference

> **Looking for named actions?** `sleep`, `shutdown`, `hibernate`, `power`, `mute`, `volume_up`, `play_pause` and more are documented in the [README](../README.md#action-types).

Use keycodes in the `action:` field with the `combo:` syntax:

```yaml
action: "combo:0x00:0x04"   # Tap 'a'
action: "combo:0x02:0x04"   # Tap 'A' (Left Shift + a)
action: "combo:0x08:0x15"   # Win + R (Run dialog)
```

**Format:** `combo:<modifier_hex>:<keycode_hex>`

---

## Modifier Keys

Combine modifiers by adding hex values (e.g. Ctrl+Shift = `0x01 + 0x02` = `0x03`)

| Modifier | Hex | | Modifier | Hex |
|---|---|---|---|---|
| None | `0x00` | | Right Ctrl | `0x10` |
| Left Ctrl | `0x01` | | Right Shift | `0x20` |
| Left Shift | `0x02` | | Right Alt / AltGr | `0x40` |
| Left Alt | `0x04` | | Right GUI / Win | `0x80` |
| Left GUI / Win | `0x08` | | | |

---

## Letter Keys (A–Z)

| Key | Code | Key | Code | Key | Code | Key | Code |
|---|---|---|---|---|---|---|---|
| a / A | `0x04` | h / H | `0x0B` | o / O | `0x12` | v / V | `0x19` |
| b / B | `0x05` | i / I | `0x0C` | p / P | `0x13` | w / W | `0x1A` |
| c / C | `0x06` | j / J | `0x0D` | q / Q | `0x14` | x / X | `0x1B` |
| d / D | `0x07` | k / K | `0x0E` | r / R | `0x15` | y / Y | `0x1C` |
| e / E | `0x08` | l / L | `0x0F` | s / S | `0x16` | z / Z | `0x1D` |
| f / F | `0x09` | m / M | `0x10` | t / T | `0x17` | | |
| g / G | `0x0A` | n / N | `0x11` | u / U | `0x18` | | |

> Add modifier `0x02` (Left Shift) to type uppercase. e.g. `combo:0x02:0x04` = `A`

---

## Number Keys & Punctuation

| Key | Code | Shifted | | Key | Code | Shifted |
|---|---|---|---|---|---|---|
| 1 | `0x1E` | `!` | | - / _ | `0x2D` | `_` |
| 2 | `0x1F` | `@` | | = / + | `0x2E` | `+` |
| 3 | `0x20` | `#` | | [ / { | `0x2F` | `{` |
| 4 | `0x21` | `$` | | ] / } | `0x30` | `}` |
| 5 | `0x22` | `%` | | \ / \| | `0x31` | `\|` |
| 6 | `0x23` | `^` | | ; / : | `0x33` | `:` |
| 7 | `0x24` | `&` | | ' / " | `0x34` | `"` |
| 8 | `0x25` | `*` | | ` / ~ | `0x35` | `~` |
| 9 | `0x26` | `(` | | , / < | `0x36` | `<` |
| 0 | `0x27` | `)` | | . / > | `0x37` | `>` |
|   |        |     | | / / ? | `0x38` | `?` |

---

## Function Keys, Special Keys & Navigation

| Key | Code | | Key | Code | | Key | Code |
|---|---|---|---|---|---|---|---|
| F1 | `0x3A` | | F7 | `0x40` | | Enter | `0x28` |
| F2 | `0x3B` | | F8 | `0x41` | | Escape | `0x29` |
| F3 | `0x3C` | | F9 | `0x42` | | Backspace | `0x2A` |
| F4 | `0x3D` | | F10 | `0x43` | | Tab | `0x2B` |
| F5 | `0x3E` | | F11 | `0x44` | | Space | `0x2C` |
| F6 | `0x3F` | | F12 | `0x45` | | Caps Lock | `0x39` |
| Up Arrow | `0x52` | | Insert | `0x49` | | Print Screen | `0x46` |
| Down Arrow | `0x51` | | Delete | `0x4C` | | Scroll Lock | `0x47` |
| Left Arrow | `0x50` | | Home | `0x4A` | | Pause / Break | `0x48` |
| Right Arrow | `0x4F` | | End | `0x4D` | | Menu | `0x65` |
| | | | Page Up | `0x4B` | | | |
| | | | Page Down | `0x4E` | | | |

### Extended Function Keys (F13–F24)

Great as "phantom" macro keys on Windows — recognized as `VK_F13`–`VK_F24` but unused by default, so they never collide with app shortcuts (bind them in AutoHotkey, OBS, Discord, etc.).

| Key | Code | | Key | Code | | Key | Code |
|---|---|---|---|---|---|---|---|
| F13 | `0x68` | | F17 | `0x6C` | | F21 | `0x70` |
| F14 | `0x69` | | F18 | `0x6D` | | F22 | `0x71` |
| F15 | `0x6A` | | F19 | `0x6E` | | F23 | `0x72` |
| F16 | `0x6B` | | F20 | `0x6F` | | F24 | `0x73` |

> **Note:** hosts cache the HID descriptor per Bluetooth bond. If the device was paired with firmware older than the F13–F24 support, remove it from the host's Bluetooth settings and pair again, or these keys are silently ignored.

---

## Numpad Keys

| Key | Code | | Key | Code | | Key | Code |
|---|---|---|---|---|---|---|---|
| Numpad 0 | `0x62` | | Numpad 4 | `0x5C` | | Numpad 8 | `0x60` |
| Numpad 1 | `0x59` | | Numpad 5 | `0x5D` | | Numpad 9 | `0x61` |
| Numpad 2 | `0x5A` | | Numpad 6 | `0x5E` | | Numpad . | `0x63` |
| Numpad 3 | `0x5B` | | Numpad 7 | `0x5F` | | Numpad Enter | `0x58` |
| Numpad + | `0x57` | | Numpad - | `0x56` | | Num Lock | `0x53` |
| Numpad * | `0x55` | | Numpad / | `0x54` | | | |

---

## Common Shortcuts Quick Reference

| Action | `action:` value | | Action | `action:` value |
|---|---|---|---|---|
| Win + R (Run) | `combo:0x08:0x15` | | Ctrl + C (Copy) | `combo:0x01:0x06` |
| Win + L (Lock) | `combo:0x08:0x0F` | | Ctrl + V (Paste) | `combo:0x01:0x19` |
| Win + D (Desktop) | `combo:0x08:0x07` | | Ctrl + Z (Undo) | `combo:0x01:0x1D` |
| Win + E (Explorer) | `combo:0x08:0x08` | | Ctrl + Alt + Del | `ctrl_alt_del` |
| Alt + F4 (Close) | `combo:0x04:0x3D` | | Ctrl + Shift + Esc | `combo:0x03:0x29` |
| Alt + Tab (Switch) | `combo:0x04:0x2B` | | Ctrl + A (Select All) | `combo:0x01:0x04` |


---

## Consumer Control Codes

Use with the `consumer:` action prefix to send any HID consumer control command directly from YAML — no C++ changes needed.

```yaml
action: "consumer:0x0192"   # Open Calculator
action: "consumer:0x018A"   # Open Email
action: "consumer:0x0223"   # Browser Home
```

### Application Launch

| Action | Code | Description |
|---|---|---|
| Calculator | `0x0192` | Open calculator |
| Email | `0x018A` | Open email client |
| File Explorer | `0x0194` | Open file explorer |
| Media Player | `0x0183` | Open media player |
| Browser Home | `0x0223` | Open browser home page |
| Browser Search | `0x0221` | Open browser search |
| Browser Bookmarks | `0x022A` | Open browser bookmarks |
| Screen Saver | `0x019E` | Activate screen saver |
| Control Panel | `0x019F` | Open control panel |
| Task Manager | `0x01A9` | Open task manager |

### Media & Volume

| Action | Code | Description |
|---|---|---|
| Play / Pause | `0x00CD` | Toggle play/pause |
| Stop | `0x00B7` | Stop playback |
| Record | `0x00B2` | Start / stop recording. No Windows handler — see per-host action overrides in the [README](../README.md#per-host-action-overrides) |
| Next Track | `0x00B5` | Skip to next track |
| Previous Track | `0x00B6` | Go to previous track |
| Mute | `0x00E2` | Toggle mute |
| Volume Up | `0x00E9` | Volume increment |
| Volume Down | `0x00EA` | Volume decrement |
| Fast Forward | `0x00B3` | Fast forward |
| Rewind | `0x00B4` | Rewind |

### Power & System

| Action | Code | Description |
|---|---|---|
| Power Down | `0x0030` | Consumer power (use `shutdown` action instead) |
| Sleep | `0x0031` | Consumer sleep (use `sleep` action instead) |
| Brightness Up | `0x006F` | Increase display brightness |
| Brightness Down | `0x0070` | Decrease display brightness |

> **Note:** For `sleep` and `shutdown`, use the dedicated named actions instead — they use the more reliable Generic Desktop HID page (`0x81`/`0x82`).

---

## Mouse Actions (Report ID 4 + 5)

The component includes a 3-button relative mouse with scroll wheel (Report ID 4)
and an absolute pointer for exact-position moves (Report ID 5).

### Named Actions

| Action | `action:` value |
|---|---|
| Left Click | `left_click` |
| Right Click | `right_click` |
| Middle Click | `middle_click` |
| Save absolute position | `mouse_abs_save` |
| Restore absolute position | `mouse_abs_restore` |

### Parameterised Actions

| Format | Example | Description |
|---|---|---|
| `mouse_click:<buttons_hex>` | `mouse_click:0x01` | Click with button mask. `0x01` = left, `0x02` = right, `0x04` = middle. Combine for simultaneous buttons. |
| `mouse_move:<x>:<y>` | `mouse_move:50:-20` | Move cursor. Values -127 to 127 (relative, pixels). |
| `mouse_scroll:<wheel>` | `mouse_scroll:3` | Scroll wheel. Positive = up, negative = down (-127 to 127). |
| `mouse_abs:<x%>:<y%>` | `mouse_abs:50:50` | Move cursor to an **exact** position, percent of screen (0–100). Uses absolute Report ID 5. |
| `mouse_abs_px:<x>:<y>` | `mouse_abs_px:1280:720` | Exact position in pixels (uses `screen_width` / `screen_height`). |
| `mouse_abs_mon:<idx>:<x%>:<y%>` | `mouse_abs_mon:1:50:50` | Percent within declared `monitors[idx]` (multi-monitor). |
| `mouse_goto:<x>:<y>` | `mouse_goto:4394:42` | Move to a Windows virtual-desktop pixel across **all** monitors (homes absolute to 0,0 then steps relatively). X/Y = Windows coords (primary top-left = 0,0; negatives allowed). Use when the absolute pointer is stuck on the primary monitor. Needs per-axis calibration + pointer settings — see note below. |

### Dict Format

```yaml
action:
  type: mouse_click
  buttons: 0x01    # 0x01 = left, 0x02 = right, 0x04 = middle

action:
  type: mouse_move
  x: 50
  y: -20

action:
  type: mouse_scroll
  wheel: 3

action:
  type: mouse_abs    # also: mouse_abs_px, or mouse_abs_mon (add `monitor: <idx>`)
  x: 50              # percent of screen
  y: 50
```

> **Note:** Relative mouse uses HID Report ID 4; absolute positioning uses Report ID 5. Absolute pointers are reliable on Windows/Linux but inconsistent on macOS/iOS. After changing the HID descriptor, hosts that previously paired the keyboard must **re-pair** to discover the updated descriptor (otherwise `mouse_abs`/`mouse_goto` do nothing).

> **`mouse_goto` accuracy:** `mouse_goto` reaches every monitor but its relative step is scaled by the host's pointer speed/DPI, so it needs calibrating. In **Mouse Properties → Pointer Options** turn **"Enhance pointer precision"** *off* and set the pointer-speed slider to a fixed position, then calibrate to it (moving it even one notch loses accuracy — e.g. this rig uses 14 of 20). Set per-axis `mouse_goto_scale_x` / `mouse_goto_scale_y` in YAML (X and Y usually differ), or dial it in live with the web **Position Finder** (which saves the calibration per host). Mark your Windows primary monitor with `primary: true` in `monitors:`. See the README's *Absolute Mouse Positioning* section.

---

## Host Switching

| Action | Description |
|---|---|
| `switch_host:N` | Switch to host slot N (0–9). Reconnects to stored host via directed advertising, or advertises for new pairing if slot is empty. |
| `switch_host:next` / `switch_host:prev` | Step one slot forward or back, wrapping at the ends. Cycles through every configured slot, empty ones included. No-op when only one slot is configured. |
| `switch_host:back` | Return to the slot that was active before the last switch, however that switch was made. Pressed again, it goes back again. |
| `prev_host` / `next_host` / `last_host` | The same three as remote keys a style can place, remappable per host. |
| `next_keyboard` / `prev_host_all` / `next_host_all` | Web page remote only: move the tab to the next linked keyboard, or through every host of every linked keyboard. |
| `peer:<name>:<action>` | Run an action on a linked keyboard listed under `peers:` — `peer:bedroom:switch_host:1`. |
| `caps_lock:on` / `caps_lock:off` / `caps_lock:toggle` | Set the host's Caps Lock: on and off tap it only when the host reports it the other way, then wait for the host to confirm. |
| `wait:connected` / `wait:connected:N` | Hold a macro until the active host is connected and ready for keys, for at most N ms (default 10000, max 60000). On timeout the macro carries on. |
| `forget_host:N` | Remove BLE bond for host slot N (0–9) and clear the stored address. Disconnects the host if currently active. |

### Dict Format

```yaml
action:
  type: switch_host
  slot: 1

action:
  type: switch_host
  slot: next          # or prev

action:
  type: switch_host
  slot: back          # whichever slot was active before the last switch

action:
  type: forget_host
  slot: 2
```

---

## Full Button Example

```yaml
button:
  - platform: espidf_ble_keyboard
    keyboard_id: ble_keyboard
    name: "Open Run Dialog"
    action: "combo:0x08:0x15"

  - platform: espidf_ble_keyboard
    keyboard_id: ble_keyboard
    name: "Type Hello"
    action: "Hello World"

  - platform: espidf_ble_keyboard
    keyboard_id: ble_keyboard
    name: "Ctrl+Alt+Del"
    action: "ctrl_alt_del"

  - platform: espidf_ble_keyboard
    keyboard_id: ble_keyboard
    name: "Left Click"
    action: "left_click"

  - platform: espidf_ble_keyboard
    keyboard_id: ble_keyboard
    name: "Move Mouse Right"
    action: "mouse_move:50:0"
```

> **Note:** Keycodes follow the [USB HID Usage Tables spec](https://www.usb.org/hid) (Keyboard/Keypad Page 0x07). The 8-byte report format is `[modifier, reserved, key1–key6]`. Currently one key per press is sent in the `key1` slot.