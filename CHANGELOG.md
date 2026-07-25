# Changelog

All notable changes to this project are documented here. This project follows
[semantic versioning](https://semver.org/); releases are tagged `vX.Y.Z` and the
web control page shows the matching version badge.

## v1.5.0 — 2026-07-25

### Added
- **Host switcher on the mouse and remote cards** — the prev/next host selector that
  the keyboard card had is now in all three cards' headers, via the same `host_slots`,
  `host_names` and `active_host_entity` options.
- **Cards stay in sync with each other** — switching the host on one card updates the
  other two, as do switches made from the web control page, a physical button, or a YAML
  action. Works out of the box via the cards' 30-second device poll; adding the optional
  `active_host` sensor makes it instant.
- **Other ESPHome buttons appear on the web control page** — every non-internal `button:`
  in your config is listed automatically, so the page can reach things BLE can't. The
  motivating case is power: a monitor can be slept with `consumer:0x30` but only woken
  with Wake-on-LAN. They work as `press_button:<object_id>` actions too, so they can be
  used in macros, per-host overrides, and the REST API. Keyed by object id rather than
  position, so adding buttons never repoints a saved macro.
- **`hide_buttons` option** to keep destructive buttons (`factory_reset`, `restart`) off
  the page, which has no authentication. Hidden buttons are also refused if their action
  is typed by hand. **`expose_buttons: false`** turns the listing off entirely.
- **`alternate:` action** — runs one **branch** per press instead of everything in sequence,
  so a single button can toggle. Branches split on `||`, while a single `|` keeps meaning
  "next step", so each branch can be a whole sequence — necessary for displays that need a
  confirmation step to power off:

  ```
  alternate:consumer:0x30 | delay:500 | ok || repeat:3:press_button:samsung_43_m70f_wol
  ```

  Set that as `remote_power` in Host Actions and the remote's power button sleeps the
  monitor over BLE, then wakes it over Wake-on-LAN. Editable with no reflash, and its
  preset **wraps** what's already in the box rather than appending. `repeat:` composes,
  which Wake-on-LAN usually wants since magic packets are unacknowledged UDP. Note this is
  *assumed* state — HID is one-way, so the device can't detect the monitor being switched
  off by other means; the README shows a template-button recipe for when it must be real.
- **`show_mac` option** on all three cards to show or hide the active host's MAC address.
- **`host_url` option** on all three cards to point a card at the ESP32 explicitly when
  auto-detection can't find it.

### Fixed
- **The keyboard card's MAC address never displayed.** It looked for the device's URL in
  a `text_sensor.*` entity, but Home Assistant has no such domain — ESPHome text sensors
  become `sensor.*` — so the lookup always failed and the line stayed blank. The cards now
  take the address from the device's `configuration_url` in Home Assistant's device
  registry (or `host_url`), and the MAC is drawn to the left of the selector.
- **Host switching failed for devices with a `-` in their name.** The card built the
  service name from the raw device name instead of the underscored slug.
- **The host poller leaked a timer per dashboard view switch** and never stopped. It is
  now cleared on disconnect, re-armed on reconnect, and runs every 30 s instead of 5 s —
  the endpoint returns every slot at once, so a host switch repaints from cache.
- **The `/api/ble_keyboard/*` endpoints sent `Access-Control-Allow-Origin` twice.** ESPHome's
  `web_server` already adds one globally, and the component added a second, so browsers saw
  `*, *` and refused the request — stricter than sending no CORS header at all. Any
  cross-origin read of the API failed, which is why the cards could never fetch host MACs.
  The component no longer sets the header and relies on ESPHome's.
- **HACS updates could leave stale cards in the browser.** The HACS entry point imported
  the three card files at fixed URLs, so an update re-fetched the entry point (HACS gives
  it a new `?hacstag=`) while the browser could keep serving cached copies of the cards
  themselves. The imports now carry the release version, so every release produces URLs
  no cache has seen.

### Changed
- **Your existing buttons will now show on the web control page.** Button listing is on by
  default, so after this update any `restart`, `safe_mode` or `factory_reset` button in
  your config appears on `/ble_keyboard` alongside the keyboard's own. Add them to
  `hide_buttons`, mark them `internal: true`, or set `expose_buttons: false` if you'd
  rather they didn't.

### Notes
- **Reflash required for the MAC display and the button listing.** Both are firmware-side.
  The host switcher itself works after a card update alone; the MAC address and the new
  buttons only appear once the device is reflashed.
- The MAC is read straight from the ESP32 over HTTP. If Home Assistant is served over
  HTTPS the browser blocks that as mixed content and the MAC line hides itself; the
  switcher still works.

**Full changes:** `v1.4.0…v1.5.0`

## v1.4.0 — 2026-07-25

### Added
- **YAML buttons as presets** — your `espidf_ble_keyboard` buttons now appear under a
  **Buttons** group in the web UI's preset dropdowns (Macros *and* Host Actions). Build a
  macro or per-host override by picking an existing button instead of retyping its action.
- **Macros as Host Action presets** — saved macros appear under a **Macros** group in the
  Host Actions preset dropdown, so a per-host override can reuse a macro's actions. (The
  Macros editor's own dropdown intentionally does not list macros.)

### Changed
- Internal refactor of the component for readability and maintainability (no behaviour change).
- Updated the Home Assistant remote card screenshot in the docs.

**Full changes:** `v1.3.0…v1.4.0`
