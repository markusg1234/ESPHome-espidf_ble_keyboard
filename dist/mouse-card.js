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
 *   # Optional overrides:
 *   # name: Mouse Control          # card title (default "Mouse Control")
 *   # sensitivity: 1.5             # movement multiplier (default 1.5)
 *   # mouse_acceleration: 0.15     # speed-based acceleration factor (default 0.15)
 *   # mouse_max_speed: 4.5         # max sensitivity cap (default 4.5)
 *   # scroll_sensitivity: 2        # scroll multiplier (default 2)
 *   # tap_to_click: true           # tap touchpad = left click (default true)
 *   # host_slots: 4                # show host switcher (needs >1; default 0 = hidden)
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *   # active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
 *   # show_mac: true               # show the active host's MAC address (default true)
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
    this._config = {
      device: config.device,
      name: config.name || null,
      sensitivity: config.sensitivity || 1.5,
      mouse_acceleration: config.mouse_acceleration || 0.15,
      mouse_max_speed: config.mouse_max_speed || 4.5,
      scroll_sensitivity: config.scroll_sensitivity || 2,
      tap_to_click: config.tap_to_click !== false,
      host_slots: config.host_slots || 0,
      host_names: config.host_names || [],
      active_host_entity: config.active_host_entity || null,
      show_mac: config.show_mac !== false,
      host_url: config.host_url || null,
    };
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        :host {
          display: block;
        }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          color: var(--primary-text-color);
          user-select: none;
          -webkit-user-select: none;
        }
        .header {
          font-size: 16px;
          font-weight: 500;
          margin-bottom: 12px;
          display: flex;
          align-items: center;
          gap: 8px;
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
        .host-info {
          text-align: center;
          min-width: 0;
        }
        .host-name {
          font-size: 12px;
          font-weight: 600;
          color: var(--primary-text-color);
          line-height: 1.2;
        }
        .host-addr {
          font-size: 12px;
          font-weight: 400;
          color: var(--secondary-text-color, #999);
          font-family: monospace;
          white-space: nowrap;
        }
        .touchpad {
          width: 100%;
          aspect-ratio: 16/9;
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
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M11 1.5v8.5l4 4.5h3.5l-2-2.5L20 8.5 14 5.5V1.5l-3 0zm-1 0L7 1.5v4L3.5 8.5 7 12l-2 2.5H8.5l4-4.5V1.5z" opacity="0"/><path d="M12 2C8.14 2 5 5.14 5 9v6c0 3.86 3.14 7 7 7s7-3.14 7-7V9c0-3.86-3.14-7-7-7zm0 2c2.76 0 5 2.24 5 5v2h-4V5h-2v6H7V9c0-2.76 2.24-5 5-5z"/></svg>
          <span class="header-name">${this._config.name || 'Mouse Control'}</span>
          <div class="header-right" id="host-switcher" style="display:none">
            <span class="host-addr"></span>
            <button class="host-btn" id="host-prev">&#9664;</button>
            <div class="host-info"><div class="host-name"></div></div>
            <button class="host-btn" id="host-next">&#9654;</button>
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
    const wantsUrl = this._config.host_slots > 1 && !this._config.host_url;
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
    const slug = this._config.device.replace(/-/g, '_');
    this._hass.callService('esphome', `${slug}_${service}`, data);
  }

  // ── Host switcher ────────────────────────────────────────────────
  // Mirrors the keyboard card: prev/next arrows around the active host's name,
  // with its MAC to the left. A switch made here reaches the other cards two
  // ways — instantly if the active-host sensor exists, otherwise on their next
  // poll below. Either is enough; the sensor just removes the lag.

  _setupHostSwitcher(shadow) {
    if (this._config.host_slots < 2) return;
    this._activeSlot = 0;
    this._hostSlots = [];

    shadow.getElementById('host-switcher').style.display = '';
    this._hostNameEl = shadow.querySelector('.host-name');
    this._hostAddrEl = shadow.querySelector('.host-addr');

    const step = (delta) => {
      const n = this._config.host_slots;
      this._switchHost((this._activeSlot + delta + n) % n);
    };
    shadow.getElementById('host-prev').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      step(-1);
    });
    shadow.getElementById('host-next').addEventListener('pointerdown', (e) => {
      e.preventDefault();
      step(1);
    });

    this._updateHostDisplay();
    this._startHostPolling();
  }

  _switchHost(slot) {
    if (!this._hass) return;
    this._activeSlot = slot;
    this._callService('switch_host', { slot });
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
    if (!this._hass || this._config.host_slots < 2) return;
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
      this._hostAddrEl.textContent = (apiSlot && apiSlot.occupied && apiSlot.addr) ? apiSlot.addr : 'Empty';
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

  getCardSize() {
    return 4;
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

const MOUSE_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'mouse_acceleration', selector: { number: { min: 0, max: 2, step: 0.05, mode: 'box' } } },
  { name: 'mouse_max_speed', selector: { number: { min: 0.5, max: 20, step: 0.5, mode: 'box' } } },
  { name: 'scroll_sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'tap_to_click', selector: { boolean: {} } },
  { name: 'host_slots', selector: { number: { min: 0, max: 10, step: 1, mode: 'box' } } },
  { name: 'host_names', selector: { text: {} } },
  { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'show_mac', selector: { boolean: {} } },
  { name: 'host_url', selector: { text: {} } },
];

const MOUSE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  sensitivity: 'Pointer sensitivity',
  mouse_acceleration: 'Acceleration',
  mouse_max_speed: 'Max speed multiplier',
  scroll_sensitivity: 'Scroll sensitivity',
  tap_to_click: 'Tap touchpad to click',
  host_slots: 'Host switcher (needs 2+; 0 = hide)',
  host_names: 'Host names, comma-separated (optional)',
  active_host_entity: 'Active-host sensor (optional)',
  show_mac: 'Show host MAC address',
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
      ...config,
    };
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
