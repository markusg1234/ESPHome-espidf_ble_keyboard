# Changelog

All notable changes to this project are documented here. This project follows
[semantic versioning](https://semver.org/); releases are tagged `vX.Y.Z` and the
web control page shows the matching version badge.

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
