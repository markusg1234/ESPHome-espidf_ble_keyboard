# Changelog

All notable changes to this project are documented here. This project follows
[semantic versioning](https://semver.org/); releases are tagged `vX.Y.Z` and the
web control page shows the matching version badge.

## Unreleased

### Fixed
- **The web page no longer runs the device short of network connections.** Loading it opens several
  at once, and the pool they come from was sized without counting the page at all — so a refresh
  could use up every one, leaving the device refusing connections and the browser waiting on
  retries until things settled. The component now reserves what the page needs. A value you set
  yourself still takes precedence, and the extra costs 128 bytes of memory.

- **A too-large mouse movement no longer sends the pointer the other way.** Asking for more travel
  or scroll than a single report can carry wrapped around, so a large positive value arrived as a
  negative one and the pointer went backwards. Values are now capped at the maximum instead. The
  web page already stayed inside the limit, so this only affected controlling the device directly.

- **A mistyped pointer-position request now fails instead of doing something else.** The position
  arguments were passed straight through into an action, so one carrying the separator that
  divides a multi-step action tacked an extra step onto the end — a stray character could type
  text or press a key rather than reporting a mistake. Positions must now be numbers, and a
  request that isn't is refused without running anything.

- **One odd character in a macro name no longer breaks the whole buttons list.** A control
  character — the kind a paste or a copied string can carry invisibly — made the page's reply
  unreadable, so the list showed only "Error loading" with nothing to explain it. Such characters
  are now written in a form the page can read, and accented or emoji names still come through
  exactly as typed.

- **Pointer calibration values the device refuses now say so.** Sending a scale outside the
  accepted range, or something that isn't a number at all, was answered with success while the
  value was quietly dropped — and a request carrying one good axis and one bad one applied half of
  it. Such a request is now refused with the reason, and nothing is applied unless all of it is
  valid. Only affects controlling the device directly; the page already checked before sending.

- **Importing a remote style now rejects button names that only looked valid.** A handful of names
  borrowed from JavaScript's own vocabulary slipped past the import check that promises to name
  every unknown button, and the style then drew a dead key with no label and no action behind it.
  Those names are refused at import now, like any other button the firmware doesn't have.

- **Host names containing punctuation now show correctly on the web page's host bar.** A name with
  a `<` in it lost everything from that character on, and one containing an ampersand escape showed
  the decoded character instead of what you typed — the name was being inserted as markup rather
  than as text. Names are now placed as text, the way every other label on the page already was.

- **Raising the web server's URL length limit no longer breaks the build.** One buffer was sized
  with a fixed number that had to match that limit exactly, so changing it failed to compile with
  an error pointing inside the component rather than at the setting that caused it — and raising
  it is the obvious thing to try, since style uploads are chunked only because a request carries
  around 500 characters of URL. The buffer now takes its size from the setting.

- **The web mouse's scroll arrows no longer keep scrolling after you let go.** Pressing an arrow,
  dragging the cursor off it and releasing left the host scrolling until the page was reloaded,
  because the release was delivered to whatever was under the cursor instead. Holding both arrows
  at once on a touchscreen stranded one of them the same way. Only a mouse or pen could trigger
  the first; a finger never could.

- **A second remote button pressed while the first is held no longer leaves one repeating forever.**
  Holding a d-pad key and then reaching for the volume rocker started a second repeat over the top
  of the first, and letting go cleared only the newer one — the older kept firing at the repeat
  rate until the page was reloaded. The most recently pressed button is now the one that repeats.

- **The web page's larger replies no longer churn the device's memory.** A reply was built up one
  piece at a time, reallocating as it grew, and then copied again in full before being sent — tens
  of kilobytes at once for a settings backup, on memory shared with the Bluetooth stack. Replies
  are now sized up front and handed over without the extra copy. Free memory on a busy device was
  measured dipping to about 23 KB, which is what prompted this.

- **A web request that used the wrong method no longer looks like a crash.** Asking one of the
  device's control endpoints for a page — following it from a browser bar, or a script that reads
  where it should write — was answered with an internal-server error, which reads as firmware that
  fell over. It now answers with an ordinary bad-request error that says what the endpoint expects.

## v1.8.0 — 2026-08-12

### Added
- **A paired host's identity key can now be read from Host Actions.** Phones broadcast a random
  address that changes every few minutes, and this key is the only thing that can tie one of those
  back to a known host — what presence detection needs. Take it to Home Assistant or a second ESP32,
  which has to do the detecting, since that job and this component both want to own the Bluetooth
  controller. Shown only when asked for, kept out of backups, and refused to other websites.

- **The web remote can be popped out into a window of its own**, so it stays in reach while you
  scroll the page or work in another app, and pinned back where it was afterwards. The window holds
  the remote and nothing else, sized to it and resized whenever a host switch brings a style of a
  different shape. On a Chromium browser reaching the device over https it can be kept above your
  other windows; elsewhere it opens as an ordinary one.
  The same view can be opened directly at `/ble_keyboard#remote` and bookmarked as a remote-only page.

- **A remote style that draws its own body loses the card from behind it once popped out**, so what
  floats in the window is the shape, shadow and taper it defines rather than a slab inside a slab.
  In the page it keeps its card like the sections around it, and a style that draws no body of its
  own keeps one everywhere.

- **The web remote can now look different on each host.** Pick a style per host slot in the new
  Remote Style panel and the remote re-skins as you switch machines — five shapes alongside the full
  one, from a compact media strip to a full set-top remote with a number pad, colour keys and a
  circular nav ring, in a dark and a light body. Styles are stored on the
  device, so they follow the host rather than the browser. Export one, edit the JSON, and import it
  back to build your own; six fit on the device.

- **More remote buttons, and buttons can carry your own label.** The remote gains Menu, Exit, Guide,
  TV, Voice, Subtitles and a full number pad as real keys, plus sixteen spares that send nothing
  until you give them a per-host override — for app-launcher keys and anything else with no standard
  code worth guessing. A style can now write a button as `["spare1","Netflix"]` to put its own text on the key,
  so an imported layout reads the way the remote it came from does.
- **A remote style can now look like the remote, not just carry its keys.** New circular navigation
  ring and one-piece volume/channel rocker sections, per-button colour and size, inverted light keys,
  and body shaping through gradients, the full border-radius grammar and an optional taper. Enough
  for a style to read as the device it belongs to rather than a generic slab.

- **The Home Assistant remote card draws the same styles as the web page.** Pick one in the card
  editor, or let it follow whatever style the device has for the active host so switching hosts
  re-skins the card too — that needs the new `remote_style` text sensor. A style you made yourself
  can be pasted into the card, where it joins the style list under its own name. The card's layout
  now comes from the firmware's own definitions rather than a second hand-kept copy.

- **Remote keys can now fire Home Assistant actions** — a new action prefix asks HA to run
  one of its own actions over the native API, so a key remapped in Host Actions can drive
  an IR blaster, a script, or a scene, from the HA remote card and the web remote alike.
  Off by default behind a new YAML opt-in, and additionally gated by HA's own per-device
  permission; Host Actions and Macros gain a ready-to-edit preset once enabled.

### Fixed
- **Sharing a device with another Bluetooth component now fails loudly instead of quietly.** Adding
  a tracker or proxy alongside the keyboard used to compile and boot with nothing in the log, then
  starve whichever one lost the race for the radio. The keyboard now checks whether Bluetooth is
  already running, says which component to move to another board, and stops rather than half-working.
- **The web page asks the device for a lot less, so it stops resetting connections.** The position
  finder polled several times a second whatever was on screen; it now stops when its section is
  hidden and slows right down while the map is locked. Any page in the background stops polling
  until it is looked at again, and the browser no longer asks for a favicon the device never had. A
  page and a popped-out remote together now make fewer requests than the page alone used to.
- **Removing a button from the remote no longer moves the others.** The remaining buttons closed the
  gap, so hiding OK pulled the D-pad arrows out of position; a removed button now leaves its place
  empty. A row or section with nothing left visible still collapses.

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
