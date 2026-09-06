# Contributing

If you are *using* the component, everything you need is in [README.md](README.md). This file is
about changing it: the first half is for anyone sending a pull request, the second is the
maintainer's release process.

## Sending a pull request

Pull requests are welcome. Branch off `main`, and open the request against `main`. Every one is
read and considered, though not every one is merged — this is a single-maintainer project with its
own direction.

**Feature ideas are wanted as issues, not as pull requests.** Say what you want it to do and why —
that part is genuinely useful. I'd rather write the feature myself, so that it fits the design of
everything around it and stays maintainable by one person. Please don't build one and send it in
unannounced; that is the surest way to spend an evening on something I then have to turn down.
Fixes, documentation corrections and small self-contained changes are what a pull request is for.

**Skip the release machinery.** No version bumps — the `?v=` on the imports in [`dist/`](dist/) and
the `webver` badge in `web_page.html` — and no `CHANGELOG.md` entry. Those move together in one batch
when a release is cut, so a PR that touches any of them conflicts with the next one. Whether a change
earns a release is my call, and a fix does not need its own. If it is merged, your commit lands on
`main` under your own name, and the changelog entry written for it credits you.

**Say what you tested on.** For firmware (`components/espidf_ble_keyboard/`) that means a real build
and, for anything touching BLE, a real host — name the operating system, because Windows, Android TV
and iOS each ignore a different part of the HID spec. For the cards (`dist/`) it means a browser and
a dashboard; see [Developing the Cards](README.md#developing-the-cards) for the reload loop.

**README examples have to survive being pasted**, because that is what they are for. A config
example must validate exactly as written, and an entity `name:` cannot contain a `/` — ESPHome
reserves it as a URL path separator, warns today and refuses outright from 2027.7.0.

## Cutting a release

*Maintainer only — a pull request never needs any of this.*

Once a change is ready, cut a release so HACS users receive it — see [Installing the cards via HACS](README.md#installing-the-cards-via-hacs). Five things move together in the same batch:

1. Every versioned import in [`dist/`](dist/) → the new tag. That means the three in the entry point [`dist/ESPHome-espidf_ble_keyboard.js`](dist/ESPHome-espidf_ble_keyboard.js) *and* the ones cards make of each other, such as `remote-card.js` importing `remote-styles.js` — `grep -rn "?v=" dist/` finds them all.
2. The `webver` badge in `web_page.html` → the new tag.
3. A new `## vX.Y.Z` section in [`CHANGELOG.md`](CHANGELOG.md).
4. Any `main`-only markers cleared from [`README.md`](README.md) — `grep -n "^> \*\*Unreleased" README.md`.
5. Nothing chatty left at `DEBUG` that every user would see. ESPHome's logger defaults to `DEBUG`, so a line logged per request prints for everyone, several a second while a page is open — `grep -rn "ESP_LOGD" components/espidf_ble_keyboard/` and check each one is per-event, not per-poll. (The web-task stack probe was the last of these; it is down to its `ESP_LOGW` and needs nothing at release.)

Step 1 is what actually gets the new cards into users' browsers. HACS re-registers the entry point under a fresh `?hacstag=` on every update, so that file always arrives new — but the imports inside it resolve to URLs HACS never varies, and a browser holding a cached card would go on serving it. Versioning the imports makes each release a URL no cache has seen.

To test a release before publishing it, tag a **pre-release** (e.g. `v1.5.0-beta.1`); HACS offers it once **Show beta versions** is ticked in the repository's download dialog.
