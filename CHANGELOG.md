# Changelog

All notable changes to this project are documented here. This project follows
[semantic versioning](https://semver.org/); releases are tagged `vX.Y.Z` and the
web control page shows the matching version badge.

## Unreleased

### Added
- **Remote keys can now fire Home Assistant actions** — a new action prefix asks HA to run
  one of its own actions over the native API, so a key remapped in Host Actions can drive
  an IR blaster, a script, or a scene, from the HA remote card and the web remote alike.
  Off by default behind a new YAML opt-in, and additionally gated by HA's own per-device
  permission; Host Actions and Macros gain a ready-to-edit preset once enabled.

## v1.7.0 — 2026-08-06

### Added
- **A paste bar on the web keyboard and the keyboard card** — in the web page's keyboard
  header, and on its own line above the card's keys. Paste or type any text and send it as
  one piece instead of tapping it out key by key; multi-line text keeps its line breaks. An
  optional auto mode types text the moment it is pasted, and on HTTPS a clipboard button
  sends the clipboard in one tap. The card can hide the bar via a new editor toggle.
- **Press and hold: a key now stays down on the host for as long as the button is held.**
  Every keypress used to arrive as a short tap however long the button was held, so
  push-to-talk was impossible. Set it per physical key with the new hold and release
  automation actions, or per remote button per host in the new Press and hold panel; the
  Media Remote card needs the new `hold_buttons` sensor. `max_key_hold_ms` guards against
  a release that never arrives.
- **The Media Remote card now follows the per-host hold-to-repeat settings**, via a new
  `repeat_buttons` text sensor. It previously repeated volume and channel only, at a fixed
  speed, ignoring whatever Host Actions said — so a host set to repeat the D-pad did so on
  the web remote but not the card.
- **A text sensor for the connected host's Bluetooth address**, so an automation can act on
  which machine is connected rather than just which slot is active. It reports the host's
  stable identity where one was exchanged at pairing, so it keeps working on phones, whose
  connection address rotates every few minutes. Hosts with a fixed address report that
  address unchanged. The web remote's host buttons and the cards show the same address, so
  a phone no longer appears under one address in the web UI and another in the sensor. Each
  host slot remembers its identity the next time that host connects, so an existing pairing
  starts showing the right address on its own — there is no need to pair it again.
- **A bonded host slot now belongs to its host until you forget it.** A different device that
  pairs while that slot is active is disconnected and its bond removed, instead of quietly
  taking the slot over — which mattered most on Android, where pairing has no PIN and nothing
  stopped the last device to pair from claiming the slot. Your own host is matched by identity,
  so it is still let back in after its address rotates or you re-pair it.
- **A `zoom` option on all three cards**, in the visual editor's Config tab (0.25–3, default
  1). It scales the whole card — buttons, text and spacing together — so you can shrink one
  to fit a smaller space or enlarge it for a wall tablet. The card's height follows the zoom,
  so `zoom: 0.55` brings the full media remote down to about 8 rows, which is as short as
  HA's height slider goes. Everything scales by the same factor in both directions, so the
  buttons keep their shape at any zoom, and a zoomed-in card scrolls rather than squashing.
- **All three Lovelace cards support resizing in sections dashboards.** They keep their
  natural size by default, as before — but the Layout tab now offers sensible width and
  height ranges (1–12 columns, 1–8 rows) instead of warning that the card can't be resized.
  Cards keep their proportions at whatever size you pick and scroll if the card is smaller
  than they are; use `zoom` to change how big the controls actually are.
- **The mouse and keyboard cards scroll as one piece when narrowed.** Previously only the
  title row kept its full width while the keys and the touchpad shrank to whatever was
  visible, so scrolling sideways dragged a squashed keyboard out of view and clipped key
  labels. Every part of the card now shares the same width and scrolls together, at full
  size. To actually make a card smaller rather than scroll it, lower its zoom.

### Fixed
- **The hidden-buttons sensor was empty until the first host switch.** It published before
  the saved lists were read from flash and never again, so a card starting up was told the
  host hid nothing. The new hold and repeat sensors publish on the same path.

### Changed
- **The media remote's buttons stay centred in a 460px column on wide cards**, instead of
  spreading out with the power button stranded at the far edge. Only visible in panel view
  or a wide masonry column; sections cap at 500px, so they look unchanged.
- **Forget Host moved into the Host Actions card**, on the same row as the host picker, and
  it now forgets **the slot the picker shows** rather than always the active host — so a host
  that isn't currently connected can be forgotten too. The host bar at the top is host buttons
  only. Still two taps (`Confirm?`, disarming after three seconds or when you change slot).
  This also puts it on single-slot devices, which never had it: the host bar hides itself
  below two slots, while the Host Actions card is always shown.
- **Both editors' save buttons now read `Save`** — the Macros button was `+ Add` (becoming
  `Save` mid-edit) and the Host Actions one was `+ Set`. The Macros label no longer changes
  to signal add-versus-update; the populated Name and Action fields are the cue instead.
- **The Host Actions replacement box is a 3-line textarea**, matching the Macros action box,
  so a long chained action is readable without scrolling through a single-line field. Both
  action boxes are also wider than the name box beside them, and the two preset dropdowns are
  now a matched width.
- **The host bar fills the card width**, five hosts per row, dividing the space evenly instead
  of leaving a lopsided gap at the right; a short final row stays left-aligned under the one
  above. The toolbar's section toggles likewise stretch to reach the right-hand edge.

## v1.6.0 — 2026-07-27

### Added
- **Hold to repeat on the web remote, configurable per host** — holding a button makes it
  fire repeatedly, the way a real remote ramps the volume or scrolls a menu; a quick tap
  still sends exactly one press. The D-pad, volume, channel, rewind and fast forward
  repeat by default. A **Hold to repeat** panel in the Host Actions card sets which
  buttons repeat, how long a button must be held first (100–2000 ms, default 400) and how
  fast it repeats (50–2000 ms, default 180) — **per host**, since a TV wants a fast volume
  ramp while a PC may want only the D-pad. Stored on the device rather than in the
  browser, so the setting follows the host instead of the phone that set it, and included
  in Backup & Restore. New `/api/ble_keyboard/repeat` and `/repeat_set` endpoints.

  The 50 ms floor on the repeat rate is deliberate: the device drops an identical press
  arriving within 30 ms of the last one, so a faster repeat would silently lose events.

### Fixed
- **Holding a button on the web remote did nothing.** The repeat had been dropped in an
  earlier reinstatement of tap-fires-on-release, leaving the `data-repeat` markup with no
  code behind it while the README still advertised the feature.
- **README** no longer credits the Home Assistant card's Mute button with hold-to-repeat;
  that card repeats volume and channel only.

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
  alternate:consumer:0x30 | delay:1000 | ok || repeat:3:press_button:samsung_43_m70f_wol
  ```

  Set that as `remote_power` in Host Actions and the remote's power button sleeps the
  monitor over BLE, then wakes it over Wake-on-LAN. Editable with no reflash, and its
  preset **wraps** what's already in the box rather than appending. `repeat:` composes,
  which Wake-on-LAN usually wants since magic packets are unacknowledged UDP. Note this is
  *assumed* state — HID is one-way, so the device can't detect the monitor being switched
  off by other means; the README shows a template-button recipe for when it must be real.
- **`macro:<name>` action** — run a stored web macro from any action string, so macros can
  call each other and an `alternate:` branch can be a whole macro. Names must now be unique
  and cannot contain `|`, since that would split the reference.
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
- **Host Actions now reference a macro instead of copying it.** Picking a macro from the
  preset dropdown used to paste a snapshot of its text, so editing the macro afterwards
  left the override running the old version with nothing showing they'd diverged. It now
  inserts `macro:<name>`, which follows the macro. Existing overrides keep their copied
  text and work unchanged — re-pick the macro to convert one into a reference. Overrides
  pointing at a macro that no longer exists are flagged with a ⚠ naming it, updated live
  as macros are renamed or deleted.
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
