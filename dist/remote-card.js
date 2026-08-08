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
 *   # Optional overrides:
 *   # name: Media Remote           # card title (auto from HA if omitted)
 *   # remote_style: auto           # auto | default | style1..style5 | custom
 *   # remote_style_json: '{...}'   # the style, when remote_style is custom
 *   # remote_style_entity: sensor.x_remote_style   # override the auto-detected id
 *   # show_numpad: true            # show number pad (default false)
 *   # show_apps: true              # show app launch row (default true)
 *   # show_color: true             # show color buttons (default false)
 *   # hidden_entity: sensor.x_hidden_buttons   # override the auto-detected id
 *   # hold_entity: sensor.x_hold_buttons       # per-host press-and-hold list
 *   # repeat_entity: sensor.x_repeat_buttons   # per-host hold-to-repeat config
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
 * Remote styles: the layout is drawn from the same style definitions the
 * device's own web page uses (dist/remote-styles.js, generated from the
 * firmware). `remote_style: auto` mirrors whatever style the device has for the
 * active host, which needs the optional `remote_style` text sensor — a
 * dashboard served over https cannot fetch the device's API. A host set to a
 * *custom* style names an id this card has no definition for, since those live
 * in the device's NVS; paste that style's JSON into `remote_style_json` to draw
 * it. show_apps / show_color / show_numpad filter whichever style is drawn.
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
// `node tools/gen-remote-styles.mjs` after changing styles in web_control.cpp.
import {
  RMT_BUILTIN, RMT_VARS, RMT_CSS, sectionHtml, validateTpl,
} from './remote-styles.js?v=1.7.0';

class BleRemoteCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
    // Before _applyHidden: on 'auto' the style comes from a text sensor, so a
    // state update can change the whole layout, and the hidden list has to be
    // applied to the buttons that redraw produced. _renderStyle no-ops when
    // nothing changed, and forces the re-apply itself when something did.
    this._renderStyle();
    this._applyHidden();
    this._applyHoldAndRepeat();
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
      // The other two per-host lists from Host Actions. Absent entity = the
      // card's own defaults: nothing holds, and volume/channel repeat.
      hold_entity: config.hold_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_hold_buttons`,
      repeat_entity: config.repeat_entity ||
        `sensor.${config.device.replace(/-/g, '_')}_repeat_buttons`,
      host_slots: config.host_slots || 0,
      host_names: config.host_names || [],
      active_host_entity: config.active_host_entity || null,
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
    };
    // Forces the next _renderStyle() to redraw even if the resolved id is
    // unchanged — the pasted JSON may have been edited under the same id.
    this._drawnStyleKey = undefined;
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

    const rawHold = read(this._config.hold_entity);
    if (rawHold !== this._lastHold) {
      this._lastHold = rawHold;
      this._holdSet = rawHold ? rawHold.split(',').map(s => s.trim()).filter(Boolean) : [];
      this._endHold();   // whatever is held belonged to the old set
    }

    // "<delay>,<rate>,name,name". Empty means this host was never configured,
    // which is the cue to keep the card's own defaults rather than to repeat
    // nothing — a configured-but-empty host sends just "<delay>,<rate>".
    const rawRepeat = read(this._config.repeat_entity);
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

  // True if this action should repeat while held on this host. With no sensor
  // the card falls back to the four buttons it has always repeated.
  _repeats(action) {
    return this._repeatSet
      ? this._repeatSet.includes(action)
      : ['volume_up', 'volume_down', 'channel_up', 'channel_down'].includes(action);
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

    const hide = raw ? raw.split(',').map(s => s.trim()).filter(Boolean) : [];
    // visibility, not display: a hidden button keeps its slot, so removing OK
    // leaves a hole in the D-pad instead of the arrows sliding into it. An
    // invisible button takes no clicks either, which opacity would not give.
    this.shadowRoot.querySelectorAll('[data-action]').forEach(el => {
      const off = hide.includes(el.dataset.action) || el.dataset.toggledOff === '1';
      el.style.visibility = off ? 'hidden' : '';
    });
    // Holding the shape stops once there is no shape left to hold: a group or
    // section with nothing visible collapses rather than leaving empty slots.
    const empty = el => ![...el.querySelectorAll('[data-action]')]
      .some(b => b.style.visibility !== 'hidden');
    this.shadowRoot.querySelectorAll('.rmt-strip-group, .rmt-rocker-col')
      .forEach(g => { g.style.display = empty(g) ? 'none' : ''; });
    this.shadowRoot.querySelectorAll('.rmt-section')
      .forEach(s => { s.style.display = empty(s) ? 'none' : ''; });
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
        .host-info { text-align: center; min-width: 0; }
        .host-name { font-size: 12px; font-weight: 600; color: var(--primary-text-color, #333); line-height: 1.2; }
        .host-addr { font-size: 12px; font-weight: 400; font-family: monospace; white-space: nowrap; color: var(--secondary-text-color, #888); }

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
        }
      </style>
      <div class="card">
        <div class="zoom">
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
    // Draw the remote. Safe before the first hass update: 'auto' falls back to
    // the default until the sensor arrives, so the card is never blank.
    this._renderStyle();
    this._setupHostSwitcher(shadow);
  }

  // ── Host switcher ────────────────────────────────────────────────
  // Mirrors the keyboard card: prev/next arrows around the active host's name,
  // with its MAC to the left. A switch made here reaches the other cards two
  // ways — instantly if the active-host sensor exists, otherwise on their next
  // poll below — and _applyHidden() repaints this card's buttons for the new host.

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
    // A hidden tab or a switched-away dashboard never delivers pointerup, and a
    // key left down on the host is worse than a press cut short.
    this._onHide = () => { if (document.hidden) this._endHold(); };
    document.addEventListener('visibilitychange', this._onHide);
    window.addEventListener('blur', this._boundEndHold = () => this._endHold());
  }

  disconnectedCallback() {
    clearInterval(this._hostPollInterval);
    this._hostPollInterval = null;
    document.removeEventListener('visibilitychange', this._onHide);
    window.removeEventListener('blur', this._boundEndHold);
    this._endHold();   // the card is going away mid-press
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
        // /hosts carries the per-host style id. It is the fallback route for
        // 'auto' — the sensor is the one that survives an https dashboard.
        this._renderStyle();
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

      this._runAction(action);
      if (!this._repeats(action)) return;
      // The first repeat waits out the configured delay, so a quick tap stays
      // one press. Matches the web remote's timing for this host.
      timer = setTimeout(() => {
        timer = null;
        interval = setInterval(() => this._runAction(action), this._repeatRate);
      }, this._repeatDelay);
    });
    card.addEventListener('pointerup', end);
    card.addEventListener('pointerleave', end);
    card.addEventListener('pointercancel', end);
  }

  // ── Remote style ────────────────────────────────────────────────
  // Resolve, then draw. Order for 'auto': the text sensor (the only route that
  // survives an https dashboard), then the tpl on the /hosts response the card
  // already polls, then the default.
  _resolveStyle() {
    const cfg = this._config;
    if (cfg.remote_style === 'custom') {
      const raw = (cfg.remote_style_json || '').trim();
      if (!raw) return { style: null, error: 'No custom style JSON — paste one in the card options.' };
      let parsed;
      try { parsed = JSON.parse(raw); }
      catch (err) { return { style: null, error: 'Custom style is not valid JSON: ' + err.message }; }
      const why = validateTpl(parsed);
      if (why) return { style: null, error: 'Custom style rejected: ' + why };
      return { style: parsed, error: null };
    }

    let id = cfg.remote_style;
    if (id === 'auto') {
      const ent = this._hass && this._hass.states[cfg.remote_style_entity];
      const fromSensor = ent && typeof ent.state === 'string' &&
        ent.state !== 'unknown' && ent.state !== 'unavailable' ? ent.state.trim() : '';
      // The style the device reports for the active host, else what /hosts said.
      id = fromSensor || this._styleFromHosts() || 'default';
    }
    const style = RMT_BUILTIN.find(t => t.id === id);
    // A host set to a *custom* style names an id this card has no definition
    // for — its JSON lives in the device's NVS. Fall back rather than blank.
    return { style: style || RMT_BUILTIN.find(t => t.id === 'default'), error: null };
  }

  // The per-host style id from the /hosts poll, when that fetch works at all.
  _styleFromHosts() {
    const slots = this._hostSlots;
    if (!Array.isArray(slots)) return '';
    const slot = slots.find(s => s.slot === this._activeSlot);
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
      for (const k in RMT_VARS) {
        if (typeof style.theme[k] === 'string') body.style.setProperty(RMT_VARS[k], style.theme[k]);
      }
    }
    body.innerHTML = style.sections.map(sectionHtml).join('');
    this._applyToggles();
    // Forced: the buttons are new, so whatever this host hides has to be
    // reapplied even though the hidden list itself did not change.
    this._applyHidden(true);
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

  _runAction(action) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_run_action`, { action });
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

const REMOTE_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'zoom', selector: { number: { min: 0.25, max: 3, step: 0.05, mode: 'box' } } },
  // Built from the generated catalogue, so the list is whatever the firmware
  // this card shipped with defines — no second place to keep in step.
  { name: 'remote_style', selector: { select: { mode: 'dropdown', options: [
    { value: 'auto', label: 'Auto (follow the device)' },
    ...RMT_BUILTIN.map(t => ({ value: t.id, label: t.name })),
    { value: 'custom', label: 'Custom (paste JSON below)' },
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
  { name: 'host_slots', selector: { number: { min: 0, max: 10, step: 1, mode: 'box' } } },
  { name: 'host_names', selector: { text: {} } },
  { name: 'active_host_entity', selector: { entity: { domain: 'sensor' } } },
  { name: 'show_mac', selector: { boolean: {} } },
  { name: 'host_url', selector: { text: {} } },
];

const REMOTE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  zoom: 'Zoom (1 = normal, 0.5 = half, 2 = double)',
  remote_style: 'Remote style',
  remote_style_json: 'Custom style JSON (paste from the web page’s Export)',
  remote_style_entity: 'Remote-style sensor (optional)',
  show_numpad: 'Show number pad',
  show_apps: 'Show app launcher row',
  show_color: 'Show colour buttons',
  hidden_entity: 'Hidden-buttons sensor (optional)',
  hold_entity: 'Press-and-hold sensor (optional)',
  repeat_entity: 'Hold-to-repeat sensor (optional)',
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
      zoom: 1,
      remote_style: 'auto',
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
    // The show_* toggles filter whatever style is drawn, and two of them
    // default to off — so choosing a style that has a number pad or colour keys
    // would strip out the very thing that makes it that style. Switching them on
    // for the sections the chosen style actually contains means it renders as
    // designed, while the toggles still do exactly what they say afterwards.
    if (config.remote_style && config.remote_style !== this._config.remote_style) {
      const picked = RMT_BUILTIN.find(t => t.id === config.remote_style);
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
