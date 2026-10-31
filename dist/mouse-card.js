/**
 * BLE Mouse Control Card for Home Assistant
 *
 * A custom Lovelace card that provides a touchpad, 3 mouse buttons,
 * and scroll controls for the ESPHome BLE Keyboard component.
 * Tap a button to click; long-press (400ms) to hold it for dragging
 * (needs the mouse_hold / mouse_release services), tap again to release.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/mouse-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README).
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-mouse-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # peer_hosts:                 # add a linked keyboard's hosts to the switcher
 *   #   - peer: bedroom
 *   #     slots: 2               # a count, or '1-3, 5' like host_slots
 *   #     names: [Bed TV, Bed PC]
 *   #     label: Bedroom
 *   #                             # (with host_slots: 0 the card drives only these)
 *   # Optional overrides:
 *   # name: Mouse Control          # card title (default "Mouse Control")
 *   # sensitivity: 1.5             # movement multiplier (default 1.5)
 *   # mouse_acceleration: 0.15     # speed-based acceleration factor (default 0.15)
 *   # mouse_max_speed: 4.5         # max sensitivity cap (default 4.5)
 *   # scroll_sensitivity: 2        # scroll multiplier (default 2)
 *   # tap_to_click: true           # tap touchpad = left click (default true)
 *   # host_slots: 4                # show host switcher (needs >1; default 0 = hidden)
 *   #                             # or pick the hosts: '1-3, 5, 7-10'
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *   # active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
 *   # show_mac: true               # show the active host's MAC address (default true)
 *   # show_forget_host: true       # Forget button beside the switcher (default false)
 *   # host_url: http://192.168.1.50  # ESP address (auto-detected from HA)
 *
 * Full example with overrides:
 *   type: custom:ble-mouse-card
 *   device: bluetooth_keyboard
 *   name: Living Room Mouse
 *   sensitivity: 2.0
 *   mouse_acceleration: 0.2
 *   mouse_max_speed: 6.0
 *   scroll_sensitivity: 3
 *   tap_to_click: false
 *   host_slots: 4
 *   host_names:
 *     - TV
 *     - Phone
 *     - Laptop
 *     - Tablet
 */

// Which build of this file the browser actually loaded, read from the ?v= its
// importer wrote rather than from a constant that has to be remembered at
// release. Printed because "did my update land?" is otherwise only answerable
// by fetching the served file and reading it; also on the card element as
// data-version for anyone already in the inspector.
const CARD_VER = new URL(import.meta.url).searchParams.get('v') || 'unversioned';
console.info(`%c BLE Mouse %c ${CARD_VER} `,
  'background:#0b6;color:#fff;border-radius:3px 0 0 3px', 'background:#333;color:#fff;border-radius:0 3px 3px 0');

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

class BleMouseCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
    // Track active host changes via HA sensor entity. The firmware publishes to
    // this sensor on every switch_host() path — HA service, the device's own web
    // UI, a physical button — so every card following it stays in step with the
    // others without polling.
    if (this._config && this._config.host_list.length > 1 && !this._peerName()) {
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
    // The registry of actions can arrive after the card does.
    this._syncForget();
  }

  setConfig(config) {
    if (!config.device) {
      throw new Error('Please define a "device" (your ESPHome device name)');
    }
    // Visible in the inspector without opening the console.
    this.setAttribute('data-version', CARD_VER);
    this._config = {
      device: config.device,
      name: config.name || null,
      sensitivity: config.sensitivity || 1.5,
      mouse_acceleration: config.mouse_acceleration || 0.15,
      mouse_max_speed: config.mouse_max_speed || 4.5,
      scroll_sensitivity: config.scroll_sensitivity || 2,
      tap_to_click: config.tap_to_click !== false,
      host_slots: config.host_slots || 0,
      // The slots the switcher offers, 0-based, from the count or list above.
      host_list: hostSlotList(config.host_slots || 0),
      host_names: config.host_names || [],
      // Linked keyboards whose hosts this card can drive, each entry
      // [{peer, slots, names, label}]. Their hosts join the switcher after
      // this keyboard's own, and whichever host is selected is where everything
      // goes — as peer:<name>:<action>, through this keyboard. With
      // host_slots: 0 the card drives nothing but the keyboards listed here.
      peer_hosts: Array.isArray(config.peer_hosts) ? config.peer_hosts : [],
      active_host_entity: config.active_host_entity || null,
      show_mac: config.show_mac !== false,
      // Dropping a host's pairing is not something to do by accident, so the
      // button is off until it is asked for, and takes two taps once it is there.
      show_forget_host: config.show_forget_host === true,
      host_url: config.host_url || null,
      zoom: this._parseZoom(config.zoom),
    };
    // The switcher starts on the chain's first host. With no hosts of this
    // keyboard's own that is a linked keyboard's, which is what lets a card
    // drive one without a switcher to select it.
    const first = this._hostChain()[0];
    this._target = first ? { peer: first.peer, slot: first.slot, entry: first.entry, name: first.name } : null;
    this._activeSlot = first ? first.slot : 0;
    // Read by the .zoom wrapper. Set on the host so it applies whether or not
    // the card has rendered yet — custom properties inherit into shadow DOM.
    this.style.setProperty('--mouse-zoom', this._config.zoom);
  }

  // Anything unparseable falls back to 1 rather than collapsing the card.
  _parseZoom(value) {
    const z = parseFloat(value);
    return Number.isFinite(z) ? Math.min(Math.max(z, 0.25), 3) : 1;
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        :host {
          display: block;
          height: 100%;
        }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          color: var(--primary-text-color);
          user-select: none;
          -webkit-user-select: none;
          box-sizing: border-box;
          height: 100%;
          display: flex;
          flex-direction: column;
          /* The touchpad stretches to fill a taller card, but stops at its
             minimum height — squeeze the card below that, or zoom in past what
             the width can hold, and this scrolls rather than letting anything
             spill over the card's edge. */
          overflow: auto;
        }
        /* zoom rather than a transform, because it affects layout: the card's
           natural height tracks the zoom, so auto height still fits exactly.
           Stretching as a flex item is what keeps the touchpad filling a taller
           card at any zoom. */
        .zoom {
          zoom: var(--mouse-zoom, 1);
          display: flex;
          flex-direction: column;
          flex: 0 0 auto;
          /* Inside a zoomed element, 100% is the card's width divided by the
             zoom, so a width:100% touchpad would come out the same size on
             screen at every zoom — only the font would change. Multiplying back
             by the zoom pins the layout to the card's unzoomed width, so every
             length scales by exactly the zoom factor in both directions. It has
             to be an exact width rather than a min-width: a floor does nothing
             below zoom 1, which left small zooms full-width with shrunken text.
             Auto
             margins centre the result, and collapse to zero when it overflows,
             so nothing is pushed out of scroll range. */
          width: calc(100% * var(--mouse-zoom, 1));
          /* Floor the block at the width its contents actually need, so every
             part of the card shares one width and the whole thing scrolls
             together. Without it the header and the button grids (which cannot
             shrink) set the scroll range while the touchpad tracked the narrower
             visible width, so it stopped short of the scrolled area. This is a
             content floor, not zoom scaling: the width above still does the zoom
             pinning, and min-content is measured inside the zoomed box so it
             scales too. Shrinking the card to fit a small slot is what the zoom
             option is for. */
          min-width: min-content;
          margin-inline: auto;
        }
        .header {
          font-size: 16px;
          font-weight: 500;
          margin-bottom: 12px;
          display: flex;
          align-items: center;
          gap: 8px;
          flex: 0 0 auto;
        }
        /* Drawn inside the remote card, under the remote its touchpad key
           belongs to: the pad, buttons and scroll keys alone, on that card's
           background, with no card, header or host switcher of their own. */
        :host([embedded]) {
          height: auto;
        }
        :host([embedded]) .card {
          background: none;
          box-shadow: none;
          border-radius: 0;
          padding: 0;
          height: auto;
          overflow: visible;
        }
        :host([embedded]) .header {
          display: none;
        }
        .header svg {
          width: 20px;
          height: 20px;
          fill: var(--primary-text-color);
          opacity: 0.7;
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
        /* Fixed at the width of its armed label, as on the device page, so the
           swap to Confirm? cannot slide the arrows out from under a finger. */
        .host-forget {
          width: 62px;
          margin-left: 6px;
          font-size: 11px;
          font-weight: 600;
          color: #c44;
        }
        .host-forget.armed,
        .host-forget:not(.off):active {
          background: #c44;
          border-color: #c44;
          color: #fff;
        }
        /* The device publishes no forget_host action: the button says so in its
           tooltip rather than arming, so it must not look or press as if it
           could. */
        .host-forget.off,
        .host-forget.off:active {
          opacity: .45;
          background: var(--secondary-background-color, #f0f0f0);
          border-color: var(--divider-color, #e0e0e0);
          color: var(--secondary-text-color, #888);
          cursor: default;
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
          color: var(--secondary-text-color, #999);
          font-family: monospace;
          white-space: nowrap;
        /* 17ch is exactly a MAC in this monospace font, so the field keeps its
           width whether it shows an address or "Empty". */
        width: 17ch;
        text-align: right;
        overflow: hidden;
        }
        .touchpad {
          width: 100%;
          /* Without this the 2px border sits outside the 100%, making the pad
             4px wider than the card and tripping a horizontal scrollbar. */
          box-sizing: border-box;
          /* aspect-ratio is what fixes the pad's height. It deliberately does
             not stretch to fill a taller card: doing so changed its height
             without changing its width, which threw the shape off — at 8 rows
             and zoom 2 the pad came out 5.35:1 instead of 16:9. */
          aspect-ratio: 16/9;
          flex: 0 0 auto;
          min-height: 60px;
          background: var(--secondary-background-color, #f0f0f0);
          border-radius: 12px;
          border: 2px solid var(--divider-color, #e0e0e0);
          cursor: crosshair;
          touch-action: none;
          position: relative;
          overflow: hidden;
          transition: border-color 0.15s;
        }
        .touchpad.active {
          border-color: var(--primary-color, #03a9f4);
        }
        .touchpad-hint {
          position: absolute;
          top: 50%;
          left: 50%;
          transform: translate(-50%, -50%);
          color: var(--secondary-text-color, #999);
          font-size: 13px;
          pointer-events: none;
          opacity: 0.5;
        }
        .touchpad.active .touchpad-hint {
          opacity: 0;
        }
        .buttons-row {
          display: grid;
          grid-template-columns: 1fr 0.7fr 1fr;
          gap: 8px;
          margin-top: 12px;
          flex: 0 0 auto;
        }
        .mouse-btn {
          padding: 14px 0;
          border: 2px solid var(--divider-color, #e0e0e0);
          border-radius: 10px;
          background: var(--secondary-background-color, #f0f0f0);
          color: var(--primary-text-color);
          font-size: 13px;
          font-weight: 500;
          cursor: pointer;
          text-align: center;
          touch-action: manipulation;
          transition: background 0.1s, border-color 0.1s;
        }
        .mouse-btn:active, .mouse-btn.pressed {
          background: var(--primary-color, #03a9f4);
          color: #fff;
          border-color: var(--primary-color, #03a9f4);
        }
        .mouse-btn.held {
          background: var(--accent-color, #ff9800);
          color: #fff;
          border-color: var(--accent-color, #ff9800);
        }
        .scroll-row {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 8px;
          margin-top: 8px;
          flex: 0 0 auto;
        }
        .scroll-btn {
          padding: 10px 0;
          border: 2px solid var(--divider-color, #e0e0e0);
          border-radius: 10px;
          background: var(--secondary-background-color, #f0f0f0);
          color: var(--primary-text-color);
          font-size: 18px;
          cursor: pointer;
          text-align: center;
          touch-action: manipulation;
          transition: background 0.1s, border-color 0.1s;
        }
        .scroll-btn:active {
          background: var(--primary-color, #03a9f4);
          color: #fff;
          border-color: var(--primary-color, #03a9f4);
        }
      </style>
      <div class="card">
        <div class="zoom">
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M11 1.5v8.5l4 4.5h3.5l-2-2.5L20 8.5 14 5.5V1.5l-3 0zm-1 0L7 1.5v4L3.5 8.5 7 12l-2 2.5H8.5l4-4.5V1.5z" opacity="0"/><path d="M12 2C8.14 2 5 5.14 5 9v6c0 3.86 3.14 7 7 7s7-3.14 7-7V9c0-3.86-3.14-7-7-7zm0 2c2.76 0 5 2.24 5 5v2h-4V5h-2v6H7V9c0-2.76 2.24-5 5-5z"/></svg>
          <span class="header-name">${this._config.name || 'Mouse Control'}</span>
          <div class="header-right" id="host-switcher" style="display:none">
            <span class="host-addr"></span>
            <button class="host-btn" id="host-prev">&#9664;</button>
            <div class="host-info"><div class="host-name"></div></div>
            <button class="host-btn" id="host-next">&#9654;</button>
            <button class="host-btn host-forget" id="host-forget" style="display:none" title="Remove the Bluetooth pairing for the host shown and clear its stored address.&#10;Disconnects it if it is the active host.&#10;&#10;Tap once to arm, tap again to confirm.">Forget</button>
          </div>
        </div>
        <div class="touchpad" id="touchpad">
          <span class="touchpad-hint">Drag to move cursor</span>
        </div>
        <div class="buttons-row">
          <button class="mouse-btn" id="btn-left">Left</button>
          <button class="mouse-btn" id="btn-middle">Middle</button>
          <button class="mouse-btn" id="btn-right">Right</button>
        </div>
        <div class="scroll-row">
          <button class="scroll-btn" id="scroll-up">&#9650; Scroll Up</button>
          <button class="scroll-btn" id="scroll-down">&#9660; Scroll Down</button>
        </div>
        </div>
      </div>
    `;

    this._heldBtn = 0;
    this._setupTouchpad();
    this._setupButtons();
    this._setupScroll();
    this._setupHostSwitcher(shadow);

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
  }

  _callService(service, data) {
    if (!this._hass) return;
    // A linked keyboard takes the same things as action strings, passed on by
    // the keyboard this card points at. Movement and scrolling that arrive
    // together are added up there, so a drag cannot outrun the link.
    if (this._peerName()) {
      const action =
        service === 'mouse_move' ? `mouse_move:${data.x}:${data.y}` :
        service === 'mouse_click' ? `mouse_click:${data.btn}` :
        service === 'mouse_hold' ? `mouse_hold:${data.btn}` :
        service === 'mouse_release' ? 'mouse_release' :
        service === 'mouse_scroll' ? `mouse_scroll:${data.amount}` :
        service === 'switch_host' ? `switch_host:${data.slot}` :
        service === 'forget_host' ? `forget_host:${data.slot}` : null;
      if (action === null) return;   // nothing else is sent to a linked keyboard
      this._hass.callService('esphome', this._svc('run_action'),
        { action: `peer:${this._peerName()}:${action}` });
      return;
    }
    this._hass.callService('esphome', this._svc(service), data);
  }

  // ── Host switcher ────────────────────────────────────────────────
  // Mirrors the keyboard card: prev/next arrows around the active host's name,
  // with its MAC to the left. A switch made here reaches the other cards two
  // ways — instantly if the active-host sensor exists, otherwise on their next
  // poll below. Either is enough; the sensor just removes the lag.

  _setupHostSwitcher(shadow) {
    if (this._hostChain().length < 2) return;
    this._hostSlots = [];

    shadow.getElementById('host-switcher').style.display = '';
    this._hostInfoEl = shadow.querySelector('.host-info');
    this._hostNameEl = shadow.querySelector('.host-name');
    this._hostAddrEl = shadow.querySelector('.host-addr');

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

    const step = (delta) => this._stepHost(delta);
    shadow.getElementById('host-prev').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      step(-1);
    });
    shadow.getElementById('host-next').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      step(1);
    });

    // Forget host: two taps, as on the device page. A brush against it only
    // arms, and the armed tap belongs to the host that was showing — stepping
    // away disarms it, in _updateHostDisplay.
    if (this._config.show_forget_host) {
      this._forgetEl = shadow.getElementById('host-forget');
      this._forgetEl.style.display = '';
      this._forgetTitle = this._forgetEl.title;
      this._forgetEl.addEventListener('click', () => {
        if (this._forgetOff) return;   // nothing to call; the tooltip says why
        if (!this._forgetArm) {
          this._forgetArm = this._shownHost();
          this._forgetEl.textContent = 'Confirm?';
          this._forgetEl.classList.add('armed');
          this._forgetTimer = setTimeout(() => this._disarmForget(), 3000);
        } else {
          this._disarmForget();
          this._forgetHost();
        }
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

  // Which keyboard this card drives: the one the selected host belongs to — or,
  // drawn inside a remote card, whichever that card's keys go to.
  _peerName() {
    if (this._peerSource) return this._peerSource();
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
    let i = this._chainIndex(chain);
    if (wholeKeyboard) {
      const from = chain[i].peer;
      do { i = (i + 1) % chain.length; } while (chain[i].peer === from && chain[i].slot !== 0);
    } else {
      i = (i + delta + chain.length) % chain.length;
    }
    const c = chain[i];
    this._target = { peer: c.peer, slot: c.slot, entry: c.entry, name: c.name };
    this._activeSlot = c.slot;
    this._switchHost(c.slot);
  }

  // A linked keyboard's host is named by its peer_hosts entry, and this
  // keyboard's /hosts says nothing about its address.
  _peerHostLabel() {
    const t = this._target;
    if (!t || !t.peer) return null;
    const label = (t.entry && (t.entry.label || t.entry.peer)) || t.peer;
    return `${label}: ${t.name || 'Host ' + (t.slot + 1)}`;
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
    this._callService('switch_host', { slot });
    this._updateHostDisplay();
  }

  // The host the switcher is showing: a linked keyboard's comes from the
  // target, this keyboard's from the slot it is really on.
  _shownHost() {
    const t = this._target;
    return t && t.peer ? { peer: t.peer, slot: t.slot } : { peer: null, slot: this._activeSlot || 0 };
  }

  // The action name as the call sends it: Home Assistant registers an ESPHome
  // device's actions under its name with the dashes underscored.
  _svc(name) {
    return `${this._config.device.replace(/-/g, '_')}_${name}`;
  }

  // Why Forget cannot do anything, or null when it can. The device publishes
  // forget_host only with api_services: true, or a hand-written service of that
  // name, and a config grown from the per-card yaml snippets often has
  // switch_host without it — in which case the call comes back as an unknown
  // action in a toast, after the host has already been confirmed away.
  _forgetUnavailable() {
    // No registry at all (an early hass, or a harness): assume the call works
    // rather than grey out a button that would have.
    const reg = this._hass && this._hass.services;
    if (!reg) return null;
    // A linked keyboard's host is forgotten by that keyboard, through this
    // one's run_action, so that is the action which has to exist.
    const name = this._shownHost().peer ? 'run_action' : 'forget_host';
    if (reg.esphome && reg.esphome[this._svc(name)]) return null;
    return `This device publishes no ${name} action.\n`
      + `Set api_services: true on the component and flash it, or add a ${name} service to`
      + ` its api: section. A device Home Assistant has not connected to has no actions either.`;
  }

  _syncForget() {
    if (!this._forgetEl) return;
    const why = this._forgetUnavailable();
    if (why === this._forgetOff) return;
    this._forgetOff = why;
    if (why) this._disarmForget();
    this._forgetEl.classList.toggle('off', !!why);
    this._forgetEl.setAttribute('aria-disabled', why ? 'true' : 'false');
    // Not the disabled attribute: a disabled button takes no pointer events, so
    // the tooltip that explains all this would never show.
    this._forgetEl.title = why || this._forgetTitle;
  }

  _disarmForget() {
    clearTimeout(this._forgetTimer);
    this._forgetArm = null;
    if (!this._forgetEl) return;
    this._forgetEl.textContent = 'Forget';
    this._forgetEl.classList.remove('armed');
  }

  // Drops the pairing for the host shown — the service the device page's Forget
  // Host posts to, and for a linked keyboard's host, that keyboard's own.
  _forgetHost() {
    const h = this._shownHost();
    this._callService('forget_host', { slot: h.slot });
    // That slot is empty now; read it back rather than wait out the 30s poll.
    setTimeout(() => this._pollHosts(), 1000);
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
        this._hostDataAvailable = true;
        this._updateHostDisplay();
      })
      .catch(() => this._updateHostDisplay());
  }

  _updateHostDisplay() {
    if (!this._hostNameEl) return;
    this._fitHostInfo();
    // An armed Forget belonged to the host it was armed on, never the new one.
    const armed = this._forgetArm, now = this._shownHost();
    if (armed && (armed.peer !== now.peer || armed.slot !== now.slot)) this._disarmForget();
    this._syncForget();
    const peerLabel = this._peerHostLabel();
    if (peerLabel !== null) {
      this._hostNameEl.textContent = peerLabel;
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

  // ── Touchpad: drag-to-move + tap-to-click ────────────────────────

  _setupTouchpad() {
    const pad = this.shadowRoot.getElementById('touchpad');
    let tracking = false;
    let lastX = 0;
    let lastY = 0;
    let lastTime = 0;
    let startTime = 0;
    let moved = false;
    let startSX = 0;
    let startSY = 0;

    // Accumulator for sub-pixel movements
    let accumX = 0;
    let accumY = 0;

    const baseSens = this._config.sensitivity;
    const accelFactor = this._config.mouse_acceleration;
    const maxSens = this._config.mouse_max_speed;
    const tapDeadZone = 5;
    const scrollSensitivity = this._config.scroll_sensitivity;

    const onStart = (x, y) => {
      tracking = true;
      lastX = startSX = x;
      lastY = startSY = y;
      lastTime = startTime = Date.now();
      moved = false;
      accumX = 0;
      accumY = 0;
      pad.classList.add('active');
    };

    const onMove = (x, y) => {
      if (!tracking) return;

      // Ignore tiny jitter so taps still register
      if (!moved) {
        const td = Math.abs(x - startSX) + Math.abs(y - startSY);
        if (td < tapDeadZone) return;
      }

      const now = Date.now();
      const dt = Math.max(now - lastTime, 1);
      const rawDx = x - lastX;
      const rawDy = y - lastY;
      const dist = Math.sqrt(rawDx * rawDx + rawDy * rawDy);
      const speed = dist / dt;
      const sens = Math.min(baseSens + speed * accelFactor, maxSens);

      accumX += rawDx * sens;
      accumY += rawDy * sens;

      const dx = Math.trunc(accumX);
      const dy = Math.trunc(accumY);

      if (dx !== 0 || dy !== 0) {
        const clampedX = Math.max(-127, Math.min(127, dx));
        const clampedY = Math.max(-127, Math.min(127, dy));
        this._callService('mouse_move', { x: clampedX, y: clampedY });
        accumX -= dx;
        accumY -= dy;
        moved = true;
      }

      lastX = x;
      lastY = y;
      lastTime = now;
    };

    const onEnd = () => {
      if (!tracking) return;
      tracking = false;
      pad.classList.remove('active');

      // Tap-to-click: short tap with no drag beyond dead zone. Suppressed
      // while a button is held so a stray tap can't drop the drag.
      if (this._config.tap_to_click && !moved && !this._heldBtn && (Date.now() - startTime) < 300) {
        this._callService('mouse_click', { btn:1 });
      }
    };

    // Pointer events for drag tracking
    pad.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      pad.setPointerCapture(e.pointerId);
      onStart(e.clientX, e.clientY);
    });
    pad.addEventListener('pointermove', (e) => {
      onMove(e.clientX, e.clientY);
    });
    pad.addEventListener('pointerup', (e) => {
      pad.releasePointerCapture(e.pointerId);
      onEnd();
    });
    pad.addEventListener('pointercancel', (e) => {
      pad.releasePointerCapture(e.pointerId);
      onEnd();
    });

    // Mouse wheel / trackpad scroll on desktop
    let wheelAccum = 0;
    pad.addEventListener('wheel', (e) => {
      e.preventDefault();
      wheelAccum += -e.deltaY * scrollSensitivity * 0.02;
      const intScroll = Math.trunc(wheelAccum);
      if (intScroll !== 0) {
        const clamped = Math.max(-127, Math.min(127, intScroll));
        this._callService('mouse_scroll', { amount: clamped });
        wheelAccum -= intScroll;
      }
    }, { passive: false });
  }

  // ── Click buttons: tap = click; long-press (400ms) = hold for dragging;
  // tap the held button = release. Held state is client-side only — a normal
  // click releases everything device-side too, so it also clears the mark.

  _setupButtons() {
    const btnMap = {
      'btn-left': 1,
      'btn-middle': 4,
      'btn-right': 2,
    };
    const setHeld = (b) => {
      this._heldBtn = b;
      for (const [id, button] of Object.entries(btnMap)) {
        this.shadowRoot.getElementById(id).classList.toggle('held', b === button);
      }
    };
    for (const [id, button] of Object.entries(btnMap)) {
      const el = this.shadowRoot.getElementById(id);
      let holdTimer = null;
      let ok = false;
      const clearHold = () => { if (holdTimer) { clearTimeout(holdTimer); holdTimer = null; } };
      el.addEventListener('contextmenu', (e) => e.preventDefault());
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        ok = true;
        el.classList.add('pressed');
        clearHold();
        holdTimer = setTimeout(() => {
          holdTimer = null;
          if (!ok) return;
          ok = false;
          el.classList.remove('pressed');
          this._callService('mouse_hold', { btn: button });
          setHeld(button);
        }, 400);
      });
      el.addEventListener('pointerup', () => {
        el.classList.remove('pressed');
        clearHold();
        if (!ok) return;
        ok = false;
        if (this._heldBtn === button) {
          this._callService('mouse_release', {});
          setHeld(0);
        } else {
          this._callService('mouse_click', { btn: button });
          setHeld(0);
        }
      });
      el.addEventListener('pointerleave', () => { ok = false; el.classList.remove('pressed'); clearHold(); });
      el.addEventListener('pointercancel', () => { ok = false; el.classList.remove('pressed'); clearHold(); });
    }
  }

  // ── Scroll buttons ───────────────────────────────────────────────

  _setupScroll() {
    const upBtn = this.shadowRoot.getElementById('scroll-up');
    const downBtn = this.shadowRoot.getElementById('scroll-down');

    let scrollInterval = null;

    const startScroll = (amount) => {
      this._callService('mouse_scroll', { amount });
      scrollInterval = setInterval(() => {
        this._callService('mouse_scroll', { amount });
      }, 150);
    };

    const stopScroll = () => {
      if (scrollInterval) {
        clearInterval(scrollInterval);
        scrollInterval = null;
      }
    };

    for (const [btn, amount] of [[upBtn, 3], [downBtn, -3]]) {
      btn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        btn.classList.add('pressed');
        startScroll(amount);
      });
      btn.addEventListener('pointerup', () => { btn.classList.remove('pressed'); stopScroll(); });
      btn.addEventListener('pointerleave', () => { btn.classList.remove('pressed'); stopScroll(); });
    }
  }

  // The 16:9 touchpad plus ~176px of chrome is ~400px in a typical
  // masonry column (1 unit = ~50px).
  getCardSize() {
    return 8;
  }

  // Sections-view sizing. 'auto' keeps the 16:9 touchpad at its natural
  // height, as it laid out before it declared any grid options; pick a row
  // count instead and the touchpad flexes to fill it, or scrolls if the card
  // is shorter than the ~176px of header and buttons around it.
  //
  // Half width by default, since a touchpad pairs well beside another card,
  // but the bounds are left wide open — 1-12 columns, and 1-8 rows, 8 being as
  // far as HA's height slider goes. max_rows is deliberately absent: the slider
  // stops at 8 either way (rowMax falls back to the size picker's own `rows`),
  // but with no bound declared computeCardGridSize() applies no upper clamp, so
  // `grid_options: {rows: N}` in the YAML editor can go taller.
  getGridOptions() {
    return {
      columns: 6,
      min_columns: 1,
      rows: 'auto',
      min_rows: 1,
    };
  }

  // Enables the dashboard's visual editor; YAML editing is unaffected.
  static getConfigElement() {
    return document.createElement('ble-mouse-card-editor');
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-mouse-card', BleMouseCard);

/* ── Visual editor ───────────────────────────────────────────────────────
 * Prefers Home Assistant's own <ha-form> so the panel matches built-in cards,
 * falling back to plain inputs if ha-form isn't registered.
 *
 * `host_names` is a YAML list, so it edits here as one comma-separated field
 * and is converted back to a list on the way out (see setConfig / _emit).
 * ---------------------------------------------------------------------- */

// peer_hosts is a YAML list of mappings, and an editor field is a box of text:
// one linked keyboard per line, its fields in the order the YAML keys come in.
//   bedroom | 2 | Bed TV, Bed PC | Bedroom
// Trailing fields can be left off, and the host count fills itself in from the
// names when it is missing.
const peerHostsToText = (list) => (Array.isArray(list) ? list : []).map((k) =>
  [k.peer || '', k.slots || 0, (k.names || []).join(', '), k.label || '']
    .join(' | ').replace(/(\s*\|)+$/, '')).join('\n');

const peerHostsFromText = (text) => String(text).split('\n').map((line) => {
  const f = line.split('|').map((x) => x.trim());
  if (!f[0]) return null;
  const names = (f[2] || '').split(',').map((n) => n.trim()).filter(Boolean);
  const slots = parseInt(f[1], 10);
  const entry = { peer: f[0], slots: slots > 0 ? slots : (names.length || 1) };
  if (names.length) entry.names = names;
  if (f[3]) entry.label = f[3];
  return entry;
}).filter(Boolean);

const MOUSE_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'zoom', selector: { number: { min: 0.25, max: 3, step: 0.05, mode: 'box' } } },
  { name: 'sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'mouse_acceleration', selector: { number: { min: 0, max: 2, step: 0.05, mode: 'box' } } },
  { name: 'mouse_max_speed', selector: { number: { min: 0.5, max: 20, step: 0.5, mode: 'box' } } },
  { name: 'scroll_sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'tap_to_click', selector: { boolean: {} } },
  { name: 'host_slots', selector: { text: {} } },
  { name: 'host_names', selector: { text: {} } },
  { name: 'peer_hosts', selector: { text: { multiline: true } } },
  { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'show_mac', selector: { boolean: {} } },
  { name: 'show_forget_host', selector: { boolean: {} } },
  { name: 'host_url', selector: { text: {} } },
];

const MOUSE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  zoom: 'Zoom (1 = normal, 0.5 = half, 2 = double)',
  sensitivity: 'Pointer sensitivity',
  mouse_acceleration: 'Acceleration',
  mouse_max_speed: 'Max speed multiplier',
  scroll_sensitivity: 'Scroll sensitivity',
  tap_to_click: 'Tap touchpad to click',
  host_slots: 'Host switcher: how many hosts, or which — 1-3,5,7-10 (0 = hide)',
  host_names: 'Host names, comma-separated (optional)',
  peer_hosts: 'Linked keyboards, one per line: bedroom | 2 | Bed TV, Bed PC | Bedroom',
  active_host_entity: 'Active-host sensor (optional)',
  show_mac: 'Show host MAC address',
  show_forget_host: 'Show forget host button (two taps to confirm)',
  host_url: 'Device URL (optional, auto-detected)',
};

class BleMouseCardEditor extends HTMLElement {
  setConfig(config) {
    this._config = {
      sensitivity: 1.5,
      mouse_acceleration: 0.15,
      mouse_max_speed: 4.5,
      scroll_sensitivity: 2,
      tap_to_click: true,
      host_slots: 0,
      show_mac: true,
      show_forget_host: false,
      zoom: 1,
      ...config,
    };
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
      form.schema = MOUSE_EDITOR_SCHEMA;
      form.computeLabel = (s) => MOUSE_EDITOR_LABELS[s.name] || s.name;
      form.addEventListener('value-changed', (e) => this._emit(e.detail.value));
      this.appendChild(form);
      this._form = form;
      return;
    }

    this.appendChild(buildMouseFallbackEditor(
      MOUSE_EDITOR_SCHEMA, MOUSE_EDITOR_LABELS, () => this._config, (cfg) => this._emit(cfg),
    ));
  }
}

// `getConfig` is a function, not an object: _emit() replaces this._config on
// every change, so a captured object would go stale after the first edit.
function buildMouseFallbackEditor(schema, labels, getConfig, onChange) {
  const config = getConfig();
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;gap:10px;padding:8px 0';
  schema.forEach((item) => {
    const row = document.createElement('label');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;font-size:14px';
    const isBool = !!item.selector.boolean;
    const input = document.createElement('input');
    if (isBool) {
      input.type = 'checkbox';
      input.checked = config[item.name] === true;
    } else {
      input.type = item.selector.number ? 'number' : 'text';
      if (item.selector.number && item.selector.number.step) input.step = item.selector.number.step;
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

customElements.define('ble-mouse-card-editor', BleMouseCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-mouse-card',
  name: 'BLE Mouse Control',
  description: 'Touchpad, mouse buttons, and scroll for BLE Keyboard component',
  preview: true,
});
