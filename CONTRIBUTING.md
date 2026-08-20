# Contributing

Maintainer notes for this repository. If you are *using* the component, everything you need is in
[README.md](README.md) — this file is only about publishing changes from it.

For the edit-and-reload loop while working on the Lovelace cards, see
[Developing the Cards](README.md#developing-the-cards) in the README; that part is useful to anyone
customising a card, so it stays there.

## Cutting a release

Once a change is ready, cut a release so HACS users receive it — see [Installing the cards via HACS](README.md#installing-the-cards-via-hacs). Five things move together in the same batch:

1. Every versioned import in [`dist/`](dist/) → the new tag. That means the three in the entry point [`dist/ESPHome-espidf_ble_keyboard.js`](dist/ESPHome-espidf_ble_keyboard.js) *and* the ones cards make of each other, such as `remote-card.js` importing `remote-styles.js` — `grep -rn "?v=" dist/` finds them all.
2. The `webver` badge in `web_control.cpp` → the new tag.
3. A new `## vX.Y.Z` section in [`CHANGELOG.md`](CHANGELOG.md).
4. Any `main`-only markers cleared from [`README.md`](README.md) — `grep -n "^> \*\*Unreleased" README.md`.
5. The web-task stack probe in `web_control.cpp` trimmed to its warning: delete the per-request `ESP_LOGD` line *and* the new-low `ESP_LOGI` line, keeping only the `ESP_LOGW` that fires under 768 bytes. ESPHome's logger defaults to `DEBUG`, so both of the others print for every user — `grep -n "Web task stack" components/espidf_ble_keyboard/web_control.cpp`.

Step 1 is what actually gets the new cards into users' browsers. HACS re-registers the entry point under a fresh `?hacstag=` on every update, so that file always arrives new — but the imports inside it resolve to URLs HACS never varies, and a browser holding a cached card would go on serving it. Versioning the imports makes each release a URL no cache has seen.

To test a release before publishing it, tag a **pre-release** (e.g. `v1.5.0-beta.1`); HACS offers it once **Show beta versions** is ticked in the repository's download dialog.
