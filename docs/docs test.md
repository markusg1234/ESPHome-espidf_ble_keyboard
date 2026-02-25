# ⌨️ BLE Keyboard Keycodes Reference

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
```

> **Note:** Keycodes follow the [USB HID Usage Tables spec](https://www.usb.org/hid) (Keyboard/Keypad Page 0x07). The 8-byte report format is `[modifier, reserved, key1–key6]`. Currently one key per press is sent in the `key1` slot.