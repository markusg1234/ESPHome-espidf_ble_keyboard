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
 *   3. Add ESPHome services to your device YAML (see README). This card uses
 *      run_action (every button) and send_string (the optional number pad) —
 *      the simplest way to get both is `api_services: true` on the component.
 *
 * Every button fires a *named* action, so any of them can be remapped per host
 * slot via the component's `actions:` config or the web UI's Host Actions card.
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-remote-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # Optional overrides:
 *   # name: Media Remote           # card title (auto from HA if omitted)
 *   # show_numpad: true            # show number pad (default false)
 *   # show_apps: true              # show app launch row (default true)
 *   # show_color: true             # show color buttons (default false)
 *   # hidden_entity: sensor.x_hidden_buttons   # override the auto-detected id
 *   # host_slots: 4                # show host switcher (needs >1; default 0 = hidden)
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *   # active_host_entity: sensor.bluetooth_keyboard_active_host  # (auto-detected)
 *   # show_mac: true               # show the active host's MAC address (default true)
 *   # host_url: http://192.168.1.50  # ESP address (auto-detected from HA)
 *
 * Per-host hiding: if the device exposes the optional `hidden_buttons` text
 * sensor, this card hides whatever the active host hides on the web remote, and
 * follows a host switch live. Without that sensor every button is shown.
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

class BleRemoteCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
    this._applyHidden();
    // Track active host changes via HA sensor entity. The firmware publishes to
    // this sensor on every switch_host() path — HA service, the device's own web
    // UI, a physical button — so every card following it stays in step with the
    // others without polling.
    if (this._config.host_slots > 1) {
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
      show_numpad: config.show_numpad === true,
      show_apps: config.show_apps !== false,
      show_color: config.show_color === true,
      // Optional text sensor carrying the active host's hidden buttons, so the
      // card mirrors the web remote's per-host hiding. Absent entity = show all.
      hidden_entity: config.hidden_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_hidden_buttons`,
      host_slots: config.host_slots || 0,
      host_names: config.host_names || [],
      active_host_entity: config.active_host_entity || null,
      show_mac: config.show_mac !== false,
      host_url: config.host_url || null,
    };
  }

  // Hide the buttons the active host has no use for. Driven by the text sensor,
  // so it follows a host switch without a dashboard reload.
  _applyHidden() {
    if (!this._hass || !this.shadowRoot) return;
    const ent = this._hass.states[this._config.hidden_entity];
    const raw = ent && typeof ent.state === 'string' &&
                ent.state !== 'unknown' && ent.state !== 'unavailable' ? ent.state : '';
    if (raw === this._lastHidden) return;   // states stream constantly; only act on change
    this._lastHidden = raw;

    const hide = raw ? raw.split(',').map(s => s.trim()).filter(Boolean) : [];
    const map = this._actionMap || {};
    Object.keys(map).forEach(id => {
      const el = this.shadowRoot.getElementById(id);
      if (el) el.style.display = hide.includes(map[id]) ? 'none' : '';
    });
    // App buttons are built dynamically and carry their action on the element.
    this.shadowRoot.querySelectorAll('[data-action]').forEach(el => {
      el.style.display = hide.includes(el.dataset.action) ? 'none' : '';
    });
    // Collapse the optional rows once everything in them is hidden.
    ['color-section', 'app-section'].forEach(id => {
      const sec = this.shadowRoot.getElementById(id);
      if (!sec || sec.dataset.enabled !== '1') return;
      const anyVisible = [...sec.querySelectorAll('button')].some(b => b.style.display !== 'none');
      sec.style.display = anyVisible ? '' : 'none';
    });
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        :host { display: block; }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          overflow: hidden;
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
        .host-info { text-align: center; min-width: 0; }
        .host-name { font-size: 12px; font-weight: 600; color: var(--primary-text-color, #333); line-height: 1.2; }
        .host-addr { font-size: 10px; font-weight: 400; font-family: monospace; white-space: nowrap; color: var(--secondary-text-color, #888); }

        /* Button grid sections */
        .section { margin-bottom: 12px; }
        .section:last-child { margin-bottom: 0; }
        .row { display: flex; justify-content: center; gap: 8px; margin-bottom: 8px; }
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
        .dpad { display: grid; grid-template-columns: 52px 52px 52px; grid-template-rows: 52px 52px 52px; gap: 4px; justify-content: center; margin: 8px 0; }
        .dpad .btn { border-radius: 12px; }
        .dpad .center { background: var(--primary-color, #03a9f4); color: #fff; border-color: var(--primary-color, #03a9f4); font-size: 11px; font-weight: 700; border-radius: 50%; }
        .dpad .center:active { background: var(--accent-color, #ff9800); }
        .dpad .empty { visibility: hidden; }

        /* Wide buttons */
        .btn.wide { width: auto; border-radius: 26px; padding: 0 18px; font-size: 12px; }

        /* Volume/channel strip */
        .strip { display: flex; align-items: center; justify-content: center; gap: 16px; }
        .strip-group { display: flex; flex-direction: column; align-items: center; gap: 4px; }
        .strip-label { font-size: 10px; color: var(--secondary-text-color, #888); font-weight: 600; text-transform: uppercase; }

        /* Media controls */
        .media-row { display: flex; justify-content: center; gap: 10px; }
        .btn.media { width: 46px; height: 46px; }

        /* Number pad */
        .numpad { display: grid; grid-template-columns: repeat(3, 52px); gap: 6px; justify-content: center; }

        /* App row */
        .app-row { display: flex; justify-content: center; gap: 8px; flex-wrap: wrap; }
        .btn.app { width: auto; border-radius: 26px; padding: 0 14px; height: 38px; font-size: 11px; }

        /* Divider */
        .divider { height: 1px; background: var(--divider-color, #e0e0e0); margin: 12px 0; }
      </style>
      <div class="card">
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M18 7V4c0-1.1-.9-2-2-2H8c-1.1 0-2 .9-2 2v3H2v15h20V7h-4zM8 4h8v3H8V4zm10 16H6V9h12v11zm-6-7c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2z"/></svg>
          <span class="header-name">${this._config.name || 'Media Remote'}</span>
          <div class="header-right" id="host-switcher" style="display:none">
            <span class="host-addr"></span>
            <button class="host-btn" id="host-prev">&#9664;</button>
            <div class="host-info"><div class="host-name"></div></div>
            <button class="host-btn" id="host-next">&#9654;</button>
          </div>
        </div>

        <!-- Power & Back row -->
        <div class="section">
          <div class="row">
            <button class="btn power" id="power" title="Power">
              <svg viewBox="0 0 24 24"><path d="M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42A6.92 6.92 0 0119 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.04.88-3.88 2.29-5.16L5.88 5.46A8.96 8.96 0 003 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.72-1.21-5.15-3.17-6.83z"/></svg>
            </button>
            <div style="flex:1"></div>
            <button class="btn" id="search" title="Search">
              <svg viewBox="0 0 24 24"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>
            </button>
            <button class="btn" id="info" title="Info / Guide">
              <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
            </button>
            <button class="btn" id="mute" title="Mute">
              <svg viewBox="0 0 24 24"><path d="M16.5 12A4.5 4.5 0 0014 7.97v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51A8.796 8.796 0 0021 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06a8.99 8.99 0 003.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></svg>
            </button>
            <button class="btn" id="home" title="Home (Win)">
              <svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg>
            </button>
            <button class="btn" id="back" title="Back (Escape)">
              <svg viewBox="0 0 24 24"><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg>
            </button>
          </div>
        </div>

        <!-- D-Pad Navigation -->
        <div class="section">
          <div class="dpad">
            <div class="empty"></div>
            <button class="btn" id="up" title="Up">
              <svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg>
            </button>
            <div class="empty"></div>
            <button class="btn" id="left" title="Left">
              <svg viewBox="0 0 24 24"><path d="M15.41 7.41L14 6l-6 6 6 6 1.41-1.41L10.83 12z"/></svg>
            </button>
            <button class="btn center" id="ok">OK</button>
            <button class="btn" id="right" title="Right">
              <svg viewBox="0 0 24 24"><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg>
            </button>
            <div class="empty"></div>
            <button class="btn" id="down" title="Down">
              <svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>
            </button>
            <div class="empty"></div>
          </div>
        </div>

        <!-- Volume & Channel -->
        <div class="section">
          <div class="strip">
            <div class="strip-group">
              <span class="strip-label">Vol</span>
              <button class="btn" id="vol_up" title="Volume Up">
                <svg viewBox="0 0 24 24"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></svg>
              </button>
              <button class="btn" id="vol_dn" title="Volume Down">
                <svg viewBox="0 0 24 24"><path d="M19 13H5v-2h14v2z"/></svg>
              </button>
            </div>
            <div style="width:40px"></div>
            <div class="strip-group">
              <span class="strip-label">Ch</span>
              <button class="btn" id="pg_up" title="Page Up (Channel Up)">
                <svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg>
              </button>
              <button class="btn" id="pg_dn" title="Page Down (Channel Down)">
                <svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>
              </button>
            </div>
          </div>
        </div>

        <div class="divider"></div>

        <!-- Media Controls -->
        <div class="section">
          <div class="media-row">
            <button class="btn media" id="prev" title="Previous Track">
              <svg viewBox="0 0 24 24"><path d="M6 6h2v12H6zm3.5 6l8.5 6V6z"/></svg>
            </button>
            <button class="btn media" id="rew" title="Rewind">
              <svg viewBox="0 0 24 24"><path d="M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"/></svg>
            </button>
            <button class="btn media" id="play" title="Play / Pause">
              <svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>
            </button>
            <button class="btn media" id="stop" title="Stop">
              <svg viewBox="0 0 24 24"><path d="M6 6h12v12H6z"/></svg>
            </button>
            <button class="btn media" id="fwd" title="Fast Forward">
              <svg viewBox="0 0 24 24"><path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/></svg>
            </button>
            <button class="btn media" id="next" title="Next Track">
              <svg viewBox="0 0 24 24"><path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/></svg>
            </button>
            <button class="btn media rec" id="rec" title="Record">
              <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="7"/></svg>
            </button>
          </div>
        </div>

        <!-- Color Buttons (optional) -->
        <div class="section" id="color-section" style="display:none">
          <div class="divider"></div>
          <div class="row">
            <button class="btn red" id="c_red"></button>
            <button class="btn green" id="c_green"></button>
            <button class="btn yellow" id="c_yellow"></button>
            <button class="btn blue" id="c_blue"></button>
          </div>
        </div>

        <!-- Number Pad (optional) -->
        <div class="section" id="numpad-section" style="display:none">
          <div class="divider"></div>
          <div class="numpad" id="numpad"></div>
        </div>

        <!-- App Launch Row (optional) -->
        <div class="section" id="app-section" style="display:none">
          <div class="divider"></div>
          <div class="app-row" id="app-row"></div>
        </div>
      </div>
    `;

    // Show optional sections. `enabled` marks the ones the config turned on, so
    // per-host hiding can collapse them without re-showing a section the user
    // disabled outright.
    if (this._config.show_color) {
      const sec = shadow.getElementById('color-section');
      sec.style.display = '';
      sec.dataset.enabled = '1';
    }

    if (this._config.show_numpad) {
      const section = shadow.getElementById('numpad-section');
      section.style.display = '';
      const numpad = shadow.getElementById('numpad');
      for (let i = 1; i <= 9; i++) {
        const btn = document.createElement('button');
        btn.className = 'btn';
        btn.textContent = i;
        btn.addEventListener('pointerdown', (e) => { 
          e.preventDefault(); 
          this._press(btn); 
          this._sendString(String(i)); 
        });
        numpad.appendChild(btn);
      }
      // Bottom row: empty, 0, empty
      const spacer1 = document.createElement('div');
      const zero = document.createElement('button');
      zero.className = 'btn';
      zero.textContent = '0';
      zero.addEventListener('pointerdown', (e) => { 
        e.preventDefault(); 
        this._press(zero); 
        this._sendString('0'); 
      });
      const spacer2 = document.createElement('div');
      numpad.appendChild(spacer1);
      numpad.appendChild(zero);
      numpad.appendChild(spacer2);
    }

    if (this._config.show_apps) {
      const section = shadow.getElementById('app-section');
      section.style.display = '';
      section.dataset.enabled = '1';
      const row = shadow.getElementById('app-row');
      // Named actions so these are remappable per host too. 'Search' shares the
      // Search button's action — both are AC Search 0x0221.
      const apps = [
        { name: 'Explorer', action: 'app_explorer' },  // 0x0194
        { name: 'Browser', action: 'app_browser' },    // 0x0223
        { name: 'Email', action: 'app_email' },        // 0x018A
        { name: 'Calc', action: 'app_calc' },          // 0x0192
        { name: 'Search', action: 'search' },          // 0x0221
      ];
      apps.forEach(app => {
        const btn = document.createElement('button');
        btn.className = 'btn app';
        btn.textContent = app.name;
        btn.dataset.action = app.action;   // so per-host hiding can find it
        btn.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          this._press(btn);
          this._runAction(app.action);
        });
        row.appendChild(btn);
      });
    }

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

    // Wire up all buttons
    this._wireButtons(shadow);
    this._setupHostSwitcher(shadow);
  }

  // ── Host switcher ────────────────────────────────────────────────
  // Mirrors the keyboard card: prev/next arrows around the active host's name,
  // with its MAC to the left. Every card watches the same active-host sensor, so
  // a switch made here shows up on the keyboard and mouse cards too — and
  // _applyHidden() repaints this card's own buttons for the new host.

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

  _press(btn) {
    btn.classList.add('p');
    setTimeout(() => btn.classList.remove('p'), 150);
  }

  _wireButtons(shadow) {
    // Button action map: id -> named action.
    //
    // These are named actions rather than raw HID codes so per-host `actions:`
    // overrides apply — e.g. a Windows host can remap record to Game Bar's
    // Win+Alt+R while a TV host keeps HID Record. Each name's built-in default
    // is the same code this card used to send directly.
    const actions = {
      // Power & navigation
      power:  'remote_power',   // HID consumer Power 0x0030 (not System Power Down)
      back:   'back',           // AC Back 0x0224
      home:   'home',           // AC Home 0x0223
      mute:   'mute',           // 0x00E2

      search: 'search',         // AC Search 0x0221
      info:   'info',           // AC More Info / Guide 0x0209

      // D-pad
      up:     'up',             // Menu Up 0x0042
      down:   'down',           // Menu Down 0x0043
      left:   'left',           // Menu Left 0x0044
      right:  'right',          // Menu Right 0x0045
      ok:     'ok',             // Enter 0x28 (not Menu Pick — see README)

      // Volume
      vol_up: 'volume_up',      // 0x00E9
      vol_dn: 'volume_down',    // 0x00EA

      // Channel (Page Up/Down)
      pg_up:  'channel_up',     // Page Up
      pg_dn:  'channel_down',   // Page Down

      // Media
      play:   'play_pause',     // 0x00CD
      stop:   'stop',           // 0x00B7
      prev:   'prev_track',     // 0x00B6
      next:   'next_track',     // 0x00B5
      rew:    'rewind',         // 0x00B4
      fwd:    'fast_forward',   // 0x00B3
      rec:    'record',         // 0x00B2

      // Color buttons (F1-F4, common for media apps)
      c_red:    'color_red',
      c_green:  'color_green',
      c_yellow: 'color_yellow',
      c_blue:   'color_blue',
    };

    // Kept so _applyHidden can map a hidden action name back to its button id.
    this._actionMap = actions;

    for (const [id, action] of Object.entries(actions)) {
      const el = shadow.getElementById(id);
      if (!el) continue;

      // Support hold-to-repeat for volume and channel
      if (['vol_up', 'vol_dn', 'pg_up', 'pg_dn'].includes(id)) {
        let interval = null;
        const start = (e) => {
          e.preventDefault();
          this._press(el);
          this._runAction(action);
          interval = setInterval(() => this._runAction(action), 180);
        };
        const stop = () => { if (interval) { clearInterval(interval); interval = null; } };
        el.addEventListener('pointerdown', start);
        el.addEventListener('pointerup', stop);
        el.addEventListener('pointerleave', stop);
        el.addEventListener('pointercancel', stop);
      } else {
        el.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          this._press(el);
          this._runAction(action);
        });
      }
    }
  }

  // Used by the optional number pad, which types digits rather than sending a
  // fixed HID code — the one part of the card that isn't a named action.
  _sendString(text) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_string`, { keys: text });
  }

  _runAction(action) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_run_action`, { action });
  }

  getCardSize() {
    return 6;
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

const REMOTE_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'show_numpad', selector: { boolean: {} } },
  { name: 'show_apps', selector: { boolean: {} } },
  { name: 'show_color', selector: { boolean: {} } },
  { name: 'hidden_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'host_slots', selector: { number: { min: 0, max: 10, step: 1, mode: 'box' } } },
  { name: 'host_names', selector: { text: {} } },
  { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'show_mac', selector: { boolean: {} } },
  { name: 'host_url', selector: { text: {} } },
];

const REMOTE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  show_numpad: 'Show number pad',
  show_apps: 'Show app launcher row',
  show_color: 'Show colour buttons',
  hidden_entity: 'Hidden-buttons sensor (optional)',
  host_slots: 'Host switcher (needs 2+; 0 = hide)',
  host_names: 'Host names, comma-separated (optional)',
  active_host_entity: 'Active-host sensor (optional)',
  show_mac: 'Show host MAC address',
  host_url: 'Device URL (optional, auto-detected)',
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
      form.schema = REMOTE_EDITOR_SCHEMA;
      form.computeLabel = (s) => REMOTE_EDITOR_LABELS[s.name] || s.name;
      form.addEventListener('value-changed', (e) => this._emit(e.detail.value));
      this.appendChild(form);
      this._form = form;
      return;
    }

    this.appendChild(buildFallbackEditor(
      REMOTE_EDITOR_SCHEMA, REMOTE_EDITOR_LABELS, () => this._config,
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
    const row = document.createElement('label');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;font-size:14px';
    const isBool = !!item.selector.boolean;
    const input = document.createElement(item.selector.select ? 'select' : 'input');
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
