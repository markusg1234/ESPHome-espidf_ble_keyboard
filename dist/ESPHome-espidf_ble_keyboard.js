/**
 * Entry point for the ESPHome BLE Keyboard dashboard cards, as Home Assistant's
 * Marketplace (HACS before 2026.11) installs them.
 *
 * The Marketplace matches the .js file named after the repository, downloads
 * every file beside it in dist/, but registers only this one as a dashboard
 * resource — so it has to pull the others in. The relative imports resolve inside
 * /local/community/ESPHome-espidf_ble_keyboard/ (HACS: /hacsfiles/…), where the
 * files are installed.
 *
 * Each card registers itself on import (customElements.define + customCards),
 * so importing for side effects is all that's needed — no build step, and the
 * cards stay separate readable files.
 *
 * The ?v= on each import is the release tag, and it must be bumped on every
 * release (same batch as the webver badge in web_page.html). The Marketplace
 * re-registers this file under a new ?v= when it updates (HACS: ?hacstag=), so
 * this file always arrives fresh — but the imports below resolve to fixed URLs
 * that it never varies, so without a version query a browser holding a cached
 * copy of a card would keep serving it after the update. A changed URL cannot be
 * served from any cache.
 *
 * Installing by hand instead? You can ignore this file and add whichever cards
 * you want as individual resources — see the README.
 */
import './keyboard-card.js?v=1.20.0-dev';
import './mouse-card.js?v=1.20.0-dev';
import './remote-card.js?v=1.20.0-dev';
