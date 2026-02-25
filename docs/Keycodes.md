# ⌨️ BLE Keyboard Keycodes Reference

This page lists all HID keycodes for use with the `espidf_ble_keyboard` ESPHome component.

Keycodes are used in the `action:` field of a button using the `combo:` syntax:

```yaml
action: "combo:0x00:0x04"   # Tap 'a'
action: "combo:0x02:0x04"   # Tap 'A' (Left Shift + a)
action: "combo:0x08:0x15"   # Win + R (Run dialog)
```

**Format:** `combo:<modifier_hex>:<keycode_hex>`

---

## Modifier Keys

Modifiers are combined using bitwise OR (add the hex values together).

| Modifier | Hex Value | Description |
|---|---|---|
| None | `0x00` | No modifier |
| Left Ctrl | `0x01` | Left Control |
| Left Shift | `0x02` | Left Shift |
| Left Alt | `0x04` | Left Alt |
| Left GUI / Win | `0x08` | Left Windows / Command key |
| Right Ctrl | `0x10` | Right Control |
| Right Shift | `0x20` | Right Shift |
| Right Alt | `0x40` | Right Alt / AltGr |
| Right GUI / Win | `0x80` | Right Windows / Command key |

**Combining modifiers:** Add the hex values. e.g. Ctrl+Shift = `0x01 + 0x02` = `0x03`

```yaml
action: "combo:0x03:0x04"   # Ctrl + Shift + A
action: "combo:0x05:0x28"   # Ctrl + Alt + Enter
```

---

## Letter Keys (A–Z)

| Key | Keycode | Unshifted | Shifted (add `0x02`) |
|---|---|---|---|
| A | `0x04` | `a` | `A` |
| B | `0x05` | `b` | `B` |
| C | `0x06` | `c` | `C` |
| D | `0x07` | `d` | `D` |
| E | `0x08` | `e` | `E` |
| F | `0x09` | `f` | `F` |
| G | `0x0A` | `g` | `G` |
| H | `0x0B` | `h` | `H` |
| I | `0x0C` | `i` | `I` |
| J | `0x0D` | `j` | `J` |
| K | `0x0E` | `k` | `K` |
| L | `0x0F` | `l` | `L` |
| M | `0x10` | `m` | `M` |
| N | `0x11` | `n` | `N` |
| O | `0x12` | `o` | `O` |
| P | `0x13` | `p` | `P` |
| Q | `0x14` | `q` | `Q` |
| R | `0x15` | `r` | `R` |
| S | `0x16` | `s` | `S` |
| T | `0x17` | `t` | `T` |
| U | `0x18` | `u` | `U` |
| V | `0x19` | `v` | `V` |
| W | `0x1A` | `w` | `W` |
| X | `0x1B` | `x` | `X` |
| Y | `0x1C` | `y` | `Y` |
| Z | `0x1D` | `z` | `Z` |

---

## Number Keys (0–9)

| Key | Keycode | Unshifted | Shifted |
|---|---|---|---|
| 1 | `0x1E` | `1` | `!` |
| 2 | `0x1F` | `2` | `@` |
| 3 | `0x20` | `3` | `#` |
| 4 | `0x21` | `4` | `$` |
| 5 | `0x22` | `5` | `%` |
| 6 | `0x23` | `6` | `^` |
| 7 | `0x24` | `7` | `&` |
| 8 | `0x25` | `8` | `*` |
| 9 | `0x26` | `9` | `(` |
| 0 | `0x27` | `0` | `)` |

---

## Special / Control Keys

| Key | Keycode | Description |
|---|---|---|
| Enter | `0x28` | Return / Enter |
| Escape | `0x29` | Escape |
| Backspace | `0x2A` | Backspace / Delete |
| Tab | `0x2B` | Tab |
| Space | `0x2C` | Spacebar |
| Caps Lock | `0x39` | Caps Lock |
| Print Screen | `0x46` | Print Screen |
| Scroll Lock | `0x47` | Scroll Lock |
| Pause | `0x48` | Pause / Break |
| Insert | `0x49` | Insert |
| Home | `0x4A` | Home |
| Page Up | `0x4B` | Page Up |
| Delete | `0x4C` | Forward Delete |
| End | `0x4D` | End |
| Page Down | `0x4E` | Page Down |
| Menu | `0x65` | Application / Menu key |

---

## Arrow Keys

| Key | Keycode |
|---|---|
| Right Arrow | `0x4F` |
| Left Arrow | `0x50` |
| Down Arrow | `0x51` |
| Up Arrow | `0x52` |

---

## Function Keys (F1–F12)

| Key | Keycode |
|---|---|
| F1 | `0x3A` |
| F2 | `0x3B` |
| F3 | `0x3C` |
| F4 | `0x3D` |
| F5 | `0x3E` |
| F6 | `0x3F` |
| F7 | `0x40` |
| F8 | `0x41` |
| F9 | `0x42` |
| F10 | `0x43` |
| F11 | `0x44` |
| F12 | `0x45` |

---

## Punctuation & Symbol Keys

| Key | Keycode | Unshifted | Shifted |
|---|---|---|---|
| `-` / `_` | `0x2D` | `-` | `_` |
| `=` / `+` | `0x2E` | `=` | `+` |
| `[` / `{` | `0x2F` | `[` | `{` |
| `]` / `}` | `0x30` | `]` | `}` |
| `\` / `\|` | `0x31` | `\` | `\|` |
| `;` / `:` | `0x33` | `;` | `:` |
| `'` / `"` | `0x34` | `'` | `"` |
| `` ` `` / `~` | `0x35` | `` ` `` | `~` |
| `,` / `<` | `0x36` | `,` | `<` |
| `.` / `>` | `0x37` | `.` | `>` |
| `/` / `?` | `0x38` | `/` | `?` |

---

## Numpad Keys

| Key | Keycode | Description |
|---|---|---|
| Num Lock | `0x53` | Num Lock |
| Numpad / | `0x54` | Numpad Divide |
| Numpad * | `0x55` | Numpad Multiply |
| Numpad - | `0x56` | Numpad Subtract |
| Numpad + | `0x57` | Numpad Add |
| Numpad Enter | `0x58` | Numpad Enter |
| Numpad 1 | `0x59` | Numpad 1 / End |
| Numpad 2 | `0x5A` | Numpad 2 / Down |
| Numpad 3 | `0x5B` | Numpad 3 / PgDn |
| Numpad 4 | `0x5C` | Numpad 4 / Left |
| Numpad 5 | `0x5D` | Numpad 5 |
| Numpad 6 | `0x5E` | Numpad 6 / Right |
| Numpad 7 | `0x5F` | Numpad 7 / Home |
| Numpad 8 | `0x60` | Numpad 8 / Up |
| Numpad 9 | `0x61` | Numpad 9 / PgUp |
| Numpad 0 | `0x62` | Numpad 0 / Ins |
| Numpad . | `0x63` | Numpad . / Del |

---

## Common Shortcuts Quick Reference

| Action | YAML `action:` value |
|---|---|
| Win + R (Run) | `combo:0x08:0x15` |
| Win + L (Lock screen) | `combo:0x08:0x0F` |
| Win + D (Show desktop) | `combo:0x08:0x07` |
| Win + E (Explorer) | `combo:0x08:0x08` |
| Ctrl + C (Copy) | `combo:0x01:0x06` |
| Ctrl + V (Paste) | `combo:0x01:0x19` |
| Ctrl + Z (Undo) | `combo:0x01:0x1D` |
| Ctrl + Alt + Del | `ctrl_alt_del` *(built-in action)* |
| Alt + F4 (Close window) | `combo:0x04:0x3D` |
| Alt + Tab (Switch window) | `combo:0x04:0x2B` |
| Ctrl + Shift + Esc (Task Manager) | `combo:0x03:0x29` |

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

---

> **Note:** Keycodes follow the [USB HID Usage Tables specification](https://www.usb.org/hid) (Section 10, Keyboard/Keypad Page). The report descriptor in this component uses Report ID `0x01` with a standard 8-byte keyboard report: `[modifier, reserved, key1, key2, key3, key4, key5, key6]`. Currently only `key1` (byte 2) is used per keypress.