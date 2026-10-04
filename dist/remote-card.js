/**
 * BLE Media Remote Card for Home Assistant
 *
 * A custom Lovelace card that provides a modern media remote control
 * for the ESPHome BLE Keyboard component. Includes power, navigation,
 * volume, media playback, and app launch buttons.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/remote-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README). Every button on
 *      this card goes through run_action, including the number pad — the
 *      simplest way to get it is `api_services: true` on the component.
 *
 * Every button fires a *named* action, so any of them can be remapped per host
 * slot via the component's `actions:` config or the web UI's Host Actions card.
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-remote-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # peer_hosts:                 # add a linked keyboard's hosts to the switcher
 *   #   - peer: bedroom
 *   #     slots: 2               # a count, or '1-3, 5' like host_slots
 *   #     names: [Bed TV, Bed PC]
 *   #     label: Bedroom
 *   #     remote_style: style3   # what that host's remote looks like
 *   #                             # (with host_slots: 0 the card drives only these)
 *   # Optional overrides:
 *   # name: Media Remote           # card title (auto from HA if omitted)
 *   # remote_style: auto           # auto | default | style1..style6 | a pasted style's id
 *   # remote_style_json: '{...}'   # style(s) copied from the web page's Export
 *   # remote_style_entity: sensor.x_remote_style   # override the auto-detected id
 *   # show_numpad: true            # show number pad (default false)
 *   # show_apps: true              # show app launch row (default true)
 *   # show_color: true             # show color buttons (default false)
 *   # hidden_entity: sensor.x_hidden_buttons   # override the auto-detected id
 *   # hold_entity: sensor.x_hold_buttons       # per-host press-and-hold list
 *   # repeat_entity: sensor.x_repeat_buttons   # per-host hold-to-repeat config
 *   # host_slots: 4                # show host switcher (needs >1; default 0 = hidden)
 *   #                             # or pick the hosts: '1-3, 5, 7-10'
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *   # active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
 *   # show_host_switcher: false    # hide the switcher, still follow the host (default true)
 *   # show_mac: true               # show the active host's MAC address (default true)
 *   # host_url: http://192.168.1.50  # ESP address (auto-detected from HA)
 *   # lcd_entity: sensor.x_lcd      # values for ["lcd",…] panels (auto-detected)
 *   # lcd_entities:                 # read these keys from HA instead
 *   #   temp: sensor.lounge_temperature
 *   # popout: on_top               # pop-out button: on_top | window | false
 *   # popout_border: true          # the card around the popped-out remote (default false;
 *   #                             # a shown host switcher brings it anyway)
 *   # popout_header: false         # no name/host switcher line in the pop-out (default true)
 *
 * Per-host hiding: if the device exposes the optional `hidden_buttons` text
 * sensor, this card hides whatever the active host hides on the web remote, and
 * follows a host switch live. Without that sensor every button is shown.
 *
 * Remote styles: the layout is drawn from the same style definitions the
 * device's own web page uses (dist/remote-styles.js, generated from the
 * firmware). `remote_style: auto` mirrors whatever style the device has for the
 * active host, which needs the optional `remote_style` text sensor — a
 * dashboard served over https cannot fetch the device's API. A host set to a
 * *custom* style names an id this card has no definition for, since those live
 * in the device's NVS. Copy that style's JSON from the web page's Export into
 * `remote_style_json` and it joins this card's style list under its own name —
 * selectable in the dropdown, and drawn under `auto` too. Use the web page's
 * "Export all" button to copy every custom style at once as a JSON array.
 * Styles only ever travel web page -> card; the card never writes back.
 * show_apps / show_color / show_numpad filter whichever style is drawn.
 *
 * LCD panels: a style may carry ["lcd",…] sections — small screens showing a
 * label and a live value. Keys beginning @ are the keyboard's own state and
 * need nothing configured; any other key names an `lcd_sources:` entry on the
 * device, whose formatted values arrive through the optional `lcd` text sensor.
 * `lcd_entities` maps a key to a Home Assistant entity instead, which wins over
 * the device's value and is how a panel shows something the ESP never sees.
 *
 * Full example with overrides:
 *   type: custom:ble-remote-card
 *   device: bluetooth_keyboard
 *   name: Living Room Remote
 *   show_numpad: true
 *   show_apps: true
 *   show_color: true
 *   host_slots: 4
 *   host_names:
 *     - TV
 *     - Phone
 *     - Laptop
 *     - Tablet
 */

// The remote's catalogue, renderer and stylesheet, generated from the firmware's
// own web page so this card draws exactly what the device does. Regenerate with
// `node tools/gen-remote-styles.mjs` after changing styles in web_page.html.
import {
  RMT_BUILTIN, RMT_BTNS, RMT_VARS, RMT_CSS, RMT_VER, RMT_LCD_LABELLED, lcdLabel,
  sectionHtml, validateTpl, themeValueBad, useIcons, knobWire, knobLevel,
} from './remote-styles.js?v=1.15.0-dev';

// Which build of this file the browser actually loaded, read from the ?v= its
// importer wrote rather than from a constant that has to be remembered at
// release. Printed because "did my update land?" is otherwise only answerable
// by fetching the served file and reading it, and it is also on the card
// element as data-version for anyone already in the inspector.
const CARD_VER = new URL(import.meta.url).searchParams.get('v') || 'unversioned';
console.info(`%c BLE Media Remote %c ${CARD_VER} `,
  'background:#0b6;color:#fff;border-radius:3px 0 0 3px', 'background:#333;color:#fff;border-radius:0 3px 3px 0');
// A card newer than its own catalogue is the half-updated install that makes a
// perfectly good style look broken — one of the two came from a cache. Only
// worth saying when both are versioned; a hand-installed card has no query
// string and would otherwise disagree with the module it imports by design.
if (CARD_VER !== 'unversioned' && RMT_VER !== 'unversioned' && RMT_VER !== CARD_VER) {
  console.warn(`ble-remote-card ${CARD_VER} is using remote-styles ${RMT_VER}. ` +
    'One of them came from a cache — reload the dashboard with the cache cleared.');
}

// Keys the device's web page acts on itself — they move that page's tab between
// linked keyboards. Nothing outside it can do that, so this card never draws them.
const PAGE_ONLY = ['next_keyboard', 'prev_host_all', 'next_host_all'];

// ── Pop-out ──────────────────────────────────────────────────────────────────
// The remote can leave the dashboard for a window of its own: one that stays
// above the others where the browser can make one (document picture-in-picture),
// an ordinary small window everywhere else. What goes there is a second card,
// created by that window's own copy of this module.
//
// Its own copy, because code keeps the clock of the window it was loaded into,
// and while the remote floats the dashboard tab is usually in the background —
// where an ordinary window's opener has its timers held to once a second.
// Measured in headless Edge with the dashboard tab hidden: a 180 ms interval
// belonging to the tab fired in bunches a second apart, one belonging to the
// window every 184 ms. So hold-to-repeat, long presses and the host poll keep
// time, and document.hidden and blur describe the window the remote is in
// rather than the tab it came from.
const POP_EVENT = 'ble-remote-popout';
// This tab's pop-outs, by keyboard. A docked card that HA builds afresh — a
// config edit, a return to its view — finds its window here and stands aside.
const POPPED = new Map();   // device -> { win, pop }
// The theme variables the card's own styles read. Copied by name as well as by
// enumerating the computed style, for a browser whose list leaves them out.
const THEME_VARS = ['--primary-color', '--accent-color', '--primary-text-color', '--secondary-text-color',
  '--secondary-background-color', '--card-background-color', '--ha-card-background', '--divider-color',
  '--error-color', '--primary-background-color'];

const popoutMode = (v) => (v === false || v === 'off' || v === 'false' || v === 'none') ? false
  : v === 'window' ? 'window' : 'on_top';
// A mouse or trackpad somewhere: a desktop, where a window of its own makes sense.
const FINE_POINTER = window.matchMedia ? window.matchMedia('(any-pointer: fine)') : null;

// A pop-out sends through this tab's connection to Home Assistant, so it cannot
// outlive the tab: a reload or a close takes the window along, rather than leave
// a remote on screen that can no longer send anything.
window.addEventListener('pagehide', () => {
  for (const { win } of POPPED.values()) {
    try { win.close(); } catch (e) { /* already gone */ }
  }
});

// What a popped-out card has in place of a dashboard. The tab's hass, which the
// card reads on its own timer — Home Assistant replaces the object on every
// change, so a new one is a change — and the one setting that decides whether
// the tab stays connected while it is out of sight.
function dashboardLink(fallback) {
  const root = () => document.querySelector('home-assistant');
  return {
    hass: () => { const ha = root(); return (ha && ha.hass) || fallback(); },
    // The event Home Assistant's own profile switch fires ("Automatically close
    // connection"). The choice is stored for this browser, as the switch's is.
    keepConnected: () => {
      const ha = root();
      if (ha) ha.dispatchEvent(new CustomEvent('hass-suspend-when-hidden',
        { detail: { suspend: false }, bubbles: true, composed: true }));
    },
  };
}

// Absolute, so an @font-face carried into the window still finds its file: a
// relative url() is relative to the sheet it came from, which the window lacks.
const absUrls = (css, base) => css.replace(/url\(\s*(['"]?)([^'")]+)\1\s*\)/g, (m, q, u) => {
  try { return `url("${new URL(u, base).href}")`; } catch (e) { return m; }
});

// Where an ordinary pop-out window last sat, per keyboard. A picture-in-picture
// window is placed by the browser, which remembers it by itself.
const POS_KEY = 'ble-remote-card:popout-pos:';
function loadPos(dev) {
  try {
    const p = JSON.parse(localStorage.getItem(POS_KEY + dev));
    return p && Number.isFinite(p.x) && Number.isFinite(p.y) ? p : null;
  } catch (e) { return null; }
}
function savePos(dev, win) {
  try { localStorage.setItem(POS_KEY + dev, JSON.stringify({ x: win.screenX, y: win.screenY })); } catch (e) { /* none */ }
}

/**
 * Parse the card's pasted-style box.
 *
 * Styles travel one way: you build one on the device's web page, where it is
 * stored, then copy its JSON here so the card knows how to draw it. The card
 * never writes back — the device stays the single source of truth, and a pasted
 * style already carries the id and name it was given there.
 *
 * Takes one style object or an array of them, so every custom remote on the
 * device can join the card's list. Returns { styles, error }; a bad paste is
 * reported rather than silently ignored.
 */
function pastedStylesOf(raw) {
  const text = (raw || '').trim();
  if (!text) return { styles: [], error: null };
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch (err) {
    return { styles: [], error: 'Style JSON is not valid: ' + err.message };
  }
  const list = Array.isArray(parsed) ? parsed : [parsed];
  const styles = [];
  for (const style of list) {
    const why = validateTpl(style);
    if (why) {
      // Every "Unknown ..." verdict — a section kind, a button, an option token,
      // a built-in value — means this card's catalogue lacks something the style
      // names. Styles are written on the device and carried here, so the usual
      // cause is not a bad style but card files older than the firmware that
      // produced it. Saying so beats leaving the style looking wrong: the card
      // and the device update separately, and only the card can tell.
      const hint = /^Unknown /.test(why)
        ? '\nThese card files may be older than the device — update the cards and try again.'
        : '';
      return { styles: [], error: `Style "${(style && style.id) || '?'}" rejected: ${why}${hint}` };
    }
    styles.push(style);
  }
  return { styles, error: null };
}

// Which hosts the switcher offers. A number is a count, as it always was — 4 is
// the first four — and a string picks them out by the numbers the switcher
// shows: "1-3, 5, 7-10" for a keyboard whose other slots are not worth a place
// on this card. What comes back is the device's own 0-based slots, in order and
// without repeats, so "1-3" is slots 0, 1 and 2.
function hostSlotList(spec) {
  const max = 10;   // MAX_HOST_SLOTS on the device
  if (typeof spec === 'number')
    return Array.from({ length: Math.max(0, Math.min(max, Math.floor(spec))) }, (_, i) => i);
  const picked = new Set();
  const take = (from, to) => {
    for (let n = Math.min(from, to); n <= Math.max(from, to); n++)
      if (n >= 1 && n <= max) picked.add(n - 1);
  };
  const parts = Array.isArray(spec) ? spec : typeof spec === 'string' ? spec.split(',') : [];
  for (const part of parts) {
    const m = String(part).trim().match(/^(\d+)\s*(?:-\s*(\d+))?$/);
    if (m) take(Number(m[1]), Number(m[2] === undefined ? m[1] : m[2]));
  }
  return [...picked].sort((a, b) => a - b);
}

class BleRemoteCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
    // Track active host changes via HA sensor entity. The firmware publishes to
    // this sensor on every switch_host() path — HA service, the device's own web
    // UI, a physical button — so every card following it stays in step with the
    // others without polling.
    // The sensor is this keyboard's active host, so it is followed only while
    // that is the host the card is on — not while a linked keyboard's is.
    // First, so the drawing below is for the host this update brought: a panel
    // naming the host used to show the old one until something else changed.
    if (this._config.host_list.length > 1 && !this._peerName()) {
      const entity = this._config.active_host_entity
        || Object.keys(hass.states).find(eid =>
             eid.startsWith('sensor.') && eid.includes(this._config.device) && eid.endsWith('_active_host')
           );
      this._hasActiveHostEntity = !!(entity && hass.states[entity]);
      if (this._hasActiveHostEntity) {
        const val = parseInt(hass.states[entity].state, 10);
        if (!isNaN(val) && val !== this._activeSlot) {
          this._activeSlot = val;
          this._syncTargetToActive();
          this._updateHostDisplay();
        }
      }
    }
    // Before _applyHidden: on 'auto' the style comes from a text sensor, so a
    // state update can change the whole layout, and the hidden list has to be
    // applied to the buttons that redraw produced. _renderStyle no-ops when
    // nothing changed, and forces the re-apply itself when something did.
    this._renderStyle();
    this._applyHidden();
    this._applyHoldAndRepeat();
    this._applyLcd();
    this._syncPop();
  }

  setConfig(config) {
    if (!config.device) {
      throw new Error('Please define a "device" (your ESPHome device name)');
    }
    // Visible in the inspector without opening the console, and it survives a
    // reconfigure because setConfig runs on every editor keystroke.
    this.setAttribute('data-version', CARD_VER);
    // As given, for the copy of this card a pop-out window builds.
    this._rawConfig = config;
    this._config = {
      device: config.device,
      // Linked keyboards whose hosts this card can drive, each entry
      // [{peer, slots, names, label, remote_style}]. Their hosts join the switcher after
      // this keyboard's own, and whichever host is selected is where everything
      // goes — as peer:<name>:<action>, through this keyboard. With
      // host_slots: 0 the card drives nothing but the keyboards listed here.
      peer_hosts: Array.isArray(config.peer_hosts) ? config.peer_hosts : [],
      name: config.name || null,
      show_numpad: config.show_numpad === true,
      show_apps: config.show_apps !== false,
      show_color: config.show_color === true,
      // Optional text sensor carrying the active host's hidden buttons, so the
      // card mirrors the web remote's per-host hiding. Absent entity = show all.
      hidden_entity: config.hidden_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_hidden_buttons`,
      // The other two per-host lists from Host Actions. Absent entity = the
      // card's own defaults: nothing holds, and volume/channel repeat.
      hold_entity: config.hold_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_hold_buttons`,
      repeat_entity: config.repeat_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_repeat_buttons`,
      host_slots: config.host_slots || 0,
      // The slots the switcher offers, 0-based, from the count or list above.
      host_list: hostSlotList(config.host_slots || 0),
      host_names: config.host_names || [],
      active_host_entity: config.active_host_entity || null,
      // Off hides the switcher and nothing else: the card goes on following
      // the active host, which host_slots: 0 would stop as well.
      show_host_switcher: config.show_host_switcher !== false,
      show_mac: config.show_mac !== false,
      host_url: config.host_url || null,
      zoom: this._parseZoom(config.zoom),
      // Which remote layout to draw. 'auto' mirrors whatever style the device
      // has for the active host; a built-in id pins one; 'custom' uses the
      // pasted JSON below.
      remote_style: config.remote_style || 'auto',
      remote_style_json: config.remote_style_json || '',
      // The style id from the device, for 'auto'. Same reason the other
      // per-host settings travel as text sensors: a dashboard on https cannot
      // fetch the device's API, so /hosts alone is not enough.
      remote_style_entity: config.remote_style_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_remote_style`,
      // The values an ["lcd",…] panel shows. Same reasoning as the lists above:
      // the device already formats them, and a text sensor is the only way they
      // reach a dashboard on https.
      lcd_entity: config.lcd_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_lcd`,
      // Per-key overrides, {key: entity_id}. These read Home Assistant directly,
      // so a panel can show something the keyboard's own node knows nothing
      // about — and they win over the device's value for the same key.
      lcd_entities: (config.lcd_entities && typeof config.lcd_entities === 'object')
        ? config.lcd_entities : {},
      // The pop-out button: 'on_top' asks for a window that stays above the
      // others and takes an ordinary one where the browser has none to give,
      // 'window' always takes an ordinary one, false leaves the button off.
      popout: popoutMode(config.popout),
      // The card around the remote in that window: its padding, background
      // and title. Off by default, as on the device page's pop-out, though a
      // shown host switcher brings it along (see _popAttrs).
      popout_border: config.popout_border === true,
      // The line with the name and host switcher, in that window. Off leaves it
      // out there whatever the dashboard's card shows — and so leaves the
      // border to popout_border alone.
      popout_header: config.popout_header !== false,
    };
    // The switcher starts on the chain's first host. With no hosts of this
    // keyboard's own that is a linked keyboard's, which is what lets a card
    // drive one without a switcher to select it.
    const first = this._hostChain()[0];
    this._target = first ? { peer: first.peer, slot: first.slot, entry: first.entry, name: first.name } : null;
    this._activeSlot = first ? first.slot : 0;
    // Forces the next _renderStyle() to redraw even if the resolved id is
    // unchanged — the pasted JSON may have been edited under the same id.
    this._drawnStyleKey = undefined;
    this._pasteRaw = undefined;
    // Per-host behaviour defaults, in place before the first hass update so a
    // press that beats the sensor read is an ordinary tap rather than a crash.
    // null repeat set = "use the card's own defaults", see _repeats().
    this._holdSet = [];
    this._repeatSet = null;
    this._repeatDelay = 400;
    this._repeatRate = 180;
    // Clearing these matters as much as the state above: they are the "sensor
    // hasn't changed" guards in _applyHoldAndRepeat/_applyHidden, so leaving
    // them set means the next hass update sees no change and never refills the
    // lists just emptied. setConfig runs again on every keystroke in the visual
    // editor's preview, which is where that would strand a card.
    this._lastHold = undefined;
    this._lastRepeat = undefined;
    this._lastHidden = undefined;
    this._lastLcd = undefined;
    // Released, not dropped — reconfiguring the card mid-press must not leave a
    // key down on the host. No-ops when nothing is held.
    this._endHold();
    // Read by the .zoom wrapper. Set on the host so it applies whether or not
    // the card has rendered yet — custom properties inherit into shadow DOM.
    this.style.setProperty('--remote-zoom', this._config.zoom);
  }

  // Anything unparseable falls back to 1 rather than collapsing the card.
  _parseZoom(value) {
    const z = parseFloat(value);
    return Number.isFinite(z) ? Math.min(Math.max(z, 0.25), 3) : 1;
  }

  // Read the active host's press-and-hold and hold-to-repeat lists, both set in
  // the web UI's Host Actions card and published as text sensors. Like
  // _applyHidden this follows a host switch with no dashboard reload.
  //
  // The handlers read these at press time rather than at bind time, so a host
  // switch mid-session changes what the next press does without rewiring.
  _applyHoldAndRepeat() {
    if (!this._hass) return;
    const read = (entity) => {
      const ent = this._hass.states[entity];
      return ent && typeof ent.state === 'string' &&
             ent.state !== 'unknown' && ent.state !== 'unavailable' ? ent.state : '';
    };

    // Both lists describe this keyboard's active host; a linked keyboard falls
    // back to the card's own defaults instead of borrowing them.
    const rawHold = this._peerName() ? '' : read(this._config.hold_entity);
    if (rawHold !== this._lastHold) {
      this._lastHold = rawHold;
      const names = rawHold ? rawHold.split(',').map(s => s.trim()).filter(Boolean) : [];
      // The same sensor carries the keys with a long-press action, as <key>@long.
      this._holdSet = names.filter(n => !n.endsWith('@long'));
      this._longSet = names.filter(n => n.endsWith('@long')).map(n => n.slice(0, -5));
      this._endHold();   // whatever is held belonged to the old set
      this._markLong();
    }

    // "<delay>,<rate>,name,name". Empty means this host was never configured,
    // which is the cue to keep the card's own defaults rather than to repeat
    // nothing — a configured-but-empty host sends just "<delay>,<rate>".
    const rawRepeat = this._peerName() ? '' : read(this._config.repeat_entity);
    if (rawRepeat !== this._lastRepeat) {
      this._lastRepeat = rawRepeat;
      if (!rawRepeat) {
        this._repeatSet = null;
        this._repeatDelay = 400;
        this._repeatRate = 180;
      } else {
        const parts = rawRepeat.split(',').map(s => s.trim());
        const delay = parseInt(parts[0], 10);
        const rate = parseInt(parts[1], 10);
        this._repeatDelay = Number.isFinite(delay) ? delay : 400;
        this._repeatRate = Number.isFinite(rate) ? rate : 180;
        this._repeatSet = parts.slice(2).filter(Boolean);
      }
    }
  }

  // True if this action should repeat while held on this host. With no sensor,
  // fall back to the catalogue's own defaults — the same `r` flags the web page
  // reads when a host has never been configured. It used to be a hardcoded four,
  // which meant an unconfigured host repeated less here than on the device's own
  // remote, and made "reset to defaults" in Host Actions change the card's
  // behaviour rather than leave it alone.
  // A tap on these waits for the release, so a press held past LONG_MS can send
  // the key's second action instead. A key in the hold list keeps holding.
  _longs(action) {
    return !!this._longSet && this._longSet.includes(action) && !PAGE_ONLY.includes(action);
  }

  // The dot on keys with a long-press action. After every redraw as well, which
  // _applyHidden follows. Not on a key in the hold list: that one holds, and its
  // long action never runs.
  _markLong() {
    if (!this.shadowRoot) return;
    const held = this._holdSet || [];
    this.shadowRoot.querySelectorAll('[data-action]').forEach(el =>
      el.classList.toggle('has-long', this._longs(el.dataset.action) && !held.includes(el.dataset.action)));
  }

  _repeats(action) {
    if (this._repeatSet) return this._repeatSet.includes(action);
    const b = RMT_BTNS[action];
    return !!(b && b.r);
  }

  // Ends the current hold, once, however the press ended. Also the safety net
  // for a card removed from the DOM or a tab hidden mid-press — a release lost
  // in transit leaves the key down on the host until max_key_hold_ms.
  _endHold() {
    if (!this._heldEl) return;
    this._heldEl.classList.remove('held');
    this._heldEl = null;
    this._runAction('release');
  }

  // Hide the buttons the active host has no use for. Driven by the text sensor,
  // so it follows a host switch without a dashboard reload.
  // `force` after a redraw: the buttons are new, but the sensor string has not
  // changed, so the guard below would otherwise skip and leave a fresh layout
  // showing what this host hides.
  _applyHidden(force) {
    if (!this._hass || !this.shadowRoot) return;
    const ent = this._hass.states[this._config.hidden_entity];
    const raw = ent && typeof ent.state === 'string' &&
                ent.state !== 'unknown' && ent.state !== 'unavailable' ? ent.state : '';
    if (!force && raw === this._lastHidden) return;   // states stream constantly; only act on change
    this._lastHidden = raw;

    // PAGE_ONLY are the web page's own keys: they move which keyboard that page
    // is driving, which means nothing on a dashboard. A style carrying them
    // draws everywhere, so they are taken out here rather than left dead.
    // The hidden list belongs to this keyboard's active host, so while the card
    // is driving a linked one nothing is hidden but the page-only keys.
    const hide = (this._peerName() ? [] : raw ? raw.split(',').map(s => s.trim()).filter(Boolean) : [])
      .concat(PAGE_ONLY);
    // visibility, not display: a hidden button keeps its slot, so removing OK
    // leaves a hole in the D-pad instead of the arrows sliding into it. An
    // invisible button takes no clicks either, which opacity would not give.
    this.shadowRoot.querySelectorAll('[data-action]').forEach(el => {
      const off = hide.includes(el.dataset.action) || el.dataset.toggledOff === '1';
      el.style.visibility = off ? 'hidden' : '';
    });
    // A knob turns by two actions, so it stops turning only when both are
    // hidden — or, one that sets a value, the key it sets. The key in its
    // middle is an ordinary key, handled above.
    this.shadowRoot.querySelectorAll('.rmt-knob, .rmt-slider').forEach(k => k.classList.toggle('off',
      k.dataset.set !== undefined ? hide.includes(k.dataset.set)
        : hide.includes(k.dataset.up) && hide.includes(k.dataset.down)));
    // A slider's thumb is its key, and is hidden for this host as that key is.
    this.shadowRoot.querySelectorAll('.rmt-slider[data-press]').forEach(k =>
      k.classList.toggle('nopress', hide.includes(k.dataset.press)));
    this._markLong();
    // Holding the shape stops once there is no shape left to hold: a group or
    // section with nothing visible collapses rather than leaving empty slots.
    // A panel is not a button, so a section holding one is never empty — every
    // key beside it can be hidden for this host and the screen still has
    // something to say. Without this the first state update after a draw
    // deletes it. Nor is a knob that still turns, one showing a level, or one
    // with a reading on a keyless middle.
    const empty = el => !el.querySelector('.rmt-lcd, .rmt-knob:not(.off), .rmt-knob[data-level], ' +
      '.rmt-knob-cap .rmt-knob-val, .rmt-slider:not(.off), .rmt-slider[data-level]') &&
      ![...el.querySelectorAll('[data-action]')]
        .some(b => b.style.visibility !== 'hidden');
    this.shadowRoot.querySelectorAll('.rmt-strip-group, .rmt-rocker-col')
      .forEach(g => { g.style.display = empty(g) ? 'none' : ''; });
    this.shadowRoot.querySelectorAll('.rmt-section')
      .forEach(s => { s.style.display = empty(s) ? 'none' : ''; });
  }

  // The unknown/unavailable dance, which four callers here need.
  _entityState(entity) {
    const ent = entity && this._hass && this._hass.states[entity];
    return ent && typeof ent.state === 'string' &&
           ent.state !== 'unknown' && ent.state !== 'unavailable' ? ent.state : '';
  }

  // Fills every screen the current style drew. Three sources, in increasing
  // precedence: the device's own lcd sensor, whose values arrive already
  // formatted — unit, decimals and all, so nothing here needs to know what kind
  // of entity is behind a key; the built-ins the card can answer by itself,
  // which is what keeps a host readout working on an https dashboard that
  // cannot reach the device at all; and the per-key entity overrides, which are
  // the point of naming a Home Assistant entity in the first place.
  _applyLcd(force) {
    if (!this._hass || !this.shadowRoot) return;
    const spans = this.shadowRoot.querySelectorAll('[data-lcd]');
    // A style may light a button or point a knob without drawing a panel at
    // all, so the early return has to consider every consumer of the map.
    if (!spans.length && !this.shadowRoot.querySelector('[data-lit], [data-level], [data-set]')) return;

    // The panel values are this keyboard's readings; a linked keyboard's own
    // are not available here, so its panels keep their dashes.
    const raw = this._peerName() ? '' : this._entityState(this._config.lcd_entity);
    const keys = Object.keys(this._config.lcd_entities);
    // The overrides' own states belong in the change key, or a panel fed
    // entirely from Home Assistant would never repaint. So does the active
    // slot, which is what the built-ins below are derived from.
    const stamp = raw + ' :: ' + this._activeSlot + ' :: ' +
      keys.map(k => k + '=' + this._entityState(this._config.lcd_entities[k])).join(';');
    if (!force && stamp === this._lastLcd) return;
    this._lastLcd = stamp;

    let vals = {};
    if (raw) {
      // A state caught mid-write is not worth a broken panel — the next update
      // is 3 s away and the dashes say plainly that nothing arrived.
      try { const o = JSON.parse(raw); if (o && typeof o === 'object') vals = o; } catch (e) { vals = {}; }
    }
    const slot = this._activeSlot || 0;
    const apiSlot = (this._hostSlots || []).find(s => s.slot === slot);
    const names = this._config.host_names;
    if (this._config.host_list.length > 0) {
      vals['@slot'] = String(slot);
      // Same order of preference the host bar uses, so the two never disagree.
      vals['@host'] = (names && names[slot]) || (apiSlot && apiSlot.name) ||
                      vals['@host'] || ('Host ' + (slot + 1));
      const addr = apiSlot && apiSlot.occupied && (apiSlot.identity || apiSlot.addr);
      if (addr) vals['@mac'] = addr;
    }
    for (const k of keys) {
      const v = this._entityState(this._config.lcd_entities[k]);
      if (v) vals[k] = v;
    }
    // Buttons that light while their source is on — same map, same moment.
    this.shadowRoot.querySelectorAll('[data-lit]').forEach(el => {
      el.classList.toggle('lit', vals[el.dataset.lit] === 'on');
    });
    spans.forEach(el => {
      let v = vals[el.dataset.lcd];
      if (v && RMT_LCD_LABELLED.includes(el.dataset.lcd)) v = lcdLabel(this._drawnStyle, v);
      el.textContent = (v === undefined || v === null || v === '') ? '--' : String(v);
    });
    // Knobs pointing at a level read the same map.
    knobLevel(this.shadowRoot, vals);
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        /* height:100% fills the slot when a row count is set, so the card
           background reaches the bottom instead of stopping short of it;
           against an auto-height section it resolves back to the content. */
        :host { display: block; height: 100%; }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          box-sizing: border-box;
          height: 100%;
          /* The Layout tab tops out at 8 rows, well under the ~13 this card
             needs with every section on, so a chosen height simply scrolls
             rather than shrinking the buttons. Auto height sizes the card to
             its content exactly, so no scrollbar appears there. */
          overflow: auto;
        }

        /* The zoom property is used rather than a transform because it affects
           layout: the card's natural height tracks the zoom, so auto height
           still fits exactly and a shorter card still scrolls. Zooming past
           ~1.1 makes the widest row (the media row) exceed a 500px section,
           which is why the card scrolls both ways. */
        .zoom {
          zoom: var(--remote-zoom, 1);
          /* Pins the layout to the card's unzoomed width, so the rows have as
             much room as at zoom 1 and the buttons aren't flex-shrunk narrower
             than they are tall — without this the 52px circles come out as
             ellipses once zoomed. Auto margins centre the result, and collapse
             to zero when it overflows, so nothing is pushed out of scroll
             range. At zoom 1 this is plain 100%. */
          width: calc(100% * var(--remote-zoom, 1));
          margin-inline: auto;
        }
        .header {
          display: flex; align-items: center; gap: 8px;
          font-size: 16px; font-weight: 600; margin-bottom: 14px;
          color: var(--primary-text-color, #333);
        }
        .header svg { width: 22px; height: 22px; fill: var(--primary-color, #03a9f4); }

        /* Host switcher */
        .header-right { margin-left: auto; display: flex; align-items: center; gap: 6px; }
        .host-btn {
          width: 24px; height: 24px; padding: 0;
          border: 1px solid var(--divider-color, #e0e0e0); border-radius: 4px;
          background: var(--secondary-background-color, #f5f5f5);
          color: var(--primary-text-color, #333);
          font-size: 12px; font-weight: 700; cursor: pointer;
          display: flex; align-items: center; justify-content: center;
          touch-action: manipulation;
        }
        .host-btn:active { background: var(--primary-color, #03a9f4); color: #fff; }
        /* Both of these are variable width — a host name can be anything, and
           the address swaps between a 17-character MAC and "Empty" — so without
           a reserved width the prev/next arrows slid sideways on every step and
           walked out from under your finger. Fixed widths keep them still; a
           long name ellipses rather than pushing them along. */
        .host-info { text-align: center; width: 84px; flex: 0 0 84px; }
        .host-name { font-size: 12px; font-weight: 600; color: var(--primary-text-color, #333); line-height: 1.2;
                     white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        /* 17ch is exactly a MAC in this monospace font. */
        .host-addr { font-size: 12px; font-weight: 400; font-family: monospace; white-space: nowrap;
                     width: 17ch; text-align: right; overflow: hidden;
                     color: var(--secondary-text-color, #888); }

        /* Button grid sections */
        .section { margin-bottom: 12px; }
        .section:last-child { margin-bottom: 0; }
        /* "safe center" centres as usual but falls back to start-alignment once
           a row is wider than the card — zoomed in, plain centring pushes the
           left half out past scrollLeft 0, where it can't be scrolled to. The
           plain rule stays first as a fallback for browsers without "safe". */
        .row { display: flex; justify-content: center; justify-content: safe center; gap: 8px; margin-bottom: 8px; }
        .row:last-child { margin-bottom: 0; }

        /* Standard round button */
        .btn {
          width: 52px; height: 52px;
          border: 1px solid var(--divider-color, #e0e0e0);
          border-radius: 50%;
          background: var(--secondary-background-color, #f5f5f5);
          color: var(--primary-text-color, #333);
          font-size: 13px; font-weight: 500;
          cursor: pointer; touch-action: manipulation;
          display: flex; align-items: center; justify-content: center;
          transition: background 0.1s, transform 0.1s;
          user-select: none; -webkit-user-select: none;
        }
        .btn:active, .btn.p {
          background: var(--primary-color, #03a9f4);
          color: #fff; transform: scale(0.93);
        }
        /* Currently held down on the host (press and hold). Stays lit for the
           whole press, unlike .p which is a 150ms tap flash. */
        .btn.held {
          background: var(--primary-color, #03a9f4);
          color: #fff;
        }
        .btn svg { width: 22px; height: 22px; fill: currentColor; pointer-events: none; }

        /* Power button */
        .btn.power { background: #c62828; color: #fff; border-color: #c62828; }
        .btn.power:active, .btn.power.p { background: #e53935; }

        /* Record button */
        .btn.rec { background: #c62828; color: #fff; border-color: #c62828; }
        .btn.rec:active, .btn.rec.p { background: #e53935; }

        /* Color buttons */
        .btn.red { background: #e53935; color: #fff; border: none; width: 44px; height: 44px; }
        .btn.green { background: #43a047; color: #fff; border: none; width: 44px; height: 44px; }
        .btn.yellow { background: #fdd835; color: #333; border: none; width: 44px; height: 44px; }
        .btn.blue { background: #1e88e5; color: #fff; border: none; width: 44px; height: 44px; }

        /* D-pad */
        .dpad { display: grid; grid-template-columns: 52px 52px 52px; grid-template-rows: 52px 52px 52px; gap: 4px; justify-content: center; justify-content: safe center; margin: 8px 0; }
        .dpad .btn { border-radius: 12px; }
        .dpad .center { background: var(--primary-color, #03a9f4); color: #fff; border-color: var(--primary-color, #03a9f4); font-size: 11px; font-weight: 700; border-radius: 50%; }
        .dpad .center:active { background: var(--accent-color, #ff9800); }
        .dpad .empty { visibility: hidden; }

        /* Wide buttons */
        .btn.wide { width: auto; border-radius: 26px; padding: 0 18px; font-size: 12px; }

        /* Volume/channel strip */
        .strip { display: flex; align-items: center; justify-content: center; justify-content: safe center; gap: 16px; }
        .strip-group { display: flex; flex-direction: column; align-items: center; gap: 4px; }
        .strip-label { font-size: 10px; color: var(--secondary-text-color, #888); font-weight: 600; text-transform: uppercase; }

        /* Media controls */
        .media-row { display: flex; justify-content: center; justify-content: safe center; gap: 10px; }
        .btn.media { width: 46px; height: 46px; }

        /* Number pad */
        .numpad { display: grid; grid-template-columns: repeat(3, 52px); gap: 6px; justify-content: center; justify-content: safe center; }

        /* App row */
        .app-row { display: flex; justify-content: center; justify-content: safe center; gap: 8px; flex-wrap: wrap; }
        .btn.app { width: auto; border-radius: 26px; padding: 0 14px; height: 38px; font-size: 11px; }

        /* Divider */
        .divider { height: 1px; background: var(--divider-color, #e0e0e0); margin: 12px 0; }

        /* The buttons are a fixed size, so past ~430px the card would just add
           whitespace either side of them. Cap the content and centre it instead,
           the way the built-in thermostat card constrains its dial. Sections cap
           at 500px, so this only bites in panel view or a wide masonry column. */
        .header, .section, .divider {
          max-width: 460px;
          margin-left: auto;
          margin-right: auto;
        }

        /* The remote's own stylesheet, generated from the firmware. Its rules
           fall back to the page palette (--bg, --fg, …), and a shadow root has
           no :root to inherit those from — so they are mapped onto HA's theme
           here. That is what makes an unthemed style follow the dashboard
           theme, exactly as the full remote does on the device's page. */
        .card {
          --bg: var(--secondary-background-color, #f0f2f5);
          --fg: var(--primary-text-color, #212121);
          --card: var(--ha-card-background, var(--card-background-color, #fff));
          --border: var(--divider-color, #d0d5dd);
          --muted: var(--secondary-text-color, #7c8aad);
          --active: var(--primary-color, #03a9f4);
          --accent: var(--accent-color, #00d4aa);
        }
        ${RMT_CSS}

        /* Shown instead of the remote when a pasted style can't be used, so a
           typo reads as a message rather than an empty card. */
        .style-error {
          margin: 12px auto;
          max-width: 460px;
          padding: 10px 12px;
          border-radius: 8px;
          background: var(--error-color, #b71c1c);
          color: #fff;
          font-size: 13px;
          line-height: 1.4;
          /* A rejection may carry a second line explaining that the cards, not
             the style, are out of date. The verdict it follows ends in a list
             whose last entry is a bare "-", so running the two together reads
             as nonsense — pre-line keeps the newline that separates them. */
          white-space: pre-line;
        }

        /* Pop-out. In a window of its own the card has no corners or shadow:
           there is no dashboard for them to sit on. Nor a pop-out button — said
           here as well as by the script, so the dashboard's card, measuring
           itself as the window will draw it, leaves the button out too. */
        :host([popped]) .card { border-radius: 0; box-shadow: none; }
        :host([popped]) .pop-btn { display: none !important; }
        /* Without its border (see _popAttrs) the window holds the remote and
           nothing else, as the device page's pop-out does: no card behind it,
           no padding, no header. The window takes the card's colour instead
           (see _dressWindow), so a remote that draws no body of its own looks
           as it does on the dashboard, and one that does is left as its own
           silhouette. With the border, popout_header can still leave out the
           line holding the name and host switcher. */
        :host([popped][bare]) .card { padding: 0; background: none; }
        :host([popped][bare]) .header, :host([popped][noheader]) .header { display: none; }
        /* The device page's answer to a window that rounds its size: a few pixels
           of the remote's own background past its edges, as a shadow, so it costs
           no layout and nothing measuring the remote sees it. */
        :host([popped][bare]) .rmt-body {
          box-shadow: var(--rb-shadow, 0 0 #0000), 0 4px 0 var(--rb-bg, transparent), 0 0 0 3px var(--rb-bg, transparent);
        }
        :host([popped][bare]) .link-note { margin: 8px; min-width: calc(100% - 16px); }
        .header .pop-btn {
          width: 26px; height: 26px; padding: 0; flex: none;
          border: none; border-radius: 4px; background: none; cursor: pointer;
          color: var(--secondary-text-color, #888);
          display: flex; align-items: center; justify-content: center;
        }
        .header .pop-btn:hover { color: var(--primary-color, #03a9f4); background: var(--secondary-background-color, #f5f5f5); }
        .header .pop-btn svg { width: 18px; height: 18px; fill: currentColor; }
        .pop-msg, .out-note, .link-note {
          max-width: 460px; margin: 0 auto 12px;
          font-size: 13px; line-height: 1.4; color: var(--primary-text-color, #333);
        }
        .out-row { display: flex; align-items: center; flex-wrap: wrap; gap: 6px 10px; margin-bottom: 8px; }
        .out-row:last-child { margin-bottom: 0; }
        .out-row span { flex: 1 1 200px; }
        .out-btn {
          flex: none; padding: 5px 12px; border-radius: 14px; cursor: pointer; font: inherit;
          border: 1px solid var(--divider-color, #e0e0e0);
          background: var(--secondary-background-color, #f5f5f5);
          color: var(--primary-text-color, #333);
        }
        .out-btn:active { background: var(--primary-color, #03a9f4); color: #fff; }
        /* No width of its own (width 0, stretched by min-width), so the window
           measuring the remote does not widen to set this out on one line. */
        .link-note {
          width: 0; min-width: 100%; box-sizing: border-box;
          padding: 8px 10px; border-radius: 8px; background: var(--secondary-background-color, #f5f5f5);
        }
        /* Out: the remote is in its window, and this card says where it went.
           Down: the window's tab lost Home Assistant, so the keys send nothing. */
        .card:not(.out) .out-note, .card:not(.down) .link-note { display: none; }
        .card.out #rmt-body, .card.out #host-switcher { display: none !important; }
        .card.down #rmt-body { opacity: 0.4; }
        .pop-btn[hidden], .pop-msg[hidden], .out-row[hidden], .out-btn[hidden] { display: none !important; }
      </style>
      <div class="card">
        <div class="zoom">
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M18 7V4c0-1.1-.9-2-2-2H8c-1.1 0-2 .9-2 2v3H2v15h20V7h-4zM8 4h8v3H8V4zm10 16H6V9h12v11zm-6-7c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2z"/></svg>
          <span class="header-name">${this._config.name || 'Media Remote'}</span>
          <button class="pop-btn" id="pop-btn" title="Pop out into a window of its own" aria-label="Pop out" hidden>
            <svg viewBox="0 0 24 24"><path d="M19 19H5V5h7V3H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 6.41V10h2V3h-7z"/></svg>
          </button>
          <div class="header-right" id="host-switcher" style="display:none">
            <span class="host-addr"></span>
            <button class="host-btn" id="host-prev">&#9664;</button>
            <div class="host-info"><div class="host-name"></div></div>
            <button class="host-btn" id="host-next">&#9654;</button>
          </div>
        </div>
        <div class="pop-msg" id="pop-msg" hidden></div>
        <div class="out-note">
          <div class="out-row">
            <span>In a window of its own.</span>
            <button class="out-btn" id="pin-back">Pin back</button>
          </div>
          <div class="out-row" id="keep-row" hidden>
            <span>Home Assistant disconnects a dashboard left in the background for 5 minutes, and the remote stops with it.</span>
            <button class="out-btn" id="keep-conn">Keep connected</button>
          </div>
        </div>
        <div class="link-note">
          <div class="out-row">
            <span>Not connected to Home Assistant, so nothing is sent. Bring the dashboard's tab forward to reconnect.</span>
            <button class="out-btn" id="link-keep" hidden>Keep connected from now on</button>
          </div>
        </div>

        <!-- Drawn by _renderStyle() from the selected remote style, using the
             same renderer the device's own web page uses. -->
        <div id="rmt-body" class="rmt-body"></div>
        </div>
      </div>
    `;

    // One device-registry lookup serves two purposes: the friendly name (when no
    // title was configured) and the ESP's own URL, which HA fills in as
    // configuration_url because web_control requires the web_server component.
    const wantsName = !this._config.name;
    const wantsUrl = this._config.host_list.length > 1 && !this._config.host_url;
    if ((wantsName || wantsUrl) && this._hass) {
      const nameSpan = shadow.querySelector('.header-name');
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

    // Wire up all buttons
    this._wireButtons(shadow);
    // Draw the remote. Safe before the first hass update: 'auto' falls back to
    // the default until the sensor arrives, so the card is never blank.
    this._renderStyle();
    this._setupHostSwitcher(shadow);
    this._wirePopout(shadow);
  }

  // ── Pop-out ──────────────────────────────────────────────────────
  // The docked card opens the window and stands aside while it is open; the
  // card inside the window is this same class, from that window's own copy of
  // the module, marked by _popLink. See POPPED at the top of the file.

  _wirePopout(shadow) {
    const btn = shadow.getElementById('pop-btn');
    // On the press for a mouse, as the device page does: the click that ends it
    // then arrives after the window exists, and a click is what an on-top
    // window needs before it will be resized. Anything else waits for the click.
    let pressed = false;
    btn.addEventListener('pointerdown', (e) => {
      pressed = e.pointerType === 'mouse';
      if (pressed) this._popOut();
    });
    btn.addEventListener('click', () => {
      if (!pressed) this._popOut();
      pressed = false;
    });
    shadow.getElementById('pin-back').addEventListener('click', () => this._pinBack());
    shadow.getElementById('keep-conn').addEventListener('click', () => this._keepConnected());
    shadow.getElementById('link-keep').addEventListener('click', () => this._keepConnected());
    if (this._popLink) this._armFit();
    this._syncPop();
  }

  // Desktop browsers only: the companion apps open no windows, and on a phone a
  // pop-up is just another tab. Not from the card editor's preview either, nor
  // from the card that is already the popped-out one.
  _popAllowed() {
    if (!this._config.popout || this._popLink || this.preview) return false;
    const w = window;
    if (w.externalAppV2 || w.externalApp ||
        (w.webkit && w.webkit.messageHandlers && w.webkit.messageHandlers.externalBus)) return false;
    return !!(FINE_POINTER && FINE_POINTER.matches);
  }

  // How the popped-out remote is dressed, as the attributes its card carries in
  // the window. Bare is the card without its border — the default, as on the
  // device page's pop-out. The name and host switcher sit on that card, so while
  // the switcher shows there the border comes with them. popout_header leaves
  // that line out of the window (noheader), and popout_border brings the border
  // back on its own.
  _popAttrs() {
    const c = this._config;
    const switcher = c.popout_header && c.show_host_switcher && this._hostChain().length >= 2;
    return { bare: !c.popout_border && !switcher, noheader: !c.popout_header };
  }

  _markPop(on) {
    const look = this._popAttrs();
    this.toggleAttribute('popped', on);
    this.toggleAttribute('bare', on && look.bare);
    this.toggleAttribute('noheader', on && look.noheader);
    return look;
  }

  // The tab's connection to Home Assistant. A dashboard left in the background
  // is disconnected after five minutes unless the profile says otherwise — and
  // the background is exactly where a floating remote's dashboard is.
  _linkUp() {
    const h = this._hass;
    return !!h && h.connected !== false && !(h.connection && h.connection.connected === false);
  }

  // Brings the card's face in line with where the remote is: docked, out in a
  // window, or out with its tab disconnected. Runs on every hass update, so it
  // only ever toggles what is already there.
  _syncPop() {
    const sr = this.shadowRoot, c = this._config;
    if (!sr || !c) return;
    const card = sr.querySelector('.card');
    if (!card) return;
    // Only what changed is written: this runs on every state change in HA.
    const show = (id, on) => { const el = sr.getElementById(id); if (el.hidden === on) el.hidden = !on; };
    const suspends = !!this._hass && this._hass.suspendWhenHidden !== false;
    if (this._popLink) {
      const down = !this._linkUp();
      card.classList.toggle('down', down);
      show('link-keep', down && suspends);
      return;
    }
    let open = POPPED.get(c.device);
    // A window closed without its pagehide reaching us is gone all the same.
    if (open && open.win.closed) { POPPED.delete(c.device); open = null; }
    card.classList.toggle('out', !!open);
    show('pop-btn', !open && this._popAllowed());
    show('keep-row', suspends);
  }

  _popMessage(text) {
    const el = this.shadowRoot && this.shadowRoot.getElementById('pop-msg');
    if (!el) return;
    el.textContent = text;
    el.hidden = false;
    clearTimeout(this._popMsgT);
    this._popMsgT = setTimeout(() => { el.hidden = true; }, 10000);
  }

  // The size the remote wants in a window of its own, rather than the size it
  // was given: the card laid out at its own width for a moment, the way the
  // device page measures its pop-out — no wider than a sections-view column,
  // though, so a style with a long row wraps it as it would on the dashboard
  // rather than asking for a window across the screen. A scrollbar the card has
  // right now is added on, or the window would hand the remote that much less
  // and wrap a row — which is what raised the scrollbar in the first place.
  _wantSize() {
    const card = this.shadowRoot && this.shadowRoot.querySelector('.card');
    if (!card) return null;
    // Only in the window: a docked card scrolls because the dashboard gave it a
    // fixed height, which says nothing about the window it is about to get.
    const bar = this._popLink ? Math.max(0, Math.min(40, card.offsetWidth - card.clientWidth)) : 0;
    // Measured as the window will draw it, even from the dashboard: the window
    // is asked for as it opens, and a size taken off the docked card — padding,
    // title and all — would show as a margin until the first click resized it.
    // Put back before anything can paint.
    const docked = !this.hasAttribute('popped');
    const keep = card.getAttribute('style');
    let w = 0, h = 0;
    try {
      const { bare } = docked ? this._markPop(true) : this._popAttrs();
      // Height only. The card's own overflow stays, as it is what keeps the
      // margin of a first or last row inside it: without the border nothing
      // else does, and the window came out that margin short with a scrollbar.
      card.style.height = 'auto';
      card.style.width = 'max-content';
      // Rounded up: a window a pixel short of the remote's width wraps its
      // widest row, and the spare pixel is the card's colour, which the window
      // has too. The cap is a sections column's 468px of content, plus the
      // border's 32.
      w = Math.min(Math.ceil(card.getBoundingClientRect().width),
                   Math.ceil(468 * (this._config.zoom || 1)) + (bare ? 0 : 32));
      card.style.width = `${w}px`;
      h = card.getBoundingClientRect().height;
    } finally {
      // Whatever happened, the dashboard's card goes back to looking like one.
      if (keep === null) card.removeAttribute('style'); else card.setAttribute('style', keep);
      if (docked) this._markPop(false);
    }
    if (!w || !h) return null;
    const scr = window.screen || {};
    // The 2px on the height is the device page's, for a window that rounds.
    return {
      w: Math.min(w + bar, (scr.availWidth || 1600) - 40),
      h: Math.min(Math.ceil(h) + 2, (scr.availHeight || 900) - 40),
    };
  }

  async _popOut() {
    if (this._popping || !this._popAllowed()) return;
    const dev = this._config.device;
    const open = POPPED.get(dev);
    if (open && !open.win.closed) {
      try { open.win.focus(); } catch (e) { /* the browser decides */ }
      return;
    }
    this._popping = true;
    try {
      await this._openPopout(dev);
    } finally {
      this._popping = false;
    }
  }

  async _openPopout(dev) {
    const sr = this.shadowRoot;
    const size = this._wantSize() || { w: 380, h: 640 };
    let win = null, pip = false, why = '';
    // Asked for before anything else happens, while the press still counts as
    // one: the browser opens either kind of window only in answer to a gesture.
    if (this._config.popout === 'on_top' && window.documentPictureInPicture) {
      try {
        win = await documentPictureInPicture.requestWindow({ width: size.w, height: size.h });
        pip = true;
      } catch (e) {
        why = (e && (e.message || e.name)) || 'refused';
      }
    }
    if (!win) {
      let feat = `popup=yes,width=${size.w},height=${size.h}`;
      const at = loadPos(dev);
      if (at) feat += `,left=${at.x},top=${at.y}`;
      // A name of its own each time: window.open finds a window by name, and one
      // this browser already has under it — another tab's pop-out — would be
      // handed back and wiped.
      win = window.open('', `ble-remote-${dev}-${Date.now()}`, feat);
    }
    if (!win) {
      this._popMessage(why
        ? `The browser would not keep a window on top (${why}), and blocked an ordinary one. Allow pop-ups for this site and press again.`
        : 'The browser blocked the window. Allow pop-ups for this site and press again.');
      return;
    }
    const d = win.document;
    if (!pip) {
      // Written out so the window is in standards mode: one opened on nothing
      // starts in quirks mode, which lays the card out differently.
      try {
        d.open();
        d.write('<!doctype html><html><head><meta charset="utf-8"></head><body></body></html>');
        d.close();
      } catch (e) { /* quirks mode, then — the card still draws */ }
    }
    d.title = sr.querySelector('.header-name').textContent || 'Media Remote';
    this._dressWindow(d);

    // The card's own module, loaded again by the window — so the card there
    // runs on that window's clock. See POPPED at the top of the file.
    const s = d.createElement('script');
    s.type = 'module';
    s.src = import.meta.url;
    d.head.appendChild(s);
    const ready = await Promise.race([
      win.customElements.whenDefined('ble-remote-card').then(() => true),
      new Promise(r => setTimeout(() => r(false), 8000)),
    ]);
    if (!ready || win.closed) {
      try { win.close(); } catch (e) { /* gone */ }
      if (!ready) this._popMessage('The remote could not be loaded into its window.');
      return;
    }
    const pop = d.createElement('ble-remote-card');
    pop._popLink = dashboardLink(() => this._hass);
    // A copy belonging to the window, so nothing in it is an object from here.
    pop.setConfig(win.JSON.parse(JSON.stringify(this._rawConfig)));
    d.body.appendChild(pop);
    pop.hass = pop._popLink.hass();

    POPPED.set(dev, { win, pop });
    win.addEventListener('pagehide', () => {
      if (!pip) savePos(dev, win);
      const cur = POPPED.get(dev);
      if (cur && cur.win === win) {
        POPPED.delete(dev);
        window.dispatchEvent(new CustomEvent(POP_EVENT));
      }
    });
    // The click that ends this press, if it opened on the press: a gesture the
    // window can be resized in, should it want a size it was not opened at.
    window.addEventListener('click', () => {
      try { pop._retryFit(); } catch (e) { /* closed already */ }
    }, { capture: true, once: true });
    window.dispatchEvent(new CustomEvent(POP_EVENT));
  }

  // The window has none of the dashboard around it, so what the card inherits
  // from there comes along: the theme's variables, its fonts and its colour
  // scheme. Read off this card rather than the page, so a theme set for one view
  // is the one the remote keeps.
  _dressWindow(d) {
    const cs = getComputedStyle(this);
    const root = d.documentElement.style;
    const names = new Set(THEME_VARS);
    for (let i = 0; i < cs.length; i++) if (cs[i].startsWith('--')) names.add(cs[i]);
    for (const n of names) {
      const v = cs.getPropertyValue(n);
      if (v !== '') root.setProperty(n, v);
    }
    root.colorScheme = cs.colorScheme;   // dark scrollbars with a dark theme
    root.height = '100%';
    // The card's own background fills the window, so a window a pixel bigger
    // than the remote shows the card rather than a strip of something else.
    let bg = getComputedStyle(this.shadowRoot.querySelector('.card')).backgroundColor;
    if (!bg || bg === 'transparent' || bg === 'rgba(0, 0, 0, 0)')
      bg = cs.getPropertyValue('--primary-background-color').trim() || '#fff';
    d.body.style.cssText = `margin:0;height:100%;overflow:hidden;background:${bg};` +
      `color:${cs.color};font-family:${cs.fontFamily}`;
    const faces = [];
    for (const sheet of [...document.styleSheets, ...(document.adoptedStyleSheets || [])]) {
      let rules;
      try { rules = sheet.cssRules; } catch (e) { continue; }   // another origin's
      for (const rule of rules)
        if (rule instanceof CSSFontFaceRule) faces.push(absUrls(rule.cssText, sheet.href || document.baseURI));
    }
    if (faces.length) {
      const st = d.createElement('style');
      st.textContent = faces.join('\n');
      d.head.appendChild(st);
    }
  }

  _pinBack() {
    const dev = this._config.device, open = POPPED.get(dev);
    POPPED.delete(dev);
    if (open) { try { open.win.close(); } catch (e) { /* gone */ } }
    window.dispatchEvent(new CustomEvent(POP_EVENT));
  }

  _keepConnected() {
    (this._popLink || dashboardLink(() => this._hass)).keepConnected();
  }

  // In the window: the dashboard's hass, read on this window's clock. A new
  // object is a change; the socket's state can change without one, so the face
  // is brought up to date either way.
  _pullHass() {
    let h = null;
    try { h = this._popLink.hass(); } catch (e) { /* its tab is going, and takes this window along */ }
    if (h && h !== this._hass) this.hass = h;
    else this._syncPop();
  }

  // A window of its own is sized to the remote, and follows it when a host's
  // style is a different size. Only the remote's own size counts: a window
  // resized by hand is left the size it was given.
  _armFit() {
    const body = this.shadowRoot.getElementById('rmt-body');
    if (!body || this._fitRO) return;
    let last = null, t = 0;
    const check = () => {
      clearTimeout(t);
      t = setTimeout(() => {
        const s = this._wantSize();
        if (!s || (last && Math.abs(last.w - s.w) <= 3 && Math.abs(last.h - s.h) <= 3)) return;
        last = s;
        this._fitWindow(s);
      }, 250);
    };
    this._fitRO = new ResizeObserver(check);
    this._fitRO.observe(body);
    // An on-top window refuses a resize that no click asked for, so one it
    // refused waits for the next click in it — or on the dashboard, see
    // _openPopout.
    window.addEventListener('click', () => this._retryFit(), true);
  }

  _fitWindow(s) {
    s = s || this._fitWant || this._wantSize();
    if (!s) return;
    const dw = s.w - window.innerWidth, dh = s.h - window.innerHeight;
    if (Math.abs(dw) <= 3 && Math.abs(dh) <= 3) { this._fitWant = null; return; }
    this._fitWant = s;
    try {
      window.resizeBy(dw, dh);
    } catch (e) {
      return;   // refused for want of a gesture: the next click tries again
    }
    // Asked once, then taken as answered. A window that came back some other
    // size — held under the screen's height, say — has said what it will, and
    // the device page measured where asking again leads: a window that twitches
    // on every click for the rest of the session.
    this._fitWant = null;
  }

  // A click is good for a few seconds, and a host switch's new style arrives
  // about a second after the click that asked for it — so try then, and over
  // the next few seconds. Each try does nothing unless a size is waiting.
  _retryFit() {
    [0, 500, 1000, 2000, 3000].forEach(ms => setTimeout(() => { if (this._fitWant) this._fitWindow(); }, ms));
  }

  // ── Host switcher ────────────────────────────────────────────────
  // Mirrors the keyboard card: prev/next arrows around the active host's name,
  // with its MAC to the left. A switch made here reaches the other cards two
  // ways — instantly if the active-host sensor exists, otherwise on their next
  // poll below — and _applyHidden() repaints this card's buttons for the new host.

  _setupHostSwitcher(shadow) {
    // Two hosts to move between is the bar's reason to exist — they can be one
    // here and one on a linked keyboard.
    if (this._hostChain().length < 2) return;
    this._hostSlots = [];
    // Hidden by choice, the card still polls: /hosts is where an 'auto' style
    // finds the host's style without the sensor, and where a panel gets the
    // host's address. Only the arrows and the name go.
    if (!this._config.show_host_switcher) {
      this._startHostPolling();
      return;
    }

    shadow.getElementById('host-switcher').style.display = '';
    this._hostInfoEl = shadow.querySelector('.host-info');
    this._hostNameEl = shadow.querySelector('.host-name');
    this._hostAddrEl = shadow.querySelector('.host-addr');

    shadow.getElementById('host-prev').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      this._stepHost(-1);
    });
    shadow.getElementById('host-next').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      this._stepHost(1);
    });
    // Tapping the name moves a whole keyboard along, for when there are more
    // hosts than anyone wants to step through.
    if (this._config.peer_hosts.length) {
      this._hostNameEl.style.cursor = 'pointer';
      this._hostNameEl.title = 'Tap to switch keyboard';
      this._hostNameEl.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this._stepHost(1, true);
      });
    }

    this._updateHostDisplay();
    this._startHostPolling();
  }

  // The switcher's hosts, in order: this keyboard's, then each linked
  // keyboard's. One entry per host, so stepping is just walking this list. Each
  // carries the name configured for it — names line up with the hosts shown, so
  // three names belong to the three hosts chosen rather than to slots 1 to 3.
  _hostChain() {
    const chain = [];
    const names = this._config.host_names || [];
    this._config.host_list.forEach((slot, i) => chain.push({ peer: null, slot, entry: null, name: names[i] }));
    for (const k of (this._config.peer_hosts || [])) {
      hostSlotList(k.slots || 0).forEach((slot, i) =>
        chain.push({ peer: k.peer || null, slot, entry: k, name: (k.names || [])[i] }));
    }
    return chain;
  }

  // The name configured for one of this keyboard's slots, if the switcher shows
  // it at all. Looked up by position for the same reason as above.
  _configNameFor(slot) {
    const i = this._config.host_list.indexOf(slot);
    return i < 0 ? undefined : (this._config.host_names || [])[i];
  }

  // Which keyboard the buttons drive: the one the selected host belongs to.
  _peerName() {
    return (this._target && this._target.peer) || null;
  }

  // This keyboard's own host can change without the card: another card, the web
  // page, a physical button. The switcher has to step from where the keyboard
  // really is, so its position follows the active slot — otherwise the first
  // press after an outside switch only asks for the host it is already on, and
  // reads as a press that did nothing. A slot this card does not list leaves the
  // position alone: stepping carries on from the last host it showed.
  _syncTargetToActive() {
    if (this._peerName()) return;
    const c = this._hostChain().find(x => !x.peer && x.slot === this._activeSlot);
    if (c) this._target = { peer: null, slot: c.slot, entry: null, name: c.name };
  }

  _chainIndex(chain) {
    const t = this._target || { peer: null, slot: this._activeSlot || 0 };
    const i = chain.findIndex(c => c.peer === t.peer && c.slot === t.slot);
    return i < 0 ? 0 : i;
  }

  // One step along the chain, or — from a tap on the keyboard's name — to the
  // first host of the next keyboard along.
  _stepHost(delta, wholeKeyboard) {
    const chain = this._hostChain();
    if (chain.length < 2) return;
    // Before the switcher moves, not after: it would show a host the keyboard
    // was never asked for.
    if (this._popLink && !this._linkUp()) { this._syncPop(); return; }
    let i = this._chainIndex(chain);
    if (wholeKeyboard) {
      const from = chain[i].peer;
      do { i = (i + 1) % chain.length; } while (chain[i].peer === from && chain[i].slot !== 0);
    } else {
      i = (i + delta + chain.length) % chain.length;
    }
    this._goToHost(chain[i]);
  }

  _goToHost(c) {
    this._target = { peer: c.peer, slot: c.slot, entry: c.entry, name: c.name };
    this._activeSlot = c.slot;
    this._switchHost(c.slot);
    // A linked keyboard's hosts can each want their own style, and nothing on
    // this side can look it up: the entry says which, else the card's own.
    this._renderStyle();
    this._applyHidden(true);
    this._applyHoldAndRepeat();   // those lists are this keyboard's active host's
  }

  // Every name the switcher can show, so the field can be sized to the longest
  // of them rather than to a guess. A linked keyboard's hosts carry its name as
  // well as their own, which no 84px field was ever going to hold.
  _hostLabels() {
    return this._hostChain().map(c => {
      if (c.peer) {
        const label = (c.entry && (c.entry.label || c.entry.peer)) || c.peer;
        return `${label}: ${c.name || 'Host ' + (c.slot + 1)}`;
      }
      const api = this._hostSlots.find(s => s.slot === c.slot);
      return c.name || (api && api.name) || 'Host ' + (c.slot + 1);
    });
  }

  // Widen the name field to fit the longest of them, measured rather than
  // estimated: the element is the one that will draw it, so its own scrollWidth
  // is the answer whatever the theme's font. Sized once for the whole set, not
  // per host — the arrows must not move as the name changes, or stepping walks
  // the button out from under your finger. Capped so a silly name cannot eat
  // the header.
  _fitHostInfo() {
    if (!this._hostNameEl || !this._hostInfoEl) return;
    const labels = this._hostLabels();
    const key = labels.join('\u0001');
    if (key === this._hostFitKey) return;
    const keep = this._hostNameEl.textContent;
    let w = 0;
    for (const t of labels) {
      this._hostNameEl.textContent = t;
      w = Math.max(w, this._hostNameEl.scrollWidth);
    }
    this._hostNameEl.textContent = keep;
    if (!w) return;   // not laid out yet; measured again on the next repaint
    this._hostFitKey = key;
    const px = Math.min(Math.max(w + 4, 84), 200);
    this._hostInfoEl.style.width = `${px}px`;
    this._hostInfoEl.style.flexBasis = `${px}px`;
  }

  _switchHost(slot) {
    if (!this._hass) return;
    this._activeSlot = slot;
    // A linked keyboard switches its own host through the same action route as
    // its buttons; the service belongs to the keyboard this card points at.
    if (this._peerName()) {
      this._runAction(`switch_host:${slot}`);
    } else {
      const slug = this._config.device.replace(/-/g, '_');
      this._hass.callService('esphome', `${slug}_switch_host`, { slot });
    }
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
    if (this._initialized && this._config && this._config.host_list.length > 1) {
      this._startHostPolling();
    }
    // A hidden tab or a switched-away dashboard never delivers pointerup, and a
    // key left down on the host is worse than a press cut short.
    this._onHide = () => { if (document.hidden) this._endHold(); };
    document.addEventListener('visibilitychange', this._onHide);
    window.addEventListener('blur', this._boundEndHold = () => this._endHold());
    if (this._popLink) {
      // In a window of its own. Closing it mid-press is a press cut short too,
      // and a closing document never disconnects its elements.
      this._markPop(true);
      window.addEventListener('pagehide', this._boundEndHold);
      clearInterval(this._hassPoll);
      this._hassPoll = setInterval(() => this._pullHass(), 300);
    } else {
      // Docked: told when a pop-out of this keyboard opens or closes.
      this._onPopEvent = () => this._syncPop();
      window.addEventListener(POP_EVENT, this._onPopEvent);
      this._syncPop();
    }
  }

  disconnectedCallback() {
    clearInterval(this._hostPollInterval);
    this._hostPollInterval = null;
    document.removeEventListener('visibilitychange', this._onHide);
    window.removeEventListener('blur', this._boundEndHold);
    window.removeEventListener('pagehide', this._boundEndHold);
    if (this._onPopEvent) window.removeEventListener(POP_EVENT, this._onPopEvent);
    clearInterval(this._hassPoll);
    this._hassPoll = null;
    this._endHold();   // the card is going away mid-press
  }

  // Resolves the ESP's base URL: an explicit host_url wins, otherwise the
  // configuration_url HA recorded for the device.
  _hostBaseUrl() {
    const url = this._config.host_url || this._deviceUrl;
    return url ? url.replace(/\/+$/, '').replace(/\/ble_keyboard$/, '') : '';
  }

  _pollHosts() {
    // /hosts answers for the keyboard this card points at; while a linked
    // keyboard's host is the one selected, its names come from peer_hosts and
    // this keyboard's answer would only fight the selection.
    if (!this._hass || this._config.host_list.length < 2 || this._peerName()) return;
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
          this._syncTargetToActive();
        }
        this._hostSlots = data.slots || [];
        // Absent on firmware older than this card — _styleFromHosts falls back.
        this._hostsStyleSlot = data.style_slot;
        this._hostDataAvailable = true;
        this._updateHostDisplay();
        // /hosts carries the per-host style id. It is the fallback route for
        // 'auto' — the sensor is the one that survives an https dashboard.
        this._renderStyle();
      })
      .catch(() => this._updateHostDisplay());
  }

  _updateHostDisplay() {
    if (!this._hostNameEl) return;
    this._fitHostInfo();
    const t = this._target;
    if (t && t.peer) {
      const label = (t.entry && (t.entry.label || t.entry.peer)) || t.peer;
      this._hostNameEl.textContent = `${label}: ${t.name || 'Host ' + (t.slot + 1)}`;
      // /hosts is this keyboard's; it says nothing about that one's addresses.
      this._hostAddrEl.style.display = 'none';
      return;
    }
    const cfgName = this._configNameFor(this._activeSlot);
    const apiSlot = this._hostSlots.find(s => s.slot === this._activeSlot);
    this._hostNameEl.textContent = cfgName
      || (apiSlot && apiSlot.name) || ('Host ' + (this._activeSlot + 1));
    // Without device data there's nothing truthful to show, so the element
    // collapses rather than leaving a blank gap beside the arrows.
    const showAddr = this._config.show_mac && this._hostDataAvailable;
    this._hostAddrEl.style.display = showAddr ? '' : 'none';
    if (showAddr) {
      // identity is the address the host keeps across reconnects and is what the
      // host MAC sensor publishes; addr is what it happened to connect with and
      // rotates on Android. Prefer identity, fall back when it can't be resolved.
      // A slot marked as never advertising has no host and never will, so
      // neither an address nor 'Empty' is the truth about it -- 'Empty' reads as
      // a slot waiting to be paired.
      const noBle = apiSlot && apiSlot.broadcast === false;
      const addr = apiSlot && apiSlot.occupied && (apiSlot.identity || apiSlot.addr);
      this._hostAddrEl.textContent = noBle ? 'No BLE' : (addr || 'Empty');
    }
  }

  _press(btn) {
    btn.classList.add('p');
    setTimeout(() => btn.classList.remove('p'), 150);
  }

  // One delegated handler for the whole remote, instead of binding each button.
  // The layout is redrawn whenever the style changes, so per-button listeners
  // would have to be rebound every time; resolving the action from the element
  // under the pointer means a redraw needs no rewiring at all. This is how the
  // device's own page does it.
  _wireButtons(shadow) {
    const card = shadow.querySelector('.card');
    if (!card) return;

    let interval = null, timer = null, activeEl = null;
    // A key with a long action, between its press and the decision: where it
    // went down and when, and whether the long action has already gone.
    let longEl = null, longAt = 0, longFired = false, sx = 0, sy = 0;
    const LONG_MS = 500;
    const longPress = (el, action) => {
      el.classList.add('long');
      setTimeout(() => el.classList.remove('long'), 300);
      this._runAction(action + '@long');
    };
    const stopRepeat = () => {
      if (interval) { clearInterval(interval); interval = null; }
      if (timer) { clearTimeout(timer); timer = null; }
    };
    const end = () => {
      stopRepeat();
      if (activeEl && this._heldEl === activeEl) this._endHold();
      activeEl = null;
    };

    card.addEventListener('pointerdown', (e) => {
      const el = e.target.closest && e.target.closest('[data-action]');
      if (!el) return;
      const action = el.dataset.action;
      e.preventDefault();
      activeEl = el;
      this._press(el);

      // Hold wins, and goes down immediately — a push-to-talk button that only
      // engages after a threshold clips the first word.
      if (this._holdSet.includes(action)) {
        this._endHold();          // only one hold at a time
        this._heldEl = el;
        el.classList.add('held');
        this._runAction('hold:' + action);
        return;
      }

      // Nothing yet for a key with a long action: the release, or LONG_MS
      // passing, decides which of its two it sends.
      if (this._longs(action)) {
        stopRepeat();
        longEl = el; longAt = Date.now(); longFired = false; sx = e.clientX; sy = e.clientY;
        timer = setTimeout(() => {
          timer = null;
          if (longEl !== el) return;
          longFired = true;
          longPress(el, action);
        }, LONG_MS);
        return;
      }

      this._runAction(action);
      if (!this._repeats(action)) return;
      // The first repeat waits out the configured delay, so a quick tap stays
      // one press. Matches the web remote's timing for this host.
      timer = setTimeout(() => {
        timer = null;
        interval = setInterval(() => this._runAction(action), this._repeatRate);
      }, this._repeatDelay);
    });
    // Released before the long action went: that was a tap.
    card.addEventListener('pointerup', () => {
      if (longEl && !longFired) {
        const action = longEl.dataset.action;
        if (Date.now() - longAt >= LONG_MS) longPress(longEl, action);
        else this._runAction(action);
      }
      longEl = null;
      end();
    });
    // Leaving, a cancelled gesture or a drag past 10 px sends nothing.
    const abandon = () => { longEl = null; end(); };
    card.addEventListener('pointerleave', abandon);
    card.addEventListener('pointercancel', abandon);
    card.addEventListener('pointermove', (e) => {
      if (longEl && Math.abs(e.clientX - sx) + Math.abs(e.clientY - sy) > 10) abandon();
    });
    // A phone's own long-press menu would take the gesture away from the key.
    card.addEventListener('contextmenu', (e) => {
      if (e.target.closest && e.target.closest('[data-action]')) e.preventDefault();
    });
    // A knob's ring: turned with the device page's own code. A run of steps goes
    // as one repeat:, which _runAction passes on to a linked keyboard whole.
    knobWire(card, (action, n) => this._runAction(n > 1 ? `repeat:${n}:${action}` : action));
  }

  // ── Remote style ────────────────────────────────────────────────
  // Resolve, then draw. Order for 'auto': the text sensor (the only route that
  // survives an https dashboard), then the tpl on the /hosts response the card
  // already polls, then the default.
  _resolveStyle() {
    const cfg = this._config;
    // Report a bad paste rather than silently drawing the default — a typo in
    // the box should say so.
    const paste = pastedStylesOf(cfg.remote_style_json || '');
    if (paste.error) return { style: null, error: paste.error };

    let id = cfg.remote_style;
    // A host on a linked keyboard draws the style its entry names, and 'auto'
    // has nothing to follow there: the style sensor is this keyboard's.
    const onPeer = this._peerName();
    if (onPeer) {
      const named = (this._target && this._target.entry && this._target.entry.remote_style) ||
                    (id !== 'auto' ? id : null);
      if (!named) {
        return { style: RMT_BUILTIN.find(t => t.id === 'default'),
                 error: `This card drives ${onPeer}, so it cannot follow that keyboard's style. ` +
                        'Name the style it uses — remote_style on the card, or on its peer_hosts entry.' };
      }
      id = named;
    }
    if (id === 'auto') {
      const ent = this._hass && this._hass.states[cfg.remote_style_entity];
      const live = !!(ent && typeof ent.state === 'string' &&
        ent.state !== 'unknown' && ent.state !== 'unavailable');
      // A live sensor is the device's own answer and is taken as it comes —
      // empty means the full remote. /hosts is asked only when there is no
      // sensor: it names the style of the host keys are going to, which while a
      // macro is only visiting another host is not the one being shown.
      id = (live ? ent.state.trim() : this._styleFromHosts()) || 'default';
    }
    // A pasted style counts under 'auto' too: once it is saved to the device it
    // can be assigned to a host, and this is the only copy of its definition the
    // card has — the device stores the document but the card never reads it back
    // (an https dashboard cannot). Without this, switching to that host would
    // fall back to the full remote even though the style is saved and assigned.
    // A pasted style is looked up exactly like a built-in — that is what makes
    // it "join the list" rather than being a separate mode.
    const style = this._pastedById(id) || RMT_BUILTIN.find(t => t.id === id);
    // A custom style the device knows but this card has never been given has no
    // definition here, so fall back rather than draw nothing.
    return { style: style || RMT_BUILTIN.find(t => t.id === 'default'), error: null };
  }

  // Styles pasted into the card, which is how a style made on the web page
  // reaches it: the device holds the definition, and this is the copy the card
  // draws from. Nothing is ever sent the other way.
  //
  // Accepts one style or an array of them, so several custom remotes can join
  // the list. Cached against the raw text — this runs on every state update.
  _pastedStyles() {
    const raw = (this._config.remote_style_json || '').trim();
    if (raw !== this._pasteRaw) {
      this._pasteRaw = raw;
      this._pasteStyles = pastedStylesOf(raw).styles;
    }
    return this._pasteStyles || [];
  }

  _pastedById(id) {
    return this._pastedStyles().find(t => t.id === id) || null;
  }

  // The per-host style id from the /hosts poll, when that fetch works at all.
  _styleFromHosts() {
    const slots = this._hostSlots;
    if (!Array.isArray(slots)) return '';
    // style_slot is the slot the device says the remote is drawn for, which
    // trails the active one while a macro visits another host. Only trusted
    // when _activeSlot came from that same poll — mixed with the active-host
    // sensor, which moves at once, a 30s-old number would hold every real
    // switch back until the next poll.
    const want = !this._hasActiveHostEntity && typeof this._hostsStyleSlot === 'number'
      ? this._hostsStyleSlot : this._activeSlot;
    const slot = slots.find(s => s.slot === want);
    return (slot && slot.tpl) || '';
  }

  _renderStyle() {
    if (!this.shadowRoot) return;
    const body = this.shadowRoot.getElementById('rmt-body');
    if (!body) return;
    const { style, error } = this._resolveStyle();

    // Redraw only when the outcome actually changes. The key covers the pasted
    // JSON too, so editing a custom style under the same id still redraws.
    const key = error || (style.id + '|' + JSON.stringify(style.sections).length +
                          '|' + (this._config.remote_style === 'custom' ? this._config.remote_style_json : ''));
    if (key === this._drawnStyleKey) return;
    this._drawnStyleKey = key;

    // Never redraw under a finger: a held key would be stranded down on the
    // host, and a running repeat would hammer a button that no longer exists.
    this._endHold();

    if (error) {
      body.innerHTML = '';
      const msg = document.createElement('div');
      msg.className = 'style-error';
      msg.textContent = error;
      body.appendChild(msg);
      return;
    }

    for (const k in RMT_VARS) body.style.removeProperty(RMT_VARS[k]);
    if (style.theme) {
      // Checked here as well as in validateTpl above, because most styles the
      // card draws never went through it: they arrive from the device's stored
      // templates or from the card's own configuration, and a theme value is
      // CSS that can fetch. Same function the device's page uses, so the two
      // cannot drift apart.
      for (const k in RMT_VARS) {
        if (typeof style.theme[k] === 'string' && !themeValueBad(k, style.theme[k]))
          body.style.setProperty(RMT_VARS[k], style.theme[k]);
      }
    }
    // A style's imported icons travel inside it: the web page's Export attaches
    // them, and this card has no other way to reach them on an https dashboard.
    // Replaced on every draw, so one style's logo never lands on another's key.
    useIcons(style.icons || {});
    body.innerHTML = style.sections.map(sectionHtml).join('');
    // Kept for _applyLcd: @last and @station arrive as an action, and this
    // is the only thing that knows the style calls it something.
    this._drawnStyle = style;
    this._applyToggles();
    // Forced: the buttons are new, so whatever this host hides has to be
    // reapplied even though the hidden list itself did not change.
    this._applyHidden(true);
    // Same reason: any panel this style drew is showing its placeholder dashes,
    // and the values are already in hand from the last state update.
    this._applyLcd(true);
  }

  // show_apps / show_color / show_numpad filter whatever style is drawn, rather
  // than only the default layout. The card editor switches them on for the
  // sections a style actually contains, so a style still renders as designed
  // unless you deliberately turn one off.
  _applyToggles() {
    if (!this.shadowRoot) return;
    const body = this.shadowRoot.getElementById('rmt-body');
    if (!body) return;
    const c = this._config;
    body.querySelectorAll('[data-action]').forEach(el => {
      const a = el.dataset.action;
      const off = (!c.show_color && a.startsWith('color_')) ||
                  (!c.show_numpad && /^num[0-9]$/.test(a));
      el.dataset.toggledOff = off ? '1' : '';
    });
    if (!c.show_apps) {
      body.querySelectorAll('.rmt-app-row').forEach(row => {
        row.querySelectorAll('[data-action]').forEach(el => { el.dataset.toggledOff = '1'; });
      });
    }
  }

  // Returns the call, so a knob can wait for one run of steps before the next.
  _runAction(action) {
    if (!this._hass) return;
    // A popped-out remote whose tab has lost Home Assistant sends nothing: the
    // call could only fail, and say so in a toast on a tab out of sight.
    if (this._popLink && !this._linkUp()) { this._syncPop(); return; }
    const peer = this._peerName();
    return this._hass.callService('esphome', `${this._config.device}_run_action`,
      { action: peer ? `peer:${peer}:${action}` : action });
  }

  // Natural pixel height of the card. Prefer measuring the rendered DOM —
  // the estimate below can't see a header or app row that wrapped to two
  // lines, or sections a host's hidden_buttons sensor collapsed.
  // Fallback constants match a headless render (all-on = 891px) plus ~30px
  // of font/wrap slack, giving the same row counts as live measurement.
  _naturalHeightPx() {
    const el = this.shadowRoot && this.shadowRoot.querySelector('.card');
    if (el && el.scrollHeight > 0) return el.scrollHeight;
    const c = this._config || {};
    let px = 540;
    if (c.show_color === true) px += 69;
    if (c.show_numpad === true) px += 251;
    if (c.show_apps !== false) px += 63;
    return px;
  }

  getCardSize() {
    return Math.ceil(this._naturalHeightPx() / 50);
  }

  // Sections-view sizing. 'auto' sizes the section to the content, which also
  // keeps it right when a host's hidden_buttons sensor collapses a section or
  // the app row wraps in a narrow card.
  //
  // The bounds are left wide open — 1-12 columns, and 1-8 rows. 8 is as far as
  // HA's height slider goes: the editor never sets the size picker's `rows`
  // property, so its row window stays at the default 8, and a bound outside
  // that puts the slider handle off its own track and stops it responding.
  //
  // Every height the slider can reach is shorter than this card needs (~13 rows
  // with every section on), so picking one scrolls the card. Use the `zoom`
  // option to make the whole remote fit instead — zoom 0.55 brings it down to
  // about 8 rows.
  //
  // max_rows is left off deliberately: the slider stops at 8 either way (rowMax
  // falls back to the picker's `rows`), but with no bound declared
  // computeCardGridSize() applies no upper clamp, so a taller card can still be
  // set as `grid_options: {rows: N}` in the YAML editor.
  getGridOptions() {
    return {
      columns: 12,
      min_columns: 1,
      rows: 'auto',
      min_rows: 1,
    };
  }

  // Enables the dashboard's visual editor instead of "Visual editor is not
  // supported". YAML editing keeps working exactly as before.
  static getConfigElement() {
    return document.createElement('ble-remote-card-editor');
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-remote-card', BleRemoteCard);

/* ── Visual editor ───────────────────────────────────────────────────────
 * Prefers Home Assistant's own <ha-form> so the panel matches built-in cards.
 * If ha-form isn't registered in the frontend, falls back to plain inputs —
 * an unstyled editor is still better than a blank panel.
 * ---------------------------------------------------------------------- */

// Built per render, not once: a pasted style joins this list by name, and the
// list is where it becomes selectable.
function remoteEditorSchema(config) {
  const pasted = pastedStylesOf((config && config.remote_style_json) || '').styles;
  return [
    { name: 'device', required: true, selector: { text: {} } },
    { name: 'name', selector: { text: {} } },
    { name: 'zoom', selector: { number: { min: 0.25, max: 3, step: 0.05, mode: 'box' } } },
    { name: 'remote_style', selector: { select: { mode: 'dropdown', options: [
      { value: 'auto', label: 'Auto (follow the device)' },
      ...RMT_BUILTIN.map(t => ({ value: t.id, label: t.name })),
      // Pasted styles sit alongside the built-ins under the names they were
      // given on the web page.
      ...pasted.map(t => ({ value: t.id, label: `${t.name} (pasted)` })),
    ] } } },
    // multiline so a style — about a kilobyte — is readable and pasteable. The
    // fallback editor renders a textarea for this too.
    { name: 'remote_style_json', selector: { text: { multiline: true } } },
    { name: 'remote_style_entity', selector: { entity: { domain: 'sensor' } } },
    { name: 'show_numpad', selector: { boolean: {} } },
    { name: 'show_apps', selector: { boolean: {} } },
    { name: 'show_color', selector: { boolean: {} } },
    { name: 'hidden_entity', selector: { entity: { domain: 'sensor' } } },
    { name: 'hold_entity', selector: { entity: { domain: 'sensor' } } },
    { name: 'repeat_entity', selector: { entity: { domain: 'sensor' } } },
    { name: 'lcd_entity', selector: { entity: { domain: 'sensor' } } },
    // An object, so the fallback editor below — which has no object selector —
    // shows it as a text box. That is survivable: it is the one field most
    // cards never set.
    { name: 'lcd_entities', selector: { object: {} } },
    { name: 'host_slots', selector: { text: {} } },
    { name: 'host_names', selector: { text: {} } },
    { name: 'peer_hosts', selector: { text: { multiline: true } } },
    { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
    { name: 'show_host_switcher', selector: { boolean: {} } },
    { name: 'show_mac', selector: { boolean: {} } },
    { name: 'host_url', selector: { text: {} } },
    { name: 'popout', selector: { select: { mode: 'dropdown', options: [
      { value: 'on_top', label: 'On top of other windows, where the browser can' },
      { value: 'window', label: 'An ordinary window' },
      { value: 'off', label: 'No pop-out button' },
    ] } } },
    { name: 'popout_border', selector: { boolean: {} } },
    { name: 'popout_header', selector: { boolean: {} } },
  ];
}

// peer_hosts is a YAML list of mappings, and an editor field is a box of text:
// one linked keyboard per line, its fields in the order the YAML keys come in.
//   bedroom | 2 | Bed TV, Bed PC | Bedroom | style3
// Trailing fields can be left off, and the host count fills itself in from the
// names when it is missing.
const peerHostsToText = (list) => (Array.isArray(list) ? list : []).map((k) =>
  [k.peer || '', k.slots || 0, (k.names || []).join(', '), k.label || '', k.remote_style || '']
    .join(' | ').replace(/(\s*\|)+$/, '')).join('\n');

const peerHostsFromText = (text) => String(text).split('\n').map((line) => {
  const f = line.split('|').map((x) => x.trim());
  if (!f[0]) return null;
  const names = (f[2] || '').split(',').map((n) => n.trim()).filter(Boolean);
  const slots = parseInt(f[1], 10);
  const entry = { peer: f[0], slots: slots > 0 ? slots : (names.length || 1) };
  if (names.length) entry.names = names;
  if (f[3]) entry.label = f[3];
  if (f[4]) entry.remote_style = f[4];
  return entry;
}).filter(Boolean);

const REMOTE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  zoom: 'Zoom (1 = normal, 0.5 = half, 2 = double)',
  remote_style: 'Remote style',
  remote_style_json: 'Paste from the web page’s Export, or Export all for every custom style',
  remote_style_entity: 'Remote-style sensor (optional)',
  show_numpad: 'Show number pad',
  show_apps: 'Show app launcher row',
  show_color: 'Show colour buttons',
  hidden_entity: 'Hidden-buttons sensor (optional)',
  hold_entity: 'Press-and-hold sensor (optional)',
  repeat_entity: 'Hold-to-repeat sensor (optional)',
  lcd_entity: 'LCD values sensor (optional)',
  lcd_entities: 'LCD keys read from Home Assistant instead, e.g. temp: sensor.lounge',
  host_slots: 'Host switcher: how many hosts, or which — 1-3,5,7-10 (0 = hide)',
  host_names: 'Host names, comma-separated (optional)',
  peer_hosts: 'Linked keyboards, one per line: bedroom | 2 | Bed TV, Bed PC | Bedroom | style3',
  active_host_entity: 'Active-host sensor (optional)',
  show_host_switcher: 'Show host switcher',
  show_mac: 'Show host MAC address',
  host_url: 'Device URL (optional, auto-detected)',
  popout: 'Pop-out button (desktop browsers)',
  popout_border: 'Pop-out border (the card and its name around the remote; shown anyway with the host switcher)',
  popout_header: 'Pop-out name and host switcher line',
};

class BleRemoteCardEditor extends HTMLElement {
  setConfig(config) {
    // Seed the toggles with their real defaults so the editor doesn't show
    // show_apps as off just because the key is absent. Spreading config last
    // preserves `type` and anything else the editor doesn't manage.
    this._config = {
      show_numpad: false,
      show_apps: true,
      show_color: false,
      host_slots: 0,
      show_host_switcher: true,
      show_mac: true,
      zoom: 1,
      remote_style: 'auto',
      popout: 'on_top',
      popout_border: false,
      popout_header: true,
      ...config,
    };
    // false in YAML is the dropdown's 'off', which _emit() turns back.
    this._config.popout = popoutMode(this._config.popout) || 'off';
    // host_names is a YAML list but edits as one comma-separated field; show it
    // as text here and turn it back into a list in _emit().
    if (Array.isArray(this._config.host_names)) {
      this._config.host_names = this._config.host_names.join(', ');
    }
    // host_slots edits as text, so it can hold a list as well as a count.
    if (typeof this._config.host_slots === 'number')
      this._config.host_slots = String(this._config.host_slots);
    // Same for peer_hosts, a list of mappings that edits as one line each.
    if (Array.isArray(this._config.peer_hosts)) {
      this._config.peer_hosts = peerHostsToText(this._config.peer_hosts);
    }
    this._render();
  }

  set hass(hass) {
    this._hass = hass;
    if (this._form) this._form.hass = hass;
  }

  _emit(config) {
    // The show_* toggles filter whatever style is drawn, and two of them
    // default to off — so choosing a style that has a number pad or colour keys
    // would strip out the very thing that makes it that style. Switching them on
    // for the sections the chosen style actually contains means it renders as
    // designed, while the toggles still do exactly what they say afterwards.
    if (config.remote_style && config.remote_style !== this._config.remote_style) {
      const picked = RMT_BUILTIN.find(t => t.id === config.remote_style)
        || pastedStylesOf(config.remote_style_json || '').styles.find(t => t.id === config.remote_style);
      if (picked) {
        const flat = JSON.stringify(picked.sections);
        if (/"num[0-9]"/.test(flat)) config.show_numpad = true;
        if (/"color_/.test(flat)) config.show_color = true;
        if (/"apps"/.test(flat)) config.show_apps = true;
      }
    }
    this._config = config;
    // Hand the card a real list again — it expects host_names to be an array.
    const out = { ...config };
    // A plain number still means a count of hosts; anything else is the list of
    // hosts to show, and stays a string.
    if (typeof out.host_slots === 'string') {
      const t = out.host_slots.trim();
      out.host_slots = /^\d+$/.test(t) ? Number(t) : t;
      if (out.host_slots === 0 || out.host_slots === '') delete out.host_slots;
    }
    if (typeof out.host_names === 'string') {
      const names = out.host_names.split(',').map((n) => n.trim()).filter(Boolean);
      if (names.length) out.host_names = names;
      else delete out.host_names;
    }
    // peer_hosts edits as lines of text; hand the card its list back.
    if (typeof out.peer_hosts === 'string') {
      const list = peerHostsFromText(out.peer_hosts);
      if (list.length) out.peer_hosts = list;
      else delete out.peer_hosts;
    }
    if (out.popout === 'off') out.popout = false;
    this.dispatchEvent(new CustomEvent('config-changed', {
      detail: { config: out },
      bubbles: true,
      composed: true,
    }));
  }

  _render() {
    if (this._rendered) {
      if (this._form) {
        // Schema too, not just data: pasting a style adds an entry to the
        // dropdown, and a cached schema would never show it.
        this._form.schema = remoteEditorSchema(this._config);
        this._form.data = this._config;
      }
      return;
    }
    this._rendered = true;

    if (customElements.get('ha-form')) {
      const form = document.createElement('ha-form');
      form.hass = this._hass;
      form.data = this._config;
      form.schema = remoteEditorSchema(this._config);
      form.computeLabel = (s) => REMOTE_EDITOR_LABELS[s.name] || s.name;
      form.addEventListener('value-changed', (e) => this._emit(e.detail.value));
      this.appendChild(form);
      this._form = form;
      return;
    }

    this.appendChild(buildFallbackEditor(
      remoteEditorSchema(this._config), REMOTE_EDITOR_LABELS, () => this._config,
      (cfg) => this._emit(cfg),
    ));
  }
}

// Shared by the fallback path: renders one row per schema entry.
// `getConfig` is a function, not an object: _emit() replaces this._config on
// every change, so a captured object would go stale after the first edit.
function buildFallbackEditor(schema, labels, getConfig, onChange) {
  const config = getConfig();
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;gap:10px;padding:8px 0';
  schema.forEach((item) => {
    // Nothing here can edit a map, and a text input would render one as
    // "[object Object]" and then save that back over it. Leaving the row out
    // costs a field this editor never had; drawing it would lose the value.
    if (item.selector.object) return;
    const row = document.createElement('label');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;font-size:14px';
    const isBool = !!item.selector.boolean;
    // A style's JSON runs to about a kilobyte, which is unusable in a one-line
    // input — so a multiline text selector gets a real textarea here too, not
    // just in the ha-form path.
    const isArea = !!(item.selector.text && item.selector.text.multiline);
    const input = document.createElement(
      item.selector.select ? 'select' : isArea ? 'textarea' : 'input');
    if (item.selector.select) {
      item.selector.select.options.forEach((o) => {
        const opt = document.createElement('option');
        opt.value = typeof o === 'string' ? o : o.value;
        opt.textContent = typeof o === 'string' ? o : o.label;
        input.appendChild(opt);
      });
      input.value = config[item.name] ?? '';
    } else if (isBool) {
      input.type = 'checkbox';
      input.checked = config[item.name] === true;
    } else if (isArea) {
      input.rows = 5;
      input.value = config[item.name] ?? '';
      input.style.cssText = 'flex:1;font-family:monospace;font-size:12px';
    } else {
      input.type = item.selector.number ? 'number' : 'text';
      if (item.selector.number) {
        if (item.selector.number.step) input.step = item.selector.number.step;
      }
      input.value = config[item.name] ?? '';
      input.style.flex = '1';
    }
    const text = document.createElement('span');
    text.textContent = (labels[item.name] || item.name) + (item.required ? ' *' : '');
    text.style.cssText = isBool ? '' : 'min-width:180px';
    if (isArea) row.style.cssText = 'display:flex;flex-direction:column;gap:4px;font-size:14px';
    if (isBool) { row.appendChild(input); row.appendChild(text); }
    else { row.appendChild(text); row.appendChild(input); }

    input.addEventListener('change', () => {
      const next = { ...getConfig() };
      if (isBool) next[item.name] = input.checked;
      else if (input.value === '') delete next[item.name];       // keep the YAML clean
      else next[item.name] = item.selector.number ? Number(input.value) : input.value;
      onChange(next);
    });
    wrap.appendChild(row);
  });
  return wrap;
}

customElements.define('ble-remote-card-editor', BleRemoteCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-remote-card',
  name: 'BLE Media Remote',
  description: 'Media remote control for ESPHome BLE Keyboard',
});
