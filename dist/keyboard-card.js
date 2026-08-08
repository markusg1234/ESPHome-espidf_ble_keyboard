/**
 * BLE Keyboard Control Card for Home Assistant
 *
 * A custom Lovelace card that provides a full on-screen QWERTY keyboard
 * for the ESPHome BLE Keyboard component.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/keyboard-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README).
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-keyboard-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # Optional overrides:
 *   # name: My Keyboard            # card title (default "BLE Keyboard")
 *   # show_fkeys: true             # show F1-F12 row (default true)
 *   # layout: us                    # keyboard layout: us (default), uk, or de
 *   #                                 NOTE: should match the ESP's keyboard_layout YAML option
 *   # host_slots: 4                # show host switcher (needs >1; default 0 = hidden)
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *   # active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
 *   # show_mac: true               # show the active host's MAC address (default true)
 *   # host_url: http://192.168.1.50  # ESP address (auto-detected from HA)
 *
 * Full example with overrides:
 *   type: custom:ble-keyboard-card
 *   device: bluetooth_keyboard
 *   name: Living Room Keyboard
 *   show_fkeys: false
 *   layout: uk
 *   host_slots: 4
 *   host_names:
 *     - TV
 *     - Phone
 *     - Laptop
 *     - Tablet
 *   active_host_entity: sensor.bluetooth_keyboard_active_host
 *   show_mac: true
 */

// HID keycodes for printable characters (used when Ctrl/Alt/Win modifiers are active)
const CHAR_TO_KEYCODE = {
  'a':0x04,'b':0x05,'c':0x06,'d':0x07,'e':0x08,'f':0x09,'g':0x0A,
  'h':0x0B,'i':0x0C,'j':0x0D,'k':0x0E,'l':0x0F,'m':0x10,'n':0x11,
  'o':0x12,'p':0x13,'q':0x14,'r':0x15,'s':0x16,'t':0x17,'u':0x18,
  'v':0x19,'w':0x1A,'x':0x1B,'y':0x1C,'z':0x1D,
  '1':0x1E,'2':0x1F,'3':0x20,'4':0x21,'5':0x22,'6':0x23,'7':0x24,
  '8':0x25,'9':0x26,'0':0x27,
  '`':0x35,'-':0x2D,'=':0x2E,'[':0x2F,']':0x30,'\\':0x31,
  ';':0x33,"'":0x34,',':0x36,'.':0x37,'/':0x38,' ':0x2C,
};

// Keyboard layouts — each key: { label, shiftLabel?, type, char?, shiftChar?, keycode?, mod?, flex? }
// To add a layout: append an entry below. The matching ASCII/Unicode tables on
// the ESP side (components/espidf_ble_keyboard/keyboard_layouts.cpp) translate
// what the card sends via send_string into the correct HID keycodes for the host.
const LAYOUTS = {
us: { name: 'English (US)', ROWS: [
  // F-key row
  [
    { label: 'Esc', type: 'special', keycode: 0x29, flex: 1.2 },
    { label: 'F1', type: 'special', keycode: 0x3A },
    { label: 'F2', type: 'special', keycode: 0x3B },
    { label: 'F3', type: 'special', keycode: 0x3C },
    { label: 'F4', type: 'special', keycode: 0x3D },
    { label: 'F5', type: 'special', keycode: 0x3E },
    { label: 'F6', type: 'special', keycode: 0x3F },
    { label: 'F7', type: 'special', keycode: 0x40 },
    { label: 'F8', type: 'special', keycode: 0x41 },
    { label: 'F9', type: 'special', keycode: 0x42 },
    { label: 'F10', type: 'special', keycode: 0x43 },
    { label: 'F11', type: 'special', keycode: 0x44 },
    { label: 'F12', type: 'special', keycode: 0x45 },
  ],
  // Number row
  [
    { label: '`', shiftLabel: '~', type: 'char', char: '`', shiftChar: '~' },
    { label: '1', shiftLabel: '!', type: 'char', char: '1', shiftChar: '!' },
    { label: '2', shiftLabel: '@', type: 'char', char: '2', shiftChar: '@' },
    { label: '3', shiftLabel: '#', type: 'char', char: '3', shiftChar: '#' },
    { label: '4', shiftLabel: '$', type: 'char', char: '4', shiftChar: '$' },
    { label: '5', shiftLabel: '%', type: 'char', char: '5', shiftChar: '%' },
    { label: '6', shiftLabel: '^', type: 'char', char: '6', shiftChar: '^' },
    { label: '7', shiftLabel: '&', type: 'char', char: '7', shiftChar: '&' },
    { label: '8', shiftLabel: '*', type: 'char', char: '8', shiftChar: '*' },
    { label: '9', shiftLabel: '(', type: 'char', char: '9', shiftChar: '(' },
    { label: '0', shiftLabel: ')', type: 'char', char: '0', shiftChar: ')' },
    { label: '-', shiftLabel: '_', type: 'char', char: '-', shiftChar: '_' },
    { label: '=', shiftLabel: '+', type: 'char', char: '=', shiftChar: '+' },
    { label: 'Bksp', type: 'special', keycode: 0x2A, flex: 1.5 },
  ],
  // QWERTY row
  [
    { label: 'Tab', type: 'special', keycode: 0x2B, flex: 1.3 },
    { label: 'q', shiftLabel: 'Q', type: 'char', char: 'q', shiftChar: 'Q' },
    { label: 'w', shiftLabel: 'W', type: 'char', char: 'w', shiftChar: 'W' },
    { label: 'e', shiftLabel: 'E', type: 'char', char: 'e', shiftChar: 'E' },
    { label: 'r', shiftLabel: 'R', type: 'char', char: 'r', shiftChar: 'R' },
    { label: 't', shiftLabel: 'T', type: 'char', char: 't', shiftChar: 'T' },
    { label: 'y', shiftLabel: 'Y', type: 'char', char: 'y', shiftChar: 'Y' },
    { label: 'u', shiftLabel: 'U', type: 'char', char: 'u', shiftChar: 'U' },
    { label: 'i', shiftLabel: 'I', type: 'char', char: 'i', shiftChar: 'I' },
    { label: 'o', shiftLabel: 'O', type: 'char', char: 'o', shiftChar: 'O' },
    { label: 'p', shiftLabel: 'P', type: 'char', char: 'p', shiftChar: 'P' },
    { label: '[', shiftLabel: '{', type: 'char', char: '[', shiftChar: '{' },
    { label: ']', shiftLabel: '}', type: 'char', char: ']', shiftChar: '}' },
    { label: '\\', shiftLabel: '|', type: 'char', char: '\\', shiftChar: '|' },
  ],
  // Home row
  [
    { label: 'Caps', type: 'caps', keycode: 0x39, flex: 1.5 },
    { label: 'a', shiftLabel: 'A', type: 'char', char: 'a', shiftChar: 'A' },
    { label: 's', shiftLabel: 'S', type: 'char', char: 's', shiftChar: 'S' },
    { label: 'd', shiftLabel: 'D', type: 'char', char: 'd', shiftChar: 'D' },
    { label: 'f', shiftLabel: 'F', type: 'char', char: 'f', shiftChar: 'F' },
    { label: 'g', shiftLabel: 'G', type: 'char', char: 'g', shiftChar: 'G' },
    { label: 'h', shiftLabel: 'H', type: 'char', char: 'h', shiftChar: 'H' },
    { label: 'j', shiftLabel: 'J', type: 'char', char: 'j', shiftChar: 'J' },
    { label: 'k', shiftLabel: 'K', type: 'char', char: 'k', shiftChar: 'K' },
    { label: 'l', shiftLabel: 'L', type: 'char', char: 'l', shiftChar: 'L' },
    { label: ';', shiftLabel: ':', type: 'char', char: ';', shiftChar: ':' },
    { label: "'", shiftLabel: '"', type: 'char', char: "'", shiftChar: '"' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.8 },
  ],
  // Shift row
  [
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 2 },
    { label: 'z', shiftLabel: 'Z', type: 'char', char: 'z', shiftChar: 'Z' },
    { label: 'x', shiftLabel: 'X', type: 'char', char: 'x', shiftChar: 'X' },
    { label: 'c', shiftLabel: 'C', type: 'char', char: 'c', shiftChar: 'C' },
    { label: 'v', shiftLabel: 'V', type: 'char', char: 'v', shiftChar: 'V' },
    { label: 'b', shiftLabel: 'B', type: 'char', char: 'b', shiftChar: 'B' },
    { label: 'n', shiftLabel: 'N', type: 'char', char: 'n', shiftChar: 'N' },
    { label: 'm', shiftLabel: 'M', type: 'char', char: 'm', shiftChar: 'M' },
    { label: ',', shiftLabel: '<', type: 'char', char: ',', shiftChar: '<' },
    { label: '.', shiftLabel: '>', type: 'char', char: '.', shiftChar: '>' },
    { label: '/', shiftLabel: '?', type: 'char', char: '/', shiftChar: '?' },
    { label: 'Shift R', type: 'modifier', mod: 'rshift', bit: 0x20, flex: 2 },
  ],
  // Bottom row
  [
    { label: 'Ctrl', type: 'modifier', mod: 'ctrl', bit: 0x01, flex: 1.2 },
    { label: 'Win', type: 'modifier', mod: 'win', bit: 0x08, flex: 1.2 },
    { label: 'Alt', type: 'modifier', mod: 'alt', bit: 0x04, flex: 1.2 },
    { label: '', type: 'char', char: ' ', shiftChar: ' ', flex: 6 },
    { label: 'Alt R', type: 'modifier', mod: 'altgr', bit: 0x40, flex: 1.2 },
    { label: 'Del', type: 'special', keycode: 0x4C, flex: 1.2 },
    { label: '\u2190', type: 'special', keycode: 0x50 },
    { label: '\u2191', type: 'special', keycode: 0x52 },
    { label: '\u2193', type: 'special', keycode: 0x51 },
    { label: '\u2192', type: 'special', keycode: 0x4F },
  ],
]},
uk: { name: 'English (UK)', ROWS: [
  // F-key row
  [
    { label: 'Esc', type: 'special', keycode: 0x29, flex: 1.2 },
    { label: 'F1', type: 'special', keycode: 0x3A },
    { label: 'F2', type: 'special', keycode: 0x3B },
    { label: 'F3', type: 'special', keycode: 0x3C },
    { label: 'F4', type: 'special', keycode: 0x3D },
    { label: 'F5', type: 'special', keycode: 0x3E },
    { label: 'F6', type: 'special', keycode: 0x3F },
    { label: 'F7', type: 'special', keycode: 0x40 },
    { label: 'F8', type: 'special', keycode: 0x41 },
    { label: 'F9', type: 'special', keycode: 0x42 },
    { label: 'F10', type: 'special', keycode: 0x43 },
    { label: 'F11', type: 'special', keycode: 0x44 },
    { label: 'F12', type: 'special', keycode: 0x45 },
  ],
  // Number row \u2014 UK: Shift+2 = ", Shift+3 = \u00a3 (GBP), Shift+` = \u00ac (negate)
  [
    { label: '`', shiftLabel: '\u00ac', type: 'char', char: '`', shiftChar: '\u00ac' },
    { label: '1', shiftLabel: '!', type: 'char', char: '1', shiftChar: '!' },
    { label: '2', shiftLabel: '"', type: 'char', char: '2', shiftChar: '"' },
    { label: '3', shiftLabel: '\u00a3', type: 'char', char: '3', shiftChar: '\u00a3' },
    { label: '4', shiftLabel: '$', type: 'char', char: '4', shiftChar: '$' },
    { label: '5', shiftLabel: '%', type: 'char', char: '5', shiftChar: '%' },
    { label: '6', shiftLabel: '^', type: 'char', char: '6', shiftChar: '^' },
    { label: '7', shiftLabel: '&', type: 'char', char: '7', shiftChar: '&' },
    { label: '8', shiftLabel: '*', type: 'char', char: '8', shiftChar: '*' },
    { label: '9', shiftLabel: '(', type: 'char', char: '9', shiftChar: '(' },
    { label: '0', shiftLabel: ')', type: 'char', char: '0', shiftChar: ')' },
    { label: '-', shiftLabel: '_', type: 'char', char: '-', shiftChar: '_' },
    { label: '=', shiftLabel: '+', type: 'char', char: '=', shiftChar: '+' },
    { label: 'Bksp', type: 'special', keycode: 0x2A, flex: 1.5 },
  ],
  // QWERTY row \u2014 UK: ends with top portion of L-Enter (sends same Enter keycode)
  [
    { label: 'Tab', type: 'special', keycode: 0x2B, flex: 1.5 },
    { label: 'q', shiftLabel: 'Q', type: 'char', char: 'q', shiftChar: 'Q' },
    { label: 'w', shiftLabel: 'W', type: 'char', char: 'w', shiftChar: 'W' },
    { label: 'e', shiftLabel: 'E', type: 'char', char: 'e', shiftChar: 'E' },
    { label: 'r', shiftLabel: 'R', type: 'char', char: 'r', shiftChar: 'R' },
    { label: 't', shiftLabel: 'T', type: 'char', char: 't', shiftChar: 'T' },
    { label: 'y', shiftLabel: 'Y', type: 'char', char: 'y', shiftChar: 'Y' },
    { label: 'u', shiftLabel: 'U', type: 'char', char: 'u', shiftChar: 'U' },
    { label: 'i', shiftLabel: 'I', type: 'char', char: 'i', shiftChar: 'I' },
    { label: 'o', shiftLabel: 'O', type: 'char', char: 'o', shiftChar: 'O' },
    { label: 'p', shiftLabel: 'P', type: 'char', char: 'p', shiftChar: 'P' },
    { label: '[', shiftLabel: '{', type: 'char', char: '[', shiftChar: '{' },
    { label: ']', shiftLabel: '}', type: 'char', char: ']', shiftChar: '}' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.25, cls: 'kb-l-top' },
  ],
  // Home row \u2014 UK adds #/~ between '@ and Enter; Shift+' = @
  [
    { label: 'Caps', type: 'caps', keycode: 0x39, flex: 1.5 },
    { label: 'a', shiftLabel: 'A', type: 'char', char: 'a', shiftChar: 'A' },
    { label: 's', shiftLabel: 'S', type: 'char', char: 's', shiftChar: 'S' },
    { label: 'd', shiftLabel: 'D', type: 'char', char: 'd', shiftChar: 'D' },
    { label: 'f', shiftLabel: 'F', type: 'char', char: 'f', shiftChar: 'F' },
    { label: 'g', shiftLabel: 'G', type: 'char', char: 'g', shiftChar: 'G' },
    { label: 'h', shiftLabel: 'H', type: 'char', char: 'h', shiftChar: 'H' },
    { label: 'j', shiftLabel: 'J', type: 'char', char: 'j', shiftChar: 'J' },
    { label: 'k', shiftLabel: 'K', type: 'char', char: 'k', shiftChar: 'K' },
    { label: 'l', shiftLabel: 'L', type: 'char', char: 'l', shiftChar: 'L' },
    { label: ';', shiftLabel: ':', type: 'char', char: ';', shiftChar: ':' },
    { label: "'", shiftLabel: '@', type: 'char', char: "'", shiftChar: '@' },
    { label: '#', shiftLabel: '~', type: 'char', char: '#', shiftChar: '~' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.75, cls: 'kb-l-bot' },
  ],
  // Shift row \u2014 UK adds the ISO \| key between LShift and Z; narrower LShift, wider RShift
  [
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 1.25 },
    { label: '\\', shiftLabel: '|', type: 'char', char: '\\', shiftChar: '|' },
    { label: 'z', shiftLabel: 'Z', type: 'char', char: 'z', shiftChar: 'Z' },
    { label: 'x', shiftLabel: 'X', type: 'char', char: 'x', shiftChar: 'X' },
    { label: 'c', shiftLabel: 'C', type: 'char', char: 'c', shiftChar: 'C' },
    { label: 'v', shiftLabel: 'V', type: 'char', char: 'v', shiftChar: 'V' },
    { label: 'b', shiftLabel: 'B', type: 'char', char: 'b', shiftChar: 'B' },
    { label: 'n', shiftLabel: 'N', type: 'char', char: 'n', shiftChar: 'N' },
    { label: 'm', shiftLabel: 'M', type: 'char', char: 'm', shiftChar: 'M' },
    { label: ',', shiftLabel: '<', type: 'char', char: ',', shiftChar: '<' },
    { label: '.', shiftLabel: '>', type: 'char', char: '.', shiftChar: '>' },
    { label: '/', shiftLabel: '?', type: 'char', char: '/', shiftChar: '?' },
    { label: 'Shift R', type: 'modifier', mod: 'rshift', bit: 0x20, flex: 2.75 },
  ],
  // Bottom row
  [
    { label: 'Ctrl', type: 'modifier', mod: 'ctrl', bit: 0x01, flex: 1.2 },
    { label: 'Win', type: 'modifier', mod: 'win', bit: 0x08, flex: 1.2 },
    { label: 'Alt', type: 'modifier', mod: 'alt', bit: 0x04, flex: 1.2 },
    { label: '', type: 'char', char: ' ', shiftChar: ' ', flex: 6 },
    { label: 'Alt R', type: 'modifier', mod: 'altgr', bit: 0x40, flex: 1.2 },
    { label: 'Del', type: 'special', keycode: 0x4C, flex: 1.2 },
    { label: '\u2190', type: 'special', keycode: 0x50 },
    { label: '\u2191', type: 'special', keycode: 0x52 },
    { label: '\u2193', type: 'special', keycode: 0x51 },
    { label: '\u2192', type: 'special', keycode: 0x4F },
  ],
]},
de: { name: 'German (QWERTZ)', ROWS: [
  // F-key row
  [
    { label: 'Esc', type: 'special', keycode: 0x29, flex: 1.2 },
    { label: 'F1', type: 'special', keycode: 0x3A },
    { label: 'F2', type: 'special', keycode: 0x3B },
    { label: 'F3', type: 'special', keycode: 0x3C },
    { label: 'F4', type: 'special', keycode: 0x3D },
    { label: 'F5', type: 'special', keycode: 0x3E },
    { label: 'F6', type: 'special', keycode: 0x3F },
    { label: 'F7', type: 'special', keycode: 0x40 },
    { label: 'F8', type: 'special', keycode: 0x41 },
    { label: 'F9', type: 'special', keycode: 0x42 },
    { label: 'F10', type: 'special', keycode: 0x43 },
    { label: 'F11', type: 'special', keycode: 0x44 },
    { label: 'F12', type: 'special', keycode: 0x45 },
  ],
  // Number row \u2014 DE: ^/\u00b0, Shift+2=", Shift+3=\u00a7, Shift+6=&, Shift+7=/, ... \u00df/?, \u00b4/`
  [
    { label: '^', shiftLabel: '\u00b0', type: 'char', char: '^', shiftChar: '\u00b0' },
    { label: '1', shiftLabel: '!', type: 'char', char: '1', shiftChar: '!' },
    { label: '2', shiftLabel: '"', type: 'char', char: '2', shiftChar: '"', altgrLabel: '\u00b2' },
    { label: '3', shiftLabel: '\u00a7', type: 'char', char: '3', shiftChar: '\u00a7', altgrLabel: '\u00b3' },
    { label: '4', shiftLabel: '$', type: 'char', char: '4', shiftChar: '$' },
    { label: '5', shiftLabel: '%', type: 'char', char: '5', shiftChar: '%' },
    { label: '6', shiftLabel: '&', type: 'char', char: '6', shiftChar: '&' },
    { label: '7', shiftLabel: '/', type: 'char', char: '7', shiftChar: '/', altgrLabel: '{' },
    { label: '8', shiftLabel: '(', type: 'char', char: '8', shiftChar: '(', altgrLabel: '[' },
    { label: '9', shiftLabel: ')', type: 'char', char: '9', shiftChar: ')', altgrLabel: ']' },
    { label: '0', shiftLabel: '=', type: 'char', char: '0', shiftChar: '=', altgrLabel: '}' },
    { label: '\u00df', shiftLabel: '?', type: 'char', char: '\u00df', shiftChar: '?', altgrLabel: '\\' },
    { label: '\u00b4', shiftLabel: '`', type: 'char', char: '\u00b4', shiftChar: '`' },
    { label: 'Bksp', type: 'special', keycode: 0x2A, flex: 1.5 },
  ],
  // QWERTZ row \u2014 DE: z/y swapped, \u00fc/\u00dc, +/* at end
  [
    { label: 'Tab', type: 'special', keycode: 0x2B, flex: 1.5 },
    { label: 'q', shiftLabel: 'Q', type: 'char', char: 'q', shiftChar: 'Q', altgrLabel: '@' },
    { label: 'w', shiftLabel: 'W', type: 'char', char: 'w', shiftChar: 'W' },
    { label: 'e', shiftLabel: 'E', type: 'char', char: 'e', shiftChar: 'E', altgrLabel: '\u20ac' },
    { label: 'r', shiftLabel: 'R', type: 'char', char: 'r', shiftChar: 'R' },
    { label: 't', shiftLabel: 'T', type: 'char', char: 't', shiftChar: 'T' },
    { label: 'z', shiftLabel: 'Z', type: 'char', char: 'z', shiftChar: 'Z' },
    { label: 'u', shiftLabel: 'U', type: 'char', char: 'u', shiftChar: 'U' },
    { label: 'i', shiftLabel: 'I', type: 'char', char: 'i', shiftChar: 'I' },
    { label: 'o', shiftLabel: 'O', type: 'char', char: 'o', shiftChar: 'O' },
    { label: 'p', shiftLabel: 'P', type: 'char', char: 'p', shiftChar: 'P' },
    { label: '\u00fc', shiftLabel: '\u00dc', type: 'char', char: '\u00fc', shiftChar: '\u00dc' },
    { label: '+', shiftLabel: '*', type: 'char', char: '+', shiftChar: '*', altgrLabel: '~' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.25, cls: 'kb-l-top' },
  ],
  // Home row \u2014 DE: \u00f6, \u00e4, # at end
  [
    { label: 'Caps', type: 'caps', keycode: 0x39, flex: 1.5 },
    { label: 'a', shiftLabel: 'A', type: 'char', char: 'a', shiftChar: 'A' },
    { label: 's', shiftLabel: 'S', type: 'char', char: 's', shiftChar: 'S' },
    { label: 'd', shiftLabel: 'D', type: 'char', char: 'd', shiftChar: 'D' },
    { label: 'f', shiftLabel: 'F', type: 'char', char: 'f', shiftChar: 'F' },
    { label: 'g', shiftLabel: 'G', type: 'char', char: 'g', shiftChar: 'G' },
    { label: 'h', shiftLabel: 'H', type: 'char', char: 'h', shiftChar: 'H' },
    { label: 'j', shiftLabel: 'J', type: 'char', char: 'j', shiftChar: 'J' },
    { label: 'k', shiftLabel: 'K', type: 'char', char: 'k', shiftChar: 'K' },
    { label: 'l', shiftLabel: 'L', type: 'char', char: 'l', shiftChar: 'L' },
    { label: '\u00f6', shiftLabel: '\u00d6', type: 'char', char: '\u00f6', shiftChar: '\u00d6' },
    { label: '\u00e4', shiftLabel: '\u00c4', type: 'char', char: '\u00e4', shiftChar: '\u00c4' },
    { label: '#', shiftLabel: "'", type: 'char', char: '#', shiftChar: "'" },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.75, cls: 'kb-l-bot' },
  ],
  // Shift row \u2014 DE: ISO key <|>, y/z swapped (y here)
  [
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 1.25 },
    { label: '<', shiftLabel: '>', type: 'char', char: '<', shiftChar: '>', altgrLabel: '|' },
    { label: 'y', shiftLabel: 'Y', type: 'char', char: 'y', shiftChar: 'Y' },
    { label: 'x', shiftLabel: 'X', type: 'char', char: 'x', shiftChar: 'X' },
    { label: 'c', shiftLabel: 'C', type: 'char', char: 'c', shiftChar: 'C' },
    { label: 'v', shiftLabel: 'V', type: 'char', char: 'v', shiftChar: 'V' },
    { label: 'b', shiftLabel: 'B', type: 'char', char: 'b', shiftChar: 'B' },
    { label: 'n', shiftLabel: 'N', type: 'char', char: 'n', shiftChar: 'N' },
    { label: 'm', shiftLabel: 'M', type: 'char', char: 'm', shiftChar: 'M', altgrLabel: 'µ' },
    { label: ',', shiftLabel: ';', type: 'char', char: ',', shiftChar: ';' },
    { label: '.', shiftLabel: ':', type: 'char', char: '.', shiftChar: ':' },
    { label: '-', shiftLabel: '_', type: 'char', char: '-', shiftChar: '_' },
    { label: 'Shift R', type: 'modifier', mod: 'rshift', bit: 0x20, flex: 2.75 },
  ],
  // Bottom row \u2014 DE labels: Strg (Ctrl), AltGr, Entf (Del)
  [
    { label: 'Strg', type: 'modifier', mod: 'ctrl', bit: 0x01, flex: 1.2 },
    { label: 'Win', type: 'modifier', mod: 'win', bit: 0x08, flex: 1.2 },
    { label: 'Alt', type: 'modifier', mod: 'alt', bit: 0x04, flex: 1.2 },
    { label: '', type: 'char', char: ' ', shiftChar: ' ', flex: 6 },
    { label: 'AltGr', type: 'modifier', mod: 'altgr', bit: 0x40, flex: 1.2 },
    { label: 'Entf', type: 'special', keycode: 0x4C, flex: 1.2 },
    { label: '\u2190', type: 'special', keycode: 0x50 },
    { label: '\u2191', type: 'special', keycode: 0x52 },
    { label: '\u2193', type: 'special', keycode: 0x51 },
    { label: '\u2192', type: 'special', keycode: 0x4F },
  ],
]},
be: { name: 'Belgian (AZERTY)', ROWS: [
  // F-key row
  [
    { label: 'Esc', type: 'special', keycode: 0x29, flex: 1.2 },
    { label: 'F1', type: 'special', keycode: 0x3A },
    { label: 'F2', type: 'special', keycode: 0x3B },
    { label: 'F3', type: 'special', keycode: 0x3C },
    { label: 'F4', type: 'special', keycode: 0x3D },
    { label: 'F5', type: 'special', keycode: 0x3E },
    { label: 'F6', type: 'special', keycode: 0x3F },
    { label: 'F7', type: 'special', keycode: 0x40 },
    { label: 'F8', type: 'special', keycode: 0x41 },
    { label: 'F9', type: 'special', keycode: 0x42 },
    { label: 'F10', type: 'special', keycode: 0x43 },
    { label: 'F11', type: 'special', keycode: 0x44 },
    { label: 'F12', type: 'special', keycode: 0x45 },
  ],
  // Number row \u2014 BE: \u00b2/\u00b3, unshifted gives & \u00e9 " ' ( \u00a7 \u00e8 ! \u00e7 \u00e0
  [
    { label: '\u00b2', shiftLabel: '\u00b3', type: 'char', char: '\u00b2', shiftChar: '\u00b3' },
    { label: '&', shiftLabel: '1', type: 'char', char: '&', shiftChar: '1', altgrLabel: '|' },
    { label: '\u00e9', shiftLabel: '2', type: 'char', char: '\u00e9', shiftChar: '2', altgrLabel: '@' },
    { label: '"', shiftLabel: '3', type: 'char', char: '"', shiftChar: '3', altgrLabel: '#' },
    { label: "'", shiftLabel: '4', type: 'char', char: "'", shiftChar: '4', altgrLabel: '{' },
    { label: '(', shiftLabel: '5', type: 'char', char: '(', shiftChar: '5', altgrLabel: '[' },
    { label: '\u00a7', shiftLabel: '6', type: 'char', char: '\u00a7', shiftChar: '6', altgrLabel: '^' },
    { label: '\u00e8', shiftLabel: '7', type: 'char', char: '\u00e8', shiftChar: '7', altgrLabel: '`' },
    { label: '!', shiftLabel: '8', type: 'char', char: '!', shiftChar: '8' },
    { label: '\u00e7', shiftLabel: '9', type: 'char', char: '\u00e7', shiftChar: '9' },
    { label: '\u00e0', shiftLabel: '0', type: 'char', char: '\u00e0', shiftChar: '0', altgrLabel: '}' },
    { label: ')', shiftLabel: '\u00b0', type: 'char', char: ')', shiftChar: '\u00b0' },
    { label: '-', shiftLabel: '_', type: 'char', char: '-', shiftChar: '_' },
    { label: 'Bksp', type: 'special', keycode: 0x2A, flex: 1.5 },
  ],
  // AZERTY row \u2014 BE: top row is a z e r t y u i o p ^ $
  [
    { label: 'Tab', type: 'special', keycode: 0x2B, flex: 1.5 },
    { label: 'a', shiftLabel: 'A', type: 'char', char: 'a', shiftChar: 'A' },
    { label: 'z', shiftLabel: 'Z', type: 'char', char: 'z', shiftChar: 'Z' },
    { label: 'e', shiftLabel: 'E', type: 'char', char: 'e', shiftChar: 'E', altgrLabel: '\u20ac' },
    { label: 'r', shiftLabel: 'R', type: 'char', char: 'r', shiftChar: 'R' },
    { label: 't', shiftLabel: 'T', type: 'char', char: 't', shiftChar: 'T' },
    { label: 'y', shiftLabel: 'Y', type: 'char', char: 'y', shiftChar: 'Y' },
    { label: 'u', shiftLabel: 'U', type: 'char', char: 'u', shiftChar: 'U' },
    { label: 'i', shiftLabel: 'I', type: 'char', char: 'i', shiftChar: 'I' },
    { label: 'o', shiftLabel: 'O', type: 'char', char: 'o', shiftChar: 'O' },
    { label: 'p', shiftLabel: 'P', type: 'char', char: 'p', shiftChar: 'P' },
    { label: '^', shiftLabel: '\u00a8', type: 'char', char: '^', shiftChar: '\u00a8', altgrLabel: '[' },
    { label: '$', shiftLabel: '*', type: 'char', char: '$', shiftChar: '*', altgrLabel: ']' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.25, cls: 'kb-l-top' },
  ],
  // Home row \u2014 BE: q s d f g h j k l m \u00f9 \u00b5
  [
    { label: 'Caps', type: 'caps', keycode: 0x39, flex: 1.5 },
    { label: 'q', shiftLabel: 'Q', type: 'char', char: 'q', shiftChar: 'Q' },
    { label: 's', shiftLabel: 'S', type: 'char', char: 's', shiftChar: 'S' },
    { label: 'd', shiftLabel: 'D', type: 'char', char: 'd', shiftChar: 'D' },
    { label: 'f', shiftLabel: 'F', type: 'char', char: 'f', shiftChar: 'F' },
    { label: 'g', shiftLabel: 'G', type: 'char', char: 'g', shiftChar: 'G' },
    { label: 'h', shiftLabel: 'H', type: 'char', char: 'h', shiftChar: 'H' },
    { label: 'j', shiftLabel: 'J', type: 'char', char: 'j', shiftChar: 'J' },
    { label: 'k', shiftLabel: 'K', type: 'char', char: 'k', shiftChar: 'K' },
    { label: 'l', shiftLabel: 'L', type: 'char', char: 'l', shiftChar: 'L' },
    { label: 'm', shiftLabel: 'M', type: 'char', char: 'm', shiftChar: 'M' },
    { label: '\u00f9', shiftLabel: '%', type: 'char', char: '\u00f9', shiftChar: '%' },
    { label: '\u00b5', shiftLabel: '\u00a3', type: 'char', char: '\u00b5', shiftChar: '\u00a3' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.75, cls: 'kb-l-bot' },
  ],
  // Shift row \u2014 BE: ISO key <|>, w x c v b n , ; : =
  [
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 1.25 },
    { label: '<', shiftLabel: '>', type: 'char', char: '<', shiftChar: '>', altgrLabel: '\\' },
    { label: 'w', shiftLabel: 'W', type: 'char', char: 'w', shiftChar: 'W' },
    { label: 'x', shiftLabel: 'X', type: 'char', char: 'x', shiftChar: 'X' },
    { label: 'c', shiftLabel: 'C', type: 'char', char: 'c', shiftChar: 'C' },
    { label: 'v', shiftLabel: 'V', type: 'char', char: 'v', shiftChar: 'V' },
    { label: 'b', shiftLabel: 'B', type: 'char', char: 'b', shiftChar: 'B' },
    { label: 'n', shiftLabel: 'N', type: 'char', char: 'n', shiftChar: 'N' },
    { label: ',', shiftLabel: '?', type: 'char', char: ',', shiftChar: '?' },
    { label: ';', shiftLabel: '.', type: 'char', char: ';', shiftChar: '.' },
    { label: ':', shiftLabel: '/', type: 'char', char: ':', shiftChar: '/' },
    { label: '=', shiftLabel: '+', type: 'char', char: '=', shiftChar: '+' },
    { label: 'Shift R', type: 'modifier', mod: 'rshift', bit: 0x20, flex: 2.75 },
  ],
  // Bottom row \u2014 BE labels (French): AltGr, Suppr (Del)
  [
    { label: 'Ctrl', type: 'modifier', mod: 'ctrl', bit: 0x01, flex: 1.2 },
    { label: 'Win', type: 'modifier', mod: 'win', bit: 0x08, flex: 1.2 },
    { label: 'Alt', type: 'modifier', mod: 'alt', bit: 0x04, flex: 1.2 },
    { label: '', type: 'char', char: ' ', shiftChar: ' ', flex: 6 },
    { label: 'AltGr', type: 'modifier', mod: 'altgr', bit: 0x40, flex: 1.2 },
    { label: 'Suppr', type: 'special', keycode: 0x4C, flex: 1.2 },
    { label: '\u2190', type: 'special', keycode: 0x50 },
    { label: '\u2191', type: 'special', keycode: 0x52 },
    { label: '\u2193', type: 'special', keycode: 0x51 },
    { label: '\u2192', type: 'special', keycode: 0x4F },
  ],
]},
};

class BleKeyboardCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
    // Track active host changes via HA sensor entity. The firmware publishes to
    // this sensor on every switch_host() path — HA service, the device's own web
    // UI, a physical button — so every card following it stays in step with the
    // others without polling.
    if (this._config && this._config.host_slots > 1) {
      const entity = this._config.active_host_entity
        || Object.keys(hass.states).find(eid =>
             eid.startsWith('sensor.') && eid.includes(this._config.device) && eid.endsWith('_active_host')
           );
      this._hasActiveHostEntity = !!(entity && hass.states[entity]);
      if (this._hasActiveHostEntity) {
        const val = parseInt(hass.states[entity].state, 10);
        if (!isNaN(val) && val !== this._activeSlot) {
          this._activeSlot = val;
          this._updateHostDisplay();
        }
      }
    }
  }

  setConfig(config) {
    if (!config.device) {
      throw new Error('Please define a "device" (your ESPHome device name)');
    }
    const layout = (config.layout || 'us').toLowerCase();
    this._config = {
      device: config.device,
      name: config.name || null,
      show_fkeys: config.show_fkeys !== false,
      show_paste: config.show_paste !== false,
      layout: LAYOUTS[layout] ? layout : 'us',
      host_slots: config.host_slots || 0,
      host_names: config.host_names || [],
      active_host_entity: config.active_host_entity || null,
      show_mac: config.show_mac !== false,
      host_url: config.host_url || null,
      zoom: this._parseZoom(config.zoom),
    };
    // Read by the .zoom wrapper. Set on the host so it applies whether or not
    // the card has rendered yet — custom properties inherit into shadow DOM.
    this.style.setProperty('--kb-zoom', this._config.zoom);
  }

  // Anything unparseable falls back to 1 rather than collapsing the card.
  _parseZoom(value) {
    const z = parseFloat(value);
    return Number.isFinite(z) ? Math.min(Math.max(z, 0.25), 3) : 1;
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    this._shift = false;
    this._capsLock = false;
    this._ctrl = false;
    this._alt = false;
    this._win = false;
    this._rshift = false;
    this._altgr = false;

    const shadow = this.attachShadow({ mode: 'open' });

    const style = document.createElement('style');
    style.textContent = `
      :host {
        display: block;
        height: 100%;
      }
      .card {
        background: var(--ha-card-background, var(--card-background-color, #fff));
        border-radius: var(--ha-card-border-radius, 12px);
        box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
        padding: 12px;
        color: var(--primary-text-color);
        user-select: none;
        -webkit-user-select: none;
        box-sizing: border-box;
        height: 100%;
        display: flex;
        flex-direction: column;
        /* The key rows stretch to fill a taller card, but the keys have a
           minimum height — squeeze the card below that, or zoom in past what
           the width can hold, and this scrolls rather than letting the keyboard
           spill over its neighbours. */
        overflow: auto;
      }
      /* zoom rather than a transform, because it affects layout: the card's
         natural height tracks the zoom, so auto height still fits exactly.
         Stretching as a flex item is what keeps the key rows filling a taller
         card at any zoom. */
      .zoom {
        zoom: var(--kb-zoom, 1);
        display: flex;
        flex-direction: column;
        flex: 0 0 auto;
        /* Inside a zoomed element, 100% is the card's width divided by the
           zoom, so a flex:1 key would come out the same size on screen at every
           zoom — only the font would change. Multiplying back by the zoom pins
           the layout to the card's unzoomed width, so every length scales by
           exactly the zoom factor in both directions. It has to be an exact
           width rather than a min-width: a floor does nothing below zoom 1,
           which
           left small zooms full-width with shrunken text. Auto margins centre
           the result, and collapse to zero when it overflows, so nothing is
           pushed out of scroll range. */
        width: calc(100% * var(--kb-zoom, 1));
        /* Floor the block at the width its contents actually need, so every part
           of the card shares one width and the whole thing scrolls together.
           Without it the header (which cannot shrink) sets the scroll range while
           the key rows track the narrower visible width, so scrolling revealed a
           squashed keyboard instead of a full-size one. This is a content floor,
           not zoom scaling: the width above still does the zoom pinning, and
           min-content is measured inside the zoomed box so it scales too.
           Shrinking the card to fit a small slot is what the zoom option is for. */
        min-width: min-content;
        margin-inline: auto;
      }
      .header {
        font-size: 16px;
        font-weight: 500;
        margin-bottom: 8px;
        display: flex;
        align-items: center;
        gap: 8px;
        flex: 0 0 auto;
      }
      .paste-bar {
        display: ${this._config.show_paste ? 'flex' : 'none'};
        align-items: center;
        gap: 4px;
        flex: 0 0 auto;
        margin-bottom: 8px;
      }
      .paste-bar textarea {
        flex: 1;
        min-width: 0;
        resize: none;
        /* Tall enough for the text line plus the horizontal scrollbar long
           text brings in - at 26px that scrollbar ate the line's bottom. */
        height: 40px;
        box-sizing: border-box;
        padding: 4px 8px;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 6px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 12px;
        font-family: inherit;
        font-weight: 400;
        line-height: 1.4;
        white-space: pre;
        overflow-x: auto;
        overflow-y: hidden;
        scrollbar-width: thin;
      }
      .paste-bar textarea::-webkit-scrollbar {
        height: 8px;
      }
      .paste-bar textarea::-webkit-scrollbar-thumb {
        background: var(--divider-color, #e0e0e0);
        border-radius: 4px;
      }
      .paste-bar textarea:focus {
        outline: none;
        border-color: var(--primary-color, #03a9f4);
      }
      .paste-auto {
        display: flex;
        align-items: center;
        gap: 3px;
        font-size: 11px;
        font-weight: 400;
        color: var(--secondary-text-color, #727272);
        cursor: pointer;
        user-select: none;
        white-space: nowrap;
      }
      .paste-auto input {
        margin: 0;
        accent-color: var(--primary-color, #03a9f4);
        cursor: pointer;
      }
      .paste-btn {
        height: 26px;
        padding: 0 10px;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 6px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 12px;
        font-family: inherit;
        cursor: pointer;
        white-space: nowrap;
        touch-action: manipulation;
      }
      .paste-btn:active {
        background: var(--primary-color, #03a9f4);
        color: #fff;
        border-color: var(--primary-color, #03a9f4);
      }
      .paste-btn:disabled {
        opacity: 0.55;
        cursor: default;
      }
      .header svg {
        width: 20px;
        height: 20px;
        fill: var(--primary-text-color);
        opacity: 0.7;
      }
      .kb-row {
        display: flex;
        gap: 3px;
        margin-bottom: 3px;
        /* Deliberately not stretching to fill a taller card: that grew the keys
           vertically without growing them sideways, so the zoom no longer kept
           its proportions. Height comes from the keys, scaled by the zoom. */
        flex: 0 0 auto;
      }
      .kb-row:last-child {
        margin-bottom: 0;
      }
      .key {
        flex: 1;
        /* A key must not be squeezed below its own label, or wide ones clip to
           "Shift R" -> "Shift" and the row stops contributing a real minimum for
           the block width above. This has to be spelled out: the automatic
           minimum a flex item would otherwise get is forced to zero by the
           overflow: hidden below, so leaving min-width off achieves nothing. */
        min-width: min-content;
        padding: 0 2px;
        min-height: 34px;
        box-sizing: border-box;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 6px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 13px;
        font-weight: 500;
        cursor: pointer;
        text-align: center;
        touch-action: manipulation;
        transition: background 0.08s, border-color 0.08s;
        line-height: 1.2;
        overflow: hidden;
        white-space: nowrap;
        outline: none;
        -webkit-tap-highlight-color: transparent;
        position: relative;
      }
      .key[data-altgr-label]::after {
        content: attr(data-altgr-label);
        position: absolute;
        bottom: 1px;
        right: 4px;
        font-size: 10px;
        opacity: 0.55;
        pointer-events: none;
      }
      .key:focus, .key:focus-visible { outline: none; }
      .key:active, .key.pressed {
        background: var(--primary-color, #03a9f4);
        color: #fff;
        border-color: var(--primary-color, #03a9f4);
      }
      .key.active {
        background: var(--primary-color, #03a9f4);
        color: #fff;
        border-color: var(--primary-color, #03a9f4);
      }
      .key.caps-active {
        background: var(--warning-color, #ff9800);
        color: #fff;
        border-color: var(--warning-color, #ff9800);
      }
      .key-fkey {
        font-size: 11px;
        padding: 0 2px;
        min-height: 26px;
      }
      .key.kb-l-top {
        border-bottom-left-radius: 0;
        border-bottom-right-radius: 0;
        border-bottom-color: transparent;
        position: relative;
        z-index: 1;
        overflow: visible;
      }
      .key.kb-l-top::after {
        content: '';
        position: absolute;
        top: 100%;
        left: -1px;
        right: -1px;
        height: 5px;
        background: var(--secondary-background-color, #f0f0f0);
        border-left: 1px solid var(--divider-color, #e0e0e0);
        border-right: 1px solid var(--divider-color, #e0e0e0);
        pointer-events: none;
      }
      .key.kb-l-top:active::after, .key.kb-l-top.pressed::after {
        background: var(--primary-color, #03a9f4);
        border-color: var(--primary-color, #03a9f4);
      }
      .key.kb-l-bot {
        border-top-left-radius: 0;
      }
      .key-space {
        font-size: 11px;
        letter-spacing: 2px;
      }
      .fkey-row {
        display: ${this._config.show_fkeys ? 'flex' : 'none'};
      }
      .header-right {
        margin-left: auto;
        display: flex;
        align-items: center;
        gap: 6px;
      }
      .host-btn {
        width: 24px;
        height: 24px;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 4px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 12px;
        font-weight: 700;
        cursor: pointer;
        display: flex;
        align-items: center;
        justify-content: center;
        touch-action: manipulation;
        padding: 0;
      }
      .host-btn:active {
        background: var(--primary-color, #03a9f4);
        color: #fff;
      }
      .host-info {
        text-align: center;
        /* Fixed, not min-width:0: the arrows must not move as the name changes
           length, or stepping through hosts walks the button out from under
           your finger. A longer name ellipses instead of pushing them. */
        width: 84px;
        flex: 0 0 84px;
      }
      .host-name {
        font-size: 12px;
        font-weight: 600;
        color: var(--primary-text-color);
        line-height: 1.2;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      }
      .host-addr {
        font-size: 12px;
        font-weight: 400;
        color: var(--secondary-text-color, #888);
        font-family: monospace;
        white-space: nowrap;
      /* 17ch is exactly a MAC in this monospace font, so the field keeps its
         width whether it shows an address or "Empty". */
      width: 17ch;
      text-align: right;
      overflow: hidden;
      }
    `;

    const card = document.createElement('div');
    card.className = 'card';

    // Everything sits inside the zoom wrapper; the card itself must stay
    // unzoomed so its height keeps matching the slot the section gives it.
    const zoom = document.createElement('div');
    zoom.className = 'zoom';
    card.appendChild(zoom);

    // Header
    const header = document.createElement('div');
    header.className = 'header';
    const defaultName = 'BLE Keyboard';
    header.innerHTML = `
      <svg viewBox="0 0 24 24"><path d="M19 10h-2V8h2v2zm0 4h-2v-2h2v2zm-4-4h-2V8h2v2zm0 4h-2v-2h2v2zm0 4H9v-2h6v2zm-8-8H5V8h2v2zm0 4H5v-2h2v2zM20 5H4c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2z"/></svg>
      <span class="header-name">${this._config.name || defaultName}</span>
    `;
    // Host switcher in header
    if (this._config.host_slots > 1) {
      this._activeSlot = 0;
      this._hostSlots = [];

      const hostRight = document.createElement('div');
      hostRight.className = 'header-right';

      const prevBtn = document.createElement('button');
      prevBtn.className = 'host-btn';
      prevBtn.textContent = '\u25C0';
      prevBtn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this._switchHost((this._activeSlot - 1 + this._config.host_slots) % this._config.host_slots);
      });

      const nextBtn = document.createElement('button');
      nextBtn.className = 'host-btn';
      nextBtn.textContent = '\u25B6';
      nextBtn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this._switchHost((this._activeSlot + 1) % this._config.host_slots);
      });

      const hostInfo = document.createElement('div');
      hostInfo.className = 'host-info';
      this._hostNameEl = document.createElement('div');
      this._hostNameEl.className = 'host-name';
      hostInfo.appendChild(this._hostNameEl);

      // MAC sits to the left of the selector, outside .host-info, so the name
      // stays vertically centred between the arrows whether or not it's shown.
      this._hostAddrEl = document.createElement('span');
      this._hostAddrEl.className = 'host-addr';

      hostRight.appendChild(this._hostAddrEl);
      hostRight.appendChild(prevBtn);
      hostRight.appendChild(hostInfo);
      hostRight.appendChild(nextBtn);
      header.appendChild(hostRight);

      this._startHostPolling();
    }

    zoom.appendChild(header);

    // Paste bar — its own line between the header and the keys; hidden via
    // CSS when show_paste is off (same pattern as the F-key row).
    zoom.appendChild(this._buildPasteBar());
    // One device-registry lookup serves two purposes: the friendly name (when no
    // title was configured) and the ESP's own URL, which HA fills in as
    // configuration_url because web_control requires the web_server component.
    const wantsName = !this._config.name;
    const wantsUrl = this._config.host_slots > 1 && !this._config.host_url;
    if ((wantsName || wantsUrl) && this._hass) {
      const nameSpan = header.querySelector('.header-name');
      const slug = this._config.device.replace(/-/g, '_');
      this._hass.callWS({ type: 'config/device_registry/list' }).then(devices => {
        const dev = devices.find(d => d.name_by_user
          ? d.name_by_user.replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug
          : (d.name || '').replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug);
        if (!dev) return;
        if (wantsName) nameSpan.textContent = dev.name_by_user || dev.name;
        if (wantsUrl && dev.configuration_url) {
          this._deviceUrl = dev.configuration_url;
          this._pollHosts();   // the first poll ran before the URL was known
        }
      }).catch(() => { /* keep default */ });
    }

    // Store key elements for label updates
    this._charKeys = [];
    this._modifierBtns = { shift: [], ctrl: [], alt: [], win: [], rshift: [], altgr: [] };

    // Build keyboard rows
    const rows = (LAYOUTS[this._config.layout] || LAYOUTS.us).ROWS;
    rows.forEach((row, rowIdx) => {
      const rowDiv = document.createElement('div');
      rowDiv.className = rowIdx === 0 ? 'kb-row fkey-row' : 'kb-row';

      row.forEach((keyDef, keyIdx) => {
        const btn = document.createElement('button');
        btn.className = 'key';
        if (rowIdx === 0) btn.classList.add('key-fkey');
        if (keyDef.char === ' ') btn.classList.add('key-space');
        if (keyDef.cls) btn.classList.add(keyDef.cls);
        if (keyDef.flex) btn.style.flex = keyDef.flex;

        btn.textContent = keyDef.char === ' ' ? 'Space' : keyDef.label;
        btn.dataset.row = rowIdx;
        btn.dataset.key = keyIdx;
        if (keyDef.altgrLabel) btn.dataset.altgrLabel = keyDef.altgrLabel;

        // Track char keys for label updates
        if (keyDef.type === 'char' && keyDef.shiftLabel) {
          this._charKeys.push({ btn, keyDef });
        }

        // Track modifier buttons
        if (keyDef.type === 'modifier' && this._modifierBtns[keyDef.mod]) {
          this._modifierBtns[keyDef.mod].push(btn);
        }

        // Track caps button
        if (keyDef.type === 'caps') {
          this._capsBtn = btn;
        }

        rowDiv.appendChild(btn);
      });

      zoom.appendChild(rowDiv);
    });

    shadow.appendChild(style);
    shadow.appendChild(card);

    // Delegated event handler
    card.addEventListener('pointerdown', (e) => {
      const btn = e.target.closest('.key');
      if (!btn) return;
      e.preventDefault();

      const rowIdx = parseInt(btn.dataset.row);
      const keyIdx = parseInt(btn.dataset.key);
      const rows = (LAYOUTS[this._config.layout] || LAYOUTS.us).ROWS;
      const keyDef = rows[rowIdx][keyIdx];

      // Visual press feedback — L-Enter halves stay in sync so the full L
      // (top + bridge + bottom) highlights as one when either half is tapped.
      let partner = null;
      if (btn.classList.contains('kb-l-top')) partner = shadow.querySelector('.key.kb-l-bot');
      else if (btn.classList.contains('kb-l-bot')) partner = shadow.querySelector('.key.kb-l-top');
      btn.classList.add('pressed');
      if (partner) partner.classList.add('pressed');
      setTimeout(() => {
        btn.classList.remove('pressed');
        if (partner) partner.classList.remove('pressed');
      }, 120);

      this._onKeyPress(keyDef);
    });
  }

  _sendString(text) {
    if (!this._hass) return;
    // Returned so the paste bar can react to success/failure; key taps ignore it.
    return this._hass.callService('esphome', `${this._config.device}_send_string`, { keys: text });
  }

  _buildPasteBar() {
    const bar = document.createElement('div');
    bar.className = 'paste-bar';

    const ta = document.createElement('textarea');
    ta.rows = 1;
    ta.maxLength = 5000;
    ta.placeholder = 'Paste text to type…';
    ta.setAttribute('autocapitalize', 'off');
    ta.spellcheck = false;

    const clipBtn = document.createElement('button');
    clipBtn.className = 'paste-btn';
    clipBtn.textContent = '📋';
    clipBtn.title = 'Read clipboard & type it';
    clipBtn.style.display = 'none';

    const autoLabel = document.createElement('label');
    autoLabel.className = 'paste-auto';
    autoLabel.title = 'Type pasted text immediately';
    const autoCk = document.createElement('input');
    autoCk.type = 'checkbox';
    autoCk.checked = localStorage.getItem('blekb_card_paste_auto') === '1';
    autoCk.addEventListener('change', () => {
      localStorage.setItem('blekb_card_paste_auto', autoCk.checked ? '1' : '0');
    });
    autoLabel.appendChild(autoCk);
    autoLabel.appendChild(document.createTextNode('auto'));

    const sendBtn = document.createElement('button');
    sendBtn.className = 'paste-btn';
    sendBtn.textContent = 'Send';

    const send = (text, fromField) => {
      // CRLF and bare CR both become LF - the firmware maps BOTH to Enter, so
      // Windows clipboard text would double-Enter without this. Unlike the
      // device web page there is no chunking: the ESPHome native API carries
      // the whole string in one service call.
      text = text.replace(/\r\n/g, '\n').replace(/\r/g, '\n');
      if (!text) return;
      if (text.length > 5000) {
        if (!confirm('Text is ' + text.length + ' characters; only the first 5000 will be typed. Continue?')) return;
        text = text.slice(0, 5000);
      }
      sendBtn.disabled = true;
      clipBtn.disabled = true;
      Promise.resolve(this._sendString(text)).then(() => {
        if (fromField) ta.value = '';
      }).catch((e) => {
        alert('Send failed - ' + (e && e.message ? e.message : e));
      }).finally(() => {
        sendBtn.disabled = false;
        clipBtn.disabled = false;
      });
    };

    sendBtn.addEventListener('click', () => send(ta.value, true));
    // Physical Enter sends; Shift+Enter inserts a newline.
    ta.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        send(ta.value, true);
      }
    });
    // Auto mode: grab the text straight from the paste event (works on plain
    // HTTP origins where navigator.clipboard does not exist) and type it.
    ta.addEventListener('paste', (e) => {
      if (!autoCk.checked) return;
      const t = (e.clipboardData || window.clipboardData).getData('text');
      if (t) {
        e.preventDefault();
        send(t, false);
      }
    });
    // One-tap clipboard read - the API only exists on secure origins, so the
    // button stays hidden when HA is reached over plain HTTP.
    if (navigator.clipboard && navigator.clipboard.readText) {
      clipBtn.style.display = '';
      clipBtn.addEventListener('click', () => {
        navigator.clipboard.readText().then((t) => { if (t) send(t, false); }).catch(() => ta.focus());
      });
    }

    bar.appendChild(ta);
    bar.appendChild(clipBtn);
    bar.appendChild(autoLabel);
    bar.appendChild(sendBtn);
    return bar;
  }

  _sendKey(modifier, keycode) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_key`, { modifier, keycode });
  }

  _onKeyPress(keyDef) {
    if (keyDef.type === 'modifier') {
      this._toggleModifier(keyDef.mod);
      return;
    }

    if (keyDef.type === 'caps') {
      this._capsLock = !this._capsLock;
      if (this._capsBtn) {
        this._capsBtn.classList.toggle('caps-active', this._capsLock);
      }
      this._sendKey(0x00, 0x39);
      this._updateKeyLabels();
      return;
    }

    // Build modifier bitmask
    let modBits = 0;
    if (this._ctrl) modBits |= 0x01;
    if (this._alt) modBits |= 0x04;
    if (this._win) modBits |= 0x08;
    if (this._altgr) modBits |= 0x40;
    if (this._rshift) modBits |= 0x20;

    if (keyDef.type === 'char') {
      if (modBits !== 0) {
        // Modifier combo — send as keycode
        if (this._shift) modBits |= 0x02;
        const code = CHAR_TO_KEYCODE[keyDef.char];
        if (code !== undefined) {
          this._sendKey(modBits, code);
        }
      } else {
        // Pure typing — use send_string
        const isLetter = keyDef.char >= 'a' && keyDef.char <= 'z';
        const anyShift = this._shift || this._rshift;
        let shifted;
        if (isLetter) {
          shifted = anyShift !== this._capsLock; // XOR
        } else {
          shifted = anyShift;
        }
        const ch = shifted ? keyDef.shiftChar : keyDef.char;
        this._sendString(ch);
      }
    } else if (keyDef.type === 'special') {
      if (this._shift) modBits |= 0x02;
      this._sendKey(modBits, keyDef.keycode);
    }

    // Auto-release one-shot modifiers (not caps lock)
    if (this._shift) this._toggleModifier('shift');
    if (this._rshift) this._toggleModifier('rshift');
    if (this._ctrl) this._toggleModifier('ctrl');
    if (this._alt) this._toggleModifier('alt');
    if (this._win) this._toggleModifier('win');
    if (this._altgr) this._toggleModifier('altgr');
  }

  _toggleModifier(mod) {
    this[`_${mod}`] = !this[`_${mod}`];
    const active = this[`_${mod}`];

    if (this._modifierBtns[mod]) {
      this._modifierBtns[mod].forEach(btn => btn.classList.toggle('active', active));
    }

    if (mod === 'shift' || mod === 'rshift') {
      this._updateKeyLabels();
    }
  }

  _updateKeyLabels() {
    const sh = (this._shift || this._rshift) !== this._capsLock;
    this._charKeys.forEach(({ btn, keyDef }) => {
      const isLetter = keyDef.char >= 'a' && keyDef.char <= 'z';
      if (isLetter) {
        btn.textContent = sh ? keyDef.shiftLabel : keyDef.label;
      } else {
        btn.textContent = (this._shift || this._rshift) ? keyDef.shiftLabel : keyDef.label;
      }
    });
  }

  _switchHost(slot) {
    if (!this._hass) return;
    this._activeSlot = slot;
    const slug = this._config.device.replace(/-/g, '_');
    this._hass.callService('esphome', `${slug}_switch_host`, { slot });
    this._updateHostDisplay();
  }

  // Slot metadata (MAC, ESP-side name, occupied) comes straight from the device.
  // /hosts returns every slot at once, so a host switch repaints from this cache
  // with no refetch — the poll only exists to notice new or forgotten bonds,
  // which is why it runs every 30s rather than continuously.
  _startHostPolling() {
    if (this._hostPollInterval) return;
    this._pollHosts();
    this._hostPollInterval = setInterval(() => this._pollHosts(), 30000);
  }

  connectedCallback() {
    if (this._initialized && this._config && this._config.host_slots > 1) {
      this._startHostPolling();
    }
  }

  disconnectedCallback() {
    clearInterval(this._hostPollInterval);
    this._hostPollInterval = null;
  }

  // Resolves the ESP's base URL: an explicit host_url wins, otherwise the
  // configuration_url HA recorded for the device.
  _hostBaseUrl() {
    const url = this._config.host_url || this._deviceUrl;
    return url ? url.replace(/\/+$/, '').replace(/\/ble_keyboard$/, '') : '';
  }

  _pollHosts() {
    if (!this._hass || !this._config.host_slots) return;
    const baseUrl = this._hostBaseUrl();
    if (!baseUrl) {
      this._updateHostDisplay();
      return;
    }
    // The endpoint sends Access-Control-Allow-Origin: *, so a normal cors fetch
    // works. It still fails when HA itself is served over https — the browser
    // blocks the http request as mixed content — hence the quiet catch.
    fetch(baseUrl + '/api/ble_keyboard/hosts', { signal: AbortSignal.timeout(3000) })
      .then(r => { if (!r.ok) throw new Error(); return r.json(); })
      .then(data => {
        // The active-host sensor is the faster and authoritative source; only
        // trust the poll's own idea of the active slot when that sensor is
        // absent, otherwise an in-flight response can undo a fresh switch.
        if (!this._hasActiveHostEntity && typeof data.active === 'number') {
          this._activeSlot = data.active;
        }
        this._hostSlots = data.slots || [];
        this._hostDataAvailable = true;
        this._updateHostDisplay();
      })
      .catch(() => this._updateHostDisplay());
  }

  _updateHostDisplay() {
    if (!this._hostNameEl) return;
    const names = this._config.host_names;
    const apiSlot = this._hostSlots.find(s => s.slot === this._activeSlot);
    this._hostNameEl.textContent = (names && names[this._activeSlot])
      ? names[this._activeSlot]
      : (apiSlot && apiSlot.name) ? apiSlot.name
      : 'Host ' + (this._activeSlot + 1);
    // Without device data there's nothing truthful to show, so the element
    // collapses rather than leaving a blank gap beside the arrows.
    const showAddr = this._config.show_mac && this._hostDataAvailable;
    this._hostAddrEl.style.display = showAddr ? '' : 'none';
    if (showAddr) {
      // identity is the address the host keeps across reconnects and is what the
      // host MAC sensor publishes; addr is what it happened to connect with and
      // rotates on Android. Prefer identity, fall back when it can't be resolved.
      const addr = apiSlot && apiSlot.occupied && (apiSlot.identity || apiSlot.addr);
      this._hostAddrEl.textContent = addr || 'Empty';
    }
  }

  getCardSize() {
    return this._config && this._config.show_fkeys !== false ? 6 : 5;
  }

  // Sections-view sizing. 'auto' sizes the section to the keyboard's natural
  // height, the way it laid out before it declared any grid options; the key
  // rows still flex, so a taller size just grows the keys and a shorter one
  // scrolls. One grid row is 56px with an 8px gap, so n rows give 64n-8 px:
  // natural height is ~271px with F-keys, ~242px without.
  //
  // The bounds are left wide open — 1-12 columns, and 1-8 rows, 8 being as far
  // as HA's height slider goes. max_rows is deliberately absent: the slider
  // stops at 8 either way (rowMax falls back to the size picker's own `rows`),
  // but with no bound declared computeCardGridSize() applies no upper clamp, so
  // `grid_options: {rows: N}` in the YAML editor can go taller.
  getGridOptions() {
    return {
      columns: 12,
      min_columns: 1,
      rows: 'auto',
      min_rows: 1,
    };
  }

  // Enables the dashboard's visual editor; YAML editing is unaffected.
  static getConfigElement() {
    return document.createElement('ble-keyboard-card-editor');
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-keyboard-card', BleKeyboardCard);

/* ── Visual editor ───────────────────────────────────────────────────────
 * Prefers Home Assistant's own <ha-form> so the panel matches built-in cards,
 * falling back to plain inputs if ha-form isn't registered.
 *
 * `host_names` is a YAML list, so it edits here as one comma-separated field
 * and is converted back to a list on the way out (see setConfig / _emit).
 * ---------------------------------------------------------------------- */

const KB_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'zoom', selector: { number: { min: 0.25, max: 3, step: 0.05, mode: 'box' } } },
  { name: 'layout', selector: { select: { options: [
    { value: 'us', label: 'English (US)' },
    { value: 'uk', label: 'English (UK)' },
    { value: 'de', label: 'German' },
    { value: 'be', label: 'Belgian' },
  ], mode: 'dropdown' } } },
  { name: 'show_fkeys', selector: { boolean: {} } },
  { name: 'show_paste', selector: { boolean: {} } },
  { name: 'host_slots', selector: { number: { min: 0, max: 10, step: 1, mode: 'box' } } },
  { name: 'host_names', selector: { text: {} } },
  { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'show_mac', selector: { boolean: {} } },
  { name: 'host_url', selector: { text: {} } },
];

const KB_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  zoom: 'Zoom (1 = normal, 0.5 = half, 2 = double)',
  layout: 'Keyboard layout',
  show_fkeys: 'Show function key row',
  show_paste: 'Show paste bar',
  host_slots: 'Host switcher (needs 2+; 0 = hide)',
  host_names: 'Host names, comma-separated (optional)',
  active_host_entity: 'Active-host sensor (optional)',
  show_mac: 'Show host MAC address',
  host_url: 'Device URL (optional, auto-detected)',
};

class BleKeyboardCardEditor extends HTMLElement {
  setConfig(config) {
    this._config = { layout: 'us', show_fkeys: true, show_paste: true, host_slots: 0, show_mac: true, zoom: 1, ...config };
    // host_names is a YAML list but edits as one comma-separated field; show it
    // as text here and turn it back into a list in _emit().
    if (Array.isArray(this._config.host_names)) {
      this._config.host_names = this._config.host_names.join(', ');
    }
    this._render();
  }

  set hass(hass) {
    this._hass = hass;
    if (this._form) this._form.hass = hass;
  }

  _emit(config) {
    this._config = config;
    // Hand the card a real list again — it expects host_names to be an array.
    const out = { ...config };
    if (typeof out.host_names === 'string') {
      const names = out.host_names.split(',').map((n) => n.trim()).filter(Boolean);
      if (names.length) out.host_names = names;
      else delete out.host_names;
    }
    this.dispatchEvent(new CustomEvent('config-changed', {
      detail: { config: out },
      bubbles: true,
      composed: true,
    }));
  }

  _render() {
    if (this._rendered) {
      if (this._form) this._form.data = this._config;
      return;
    }
    this._rendered = true;

    if (customElements.get('ha-form')) {
      const form = document.createElement('ha-form');
      form.hass = this._hass;
      form.data = this._config;
      form.schema = KB_EDITOR_SCHEMA;
      form.computeLabel = (s) => KB_EDITOR_LABELS[s.name] || s.name;
      form.addEventListener('value-changed', (e) => this._emit(e.detail.value));
      this.appendChild(form);
      this._form = form;
      return;
    }

    this.appendChild(buildKbFallbackEditor(
      KB_EDITOR_SCHEMA, KB_EDITOR_LABELS, () => this._config, (cfg) => this._emit(cfg),
    ));
  }
}

// `getConfig` is a function, not an object: _emit() replaces this._config on
// every change, so a captured object would go stale after the first edit.
function buildKbFallbackEditor(schema, labels, getConfig, onChange) {
  const config = getConfig();
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;gap:10px;padding:8px 0';
  schema.forEach((item) => {
    const row = document.createElement('label');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;font-size:14px';
    const isBool = !!item.selector.boolean;
    const input = document.createElement(item.selector.select ? 'select' : 'input');
    if (item.selector.select) {
      item.selector.select.options.forEach((o) => {
        const opt = document.createElement('option');
        opt.value = o.value;
        opt.textContent = o.label;
        input.appendChild(opt);
      });
      input.value = config[item.name] ?? '';
    } else if (isBool) {
      input.type = 'checkbox';
      input.checked = config[item.name] === true;
    } else {
      input.type = item.selector.number ? 'number' : 'text';
      input.value = config[item.name] ?? '';
      input.style.flex = '1';
    }
    const text = document.createElement('span');
    text.textContent = (labels[item.name] || item.name) + (item.required ? ' *' : '');
    text.style.cssText = isBool ? '' : 'min-width:180px';
    if (isBool) { row.appendChild(input); row.appendChild(text); }
    else { row.appendChild(text); row.appendChild(input); }

    input.addEventListener('change', () => {
      const next = { ...getConfig() };
      if (isBool) next[item.name] = input.checked;
      else if (input.value === '') delete next[item.name];
      else next[item.name] = item.selector.number ? Number(input.value) : input.value;
      onChange(next);
    });
    wrap.appendChild(row);
  });
  return wrap;
}

customElements.define('ble-keyboard-card-editor', BleKeyboardCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-keyboard-card',
  name: 'BLE Keyboard Control',
  description: 'Full on-screen QWERTY keyboard for BLE Keyboard component',
  preview: true,
});
