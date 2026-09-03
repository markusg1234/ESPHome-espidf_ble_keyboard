#include "esphome/core/defines.h"
#ifdef USE_BLE_KEYBOARD_WEB_CONTROL

#include "web_control.h"
#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <span>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *const TAG = "ble_kb_web";

// ── Embedded HTML/JS page ───────────────────
// The page itself lives in web_page.html, next to this file. __init__.py reads
// it at codegen, gzips it, and emits the bytes as a progmem array that setup()
// hands to set_web_page() — so the firmware carries only the compressed copy.
// It was 243,148 bytes of flash uncompressed, 15% of the whole image; gzipped it
// is about 73 KB — the exact figure moves a little with the zlib the build host's
// Python carries. Edit the .html, not a string literal in here.


// JSON-escape a string (handles \n, \r, \t, \, ")
static std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        // Everything else below 0x20 needs escaping too: a raw control byte
        // inside a JSON string is invalid, so one stray character in a macro
        // name made the page's JSON.parse throw and the whole buttons list fall
        // back to "Error loading" with nothing to say why.
        //
        // Compared as unsigned deliberately. char is signed here, so every
        // continuation byte of a UTF-8 sequence is negative, and a plain
        // `c < 0x20` would escape those too — mangling every name that isn't
        // pure ASCII. Bytes at 0x80 and above are valid inside a JSON string
        // and pass through untouched.
        if (static_cast<unsigned char>(c) < 0x20) {
          char esc[7];
          snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned char>(c));
          out += esc;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

// Clamp to what a HID relative report can carry, instead of letting the cast
// wrap. ?x=200 used to arrive as -56 and move the pointer the opposite way,
// which reads as the device ignoring you rather than as a rejected value. The
// page clamps before sending, so only direct API callers ever saw it.
// A 0-255 query argument, or -1 for missing, empty, non-numeric or out of range.
// Out of line and one argument at a time. Measured with -fstack-usage this did
// not shrink handleRequest'''s frame — GCC already reuses slots across the
// endpoint branches — so it is here for readability and because it is reused,
// not as a stack saving. The frame is 688 bytes either way.
__attribute__((noinline)) static int parse_byte_arg(AsyncWebServerRequest *request, const char *name,
                                                    int missing) {
  if (!request->hasArg(name))
    return missing;
  std::string v = request->arg(name);
  if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos)
    return -1;
  long n = atol(v.c_str());
  return (n >= 0 && n <= 0xFF) ? (int) n : -1;
}

static int8_t clamp_i8(int v) {
  if (v < -128) return -128;
  if (v > 127) return 127;
  return (int8_t) v;
}

// Is this request addressed to *this device*, or to a name the sender chose?
//
// Sec-Fetch-Site below cannot see a DNS rebinding attack. The attacker serves a
// page from their own domain, then re-points that domain at this device's
// address; the browser fetches again, and because the origin string has not
// changed it calls the request same-origin — which, as far as it knows, it is.
// The one thing still carrying the attacker's name is Host, because the browser
// fills it in from the URL it was handed.
//
// So an address literal or an mDNS name means the device was addressed
// directly, and anything else has to be named in the config. `web_host_check:
// false` turns the test off for setups this cannot anticipate — the obvious one
// being a reverse proxy that fronts the device under a real domain name.
static bool host_ok(AsyncWebServerRequest *request, EspidfBleKeyboard *kb) {
  if (!kb->web_host_check())
    return true;
  auto header = request->get_header("Host");
  // No Host at all is HTTP/1.0 or something hand-made, not a browser being
  // steered — the same reasoning that lets a missing Sec-Fetch-Site through.
  if (!header.has_value())
    return true;
  std::string name = header.value();
  for (auto &c : name)
    c = (char) tolower((unsigned char) c);  // host names are case-insensitive
  // Drop the port. An IPv6 literal is bracketed and full of colons, so only a
  // colon after the closing bracket separates a port.
  size_t bracket = name.rfind(']');
  size_t colon = name.rfind(':');
  if (colon != std::string::npos && (bracket == std::string::npos || colon > bracket))
    name.erase(colon);
  if (name.empty())
    return true;
  // A trailing dot is the fully-qualified spelling of the same name. Browsers
  // usually strip it before sending, but not all of them do, and someone who
  // types one should not be turned away.
  if (name.back() == '.')
    name.pop_back();
  if (name.empty())
    return true;
  if (name.front() == '[')
    return true;  // IPv6 literal
  // An IPv4 literal is digits and dots and nothing else, and a host name can
  // never be — so the character set settles it without parsing the address.
  if (name.find_first_not_of("0123456789.") == std::string::npos)
    return true;
  // No dot at all is not a name anyone can own, so it cannot be the one a
  // rebinding attack arrives under — every registered domain has a dot in it.
  // It is also how the device is reached on a network whose DHCP server
  // publishes short host names, which is common enough to be worth allowing.
  if (name.find('.') == std::string::npos)
    return true;
  if (name.size() >= 6 && name.compare(name.size() - 6, 6, ".local") == 0)
    return true;
  for (const auto &allowed : kb->web_allowed_hosts())
    if (name == allowed)
      return true;
  // Worth a line in the log: to the person running a proxy this is a refusal
  // with no visible cause, and the name they need to add is the one here.
  //
  // Once per name, not once per request. A page loaded under a refused name
  // still sends — the mouse pad alone posts on every movement event — so
  // warning each time would bury the one line that explains it under hundreds
  // saying the same thing. Same reasoning as the stack probe below.
  static std::string warned;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  if (warned != name) {
    warned = name;
    ESP_LOGW(TAG, "Refused a request addressed to '%s' — add it to web_allowed_hosts, "
                  "or set web_host_check: false", name.c_str());
  }
  return false;
}

// Is this request the device's own page, rather than one some other site made
// the browser send?
//
// Every state-changing endpoint here is a *simple* POST — query-string only, no
// body, no custom headers — so a browser sends it cross-origin with no preflight
// to ask permission first. CORS then blocks the attacker from *reading* the
// reply, which is no comfort at all when the request itself is what types on the
// paired host. Any page in any tab could drive this keyboard.
//
// Sec-Fetch-Site is set by the browser itself and cannot be forged by page
// script, which is what makes it worth checking. A request carrying none is not
// a browser being turned against its owner — curl, a script, a Home Assistant
// automation — and all of those could already reach every endpoint here, so they
// keep working exactly as documented.
//
// The other way round the same-origin claim can be false is rebinding, which is
// what host_ok() above covers, and being framed — an iframe's requests really
// are same-origin — which the page response answers with X-Frame-Options.
//
// This is not access control. It closes the "a web page turns your browser
// against you" route and nothing else; `web_server:` auth is still what protects
// the device on a network you do not trust.
static bool same_origin_ok(AsyncWebServerRequest *request, EspidfBleKeyboard *kb) {
  if (!host_ok(request, kb))
    return false;
  auto site = request->get_header("Sec-Fetch-Site");
  return !site.has_value() || site.value() == "same-origin" || site.value() == "none";
}

// ── Stack headroom probe ───────────────────────────────────────────
// Everything in the handler below runs on the web server's task, including
// execute_action(), which the press endpoint calls inline — and an action can
// recurse through repeat:, alternate: and '|' chains before it reaches the HID
// layer. That task gets ESP-IDF's default httpd stack plus 256 bytes, which is
// not much to spend on a chain like that while boot-time init is also running.
//
// An overflow there does not announce itself: it writes past the end of the
// stack and the eventual panic reports an impossible exccause with no faulting
// address, which is what a crash record from 2026-08-21 looked like. Warning as
// the low-water mark falls turns the next one into a number seen *before* the
// crash rather than a backtrace that can't be read after it.
//
// Measured on the device: ~860 bytes free across the ordinary endpoints, so the
// 768-byte warning is close enough to the real figure to mean something. If you
// need the per-request numbers back while chasing something, log `headroom`
// unconditionally in the destructor below.
//
// FreeRTOS keeps the minimum itself — in bytes on ESP-IDF, unlike stock
// FreeRTOS, which reports words.
//
// Only the warning is left. It used to log a line per request at DEBUG, and a
// new low-water mark at INFO — scaffolding for the stack hunt that answered its
// question, and which ESPHome's default DEBUG level printed for every user: a
// line for every poll of /status and /hosts, several a second with a page open.
// The measurement still runs on every request, so the warning below is as much
// of a tripwire as it ever was.
class StackHeadroomProbe {
 public:
  explicit StackHeadroomProbe(const char *url) : url_(url) {}
  ~StackHeadroomProbe() {
    // Only a new low, not every request that happens to be under the margin.
    // Once the headroom is low it is low on *every* request, so warning each
    // time buries the log in a repeat of the same fact — which is exactly what
    // the per-request DEBUG line used to do, and the reason it was removed.
    // A falling number is the news; the same number again is not.
    static UBaseType_t lowest = static_cast<UBaseType_t>(-1);  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    UBaseType_t headroom = uxTaskGetStackHighWaterMark(nullptr);
    if (headroom >= lowest)
      return;
    lowest = headroom;
    // Below this, one deeper call chain could run off the end. Nothing else
    // warns before an overflow corrupts memory silently, so it is worth seeing
    // at the default log level rather than only when someone raises it.
    if (headroom < 768) {
      ESP_LOGW(TAG, "Web task stack down to %u B free (%s), heap %u B", (unsigned) headroom, url_,
               (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
  }

 protected:
  const char *url_;
};

// ── Internal handler class ─────────────────────────────────────────
// Inherits from the platform-specific AsyncWebHandler via web_server_base

class BleKbWebHandler : public AsyncWebHandler {
 public:
  BleKbWebHandler(EspidfBleKeyboard *kb) : kb_(kb) {}

  // Deliberately not inlined. The buffer below is CONFIG_HTTPD_MAX_URI_LEN+1 —
  // 513 bytes by default — and inlined into handleRequest it would sit in that
  // function's frame for the whole request, on top of whatever the endpoint
  // itself needs, instead of being given back the moment the URL has been
  // copied out. The web task gets 4352 bytes in total (ESP-IDF's 4096 plus the
  // 256 ESPHome adds, neither of which this component can change), so half a
  // kilobyte is a large share of what there is.
  __attribute__((noinline)) static std::string get_url(AsyncWebServerRequest *request) {
    // Sized from the web server's own constant rather than the 513 it happens to
    // equal by default. url_to() takes a fixed-extent span, and those do not
    // convert across a size mismatch, so hardcoding the number turned any config
    // that set CONFIG_HTTPD_MAX_URI_LEN into a compile error *in this file* —
    // which is not one the person who changed the option can edit. Raising that
    // limit is a natural thing to try, too: style uploads are chunked precisely
    // because a request carries only ~512 bytes of URL.
    //
    // A raw array converts to the span implicitly, which is how web_server_idf.h
    // calls it itself.
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    auto ref = request->url_to(buf);
    return std::string(ref.begin(), ref.end());
  }

  bool canHandle(AsyncWebServerRequest *request) const override {
    std::string url = get_url(request);
    return url == "/ble_keyboard" ||
           url.rfind("/api/ble_keyboard/", 0) == 0;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
      std::string url = get_url(request);
      // Declared after url so it is destroyed first, while url is still alive.
      // Its destructor is the measurement, so it covers every return below —
      // including the early one that serves the page.
      StackHeadroomProbe stack_probe(url.c_str());

      // Do NOT add Access-Control-Allow-Origin here. ESPHome's web_server (which
      // web_control requires) already installs one globally via DefaultHeaders,
      // so setting it again emits the header twice — and a browser rejects
      // "*, *" outright, which is worse than having no CORS at all. That broke
      // cross-origin reads of /hosts from the Home Assistant cards.
      // Takes the body by const reference, not as a char*. beginResponse() only
      // offers a std::string overload, so a char* was built into a temporary
      // string first — a second full copy of every response alongside the one
      // the caller already holds, which on the 10-15 KB /backup document was
      // tens of kilobytes on a heap that also carries the BLE stack. Measured
      // on device: free heap dips to ~23 KB, so this is worth the const ref.
      // One copy still remains, inside AsyncWebServerResponseContent, whose
      // constructor takes its parameter by value; removing that needs internals
      // this file deliberately keeps out of.
      //
      // Note the body is now sent as sized data rather than up to the first NUL.
      // Only json_escape's unescaped control bytes could put one there, and that
      // body is malformed JSON either way.
      auto send_response = [request](int code, const char* type, const std::string &content) {
        AsyncWebServerResponse* response = request->beginResponse(code, type, content);
        response->addHeader("Connection", "close");
        request->send(response);
      };

    // Serve the page — use Progmem response to avoid heap-copying the large HTML
    if (url == "/ble_keyboard") {
      // The bytes are gzip, compressed at codegen from web_page.html and handed
      // over by set_web_page(). They are sent straight out of flash: the Progmem
      // response holds a pointer, so nothing is copied to the heap, and the
      // browser gets 73 KB instead of 243 KB. Every browser accepts gzip, and
      // ESPHome's own web UI serves its index the same way — but a client that
      // asks for the page by hand has to say so, e.g. curl --compressed.
      AsyncWebServerResponse* response = request->beginResponse(
          200, "text/html", kb_->web_page(), kb_->web_page_size());
      response->addHeader("Content-Encoding", "gzip");
      response->addHeader("Connection", "close");
      // Being framed is the third way a request can be same-origin without
      // being the user's doing, alongside the two host_ok() and same_origin_ok()
      // cover. Inside someone else's iframe this page is still on its own
      // origin, so its requests pass every check above — and a transparent
      // overlay turns one click on their site into a real keystroke on the
      // paired host. Nothing in this project frames the page: the pop-out remote
      // uses window.open and document picture-in-picture, and the Home Assistant
      // cards are custom elements.
      //
      // SAMEORIGIN would not be a softer setting, it would be the same one with
      // a different failure: a dashboard embedding this page is on its own
      // origin too, so it is refused either way. Hence an option rather than a
      // weaker header — someone deliberately framing the page in a dashboard
      // they run can say so, and accept what that opens.
      if (!kb_->web_allow_framing())
        response->addHeader("X-Frame-Options", "DENY");
      // Force the browser to fetch the page on every load so a firmware flash
      // always brings UI changes (new layouts, fixes, etc.) without needing a
      // manual hard-reload.
      //
      // no-store rather than no-cache, and the difference matters once
      // `web_server:` has auth. no-cache still lets the page be *stored*, and a
      // restored tab can come back from that store without a navigation at all —
      // measured on a phone, where a whole page-init sequence of API calls
      // arrived with no request for this document anywhere before it. With no
      // navigation there is no document-level authentication, so the browser
      // starts with no credential and challenges every one of those API calls
      // separately, stacking a login prompt for each. What that looks like is a
      // prompt that will not go away.
      //
      // no-store costs nothing here: this response carries no ETag or
      // Last-Modified, so a revalidation was always a full re-download anyway.
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
      return;
    }

    std::string path = url.substr(strlen("/api/ble_keyboard/"));

    // GET-only endpoints (read state)
    if (path == "status") {
      std::string json = "{\"connected\":";
      json.reserve(512);  // one allocation instead of the five doublings this would take
      json += kb_->is_connected() ? "true" : "false";
      json += ",\"paired\":";
      json += kb_->is_paired() ? "true" : "false";
      json += ",\"ha_action\":";
      json += kb_->ha_action_enabled() ? "true" : "false";
      // Escaped like every other endpoint's strings. ESPHome restricts device
      // names, so this is consistency rather than a live bug — but a /status
      // that cannot be parsed takes the whole page down with it.
      json += ",\"device_name\":\"";
      json += json_escape(kb_->device_name());
      json += "\",\"layout\":\"";
      json += json_escape(kb_->active_layout_id());
      json += "\",\"layouts\":[";
      for (size_t i = 0; i < layout_count(); i++) {
        const KeyboardLayout *lay = layout_at(i);
        if (lay == nullptr) continue;
        if (i > 0) json += ",";
        json += "{\"id\":\"";
        json += json_escape(lay->id);
        json += "\",\"name\":\"";
        json += json_escape(lay->display_name);
        json += "\"}";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "buttons") {
      std::string json = "[";
      json.reserve(512);
      bool first = true;
      // YAML-defined buttons (read-only)
      const auto &btns = kb_->get_buttons();
      for (size_t i = 0; i < btns.size(); i++) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(btns[i].name);
        json += "\",\"action\":\"";
        json += json_escape(btns[i].action);
        json += "\",\"editable\":false}";
      }
      // Buttons from other ESPHome platforms (wake_on_lan, template, …).
      // Read-only like the YAML ones — the page can press them but not edit.
      for (const auto &ext : kb_->get_external_buttons()) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(ext.name);
        json += "\",\"action\":\"";
        json += json_escape(ext.action);
        json += "\",\"editable\":false}";
      }
      // User-defined macros (editable)
      const auto &macros = kb_->get_macros();
      for (size_t i = 0; i < macros.size(); i++) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(macros[i].name);
        json += "\",\"action\":\"";
        json += json_escape(macros[i].action);
        json += "\",\"editable\":true,\"index\":";
        json += std::to_string(i);
        json += "}";
      }
      json += "]";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "mouse_config") {
      char buf[128];
      snprintf(buf, sizeof(buf),
               "{\"sensitivity\":%.2f,\"acceleration\":%.2f,\"max_speed\":%.2f,\"scroll_sensitivity\":%.2f}",
               kb_->mouse_sensitivity(), kb_->mouse_accel(),
               kb_->mouse_max_speed(), kb_->scroll_sensitivity());
      send_response(200, "application/json", buf);
      return;
    }

    if (path == "screen") {
      // Absolute-pointer geometry for the web Position Finder.
      char gsxbuf[16], gsybuf[16];
      snprintf(gsxbuf, sizeof(gsxbuf), "%.4f", kb_->mouse_goto_scale_x());
      snprintf(gsybuf, sizeof(gsybuf), "%.4f", kb_->mouse_goto_scale_y());
      std::string json = "{\"w\":" + std::to_string(kb_->screen_width()) +
                         ",\"h\":" + std::to_string(kb_->screen_height()) +
                         ",\"ox\":" + std::to_string(kb_->primary_origin_x()) +
                         ",\"oy\":" + std::to_string(kb_->primary_origin_y()) +
                         ",\"gsx\":" + gsxbuf + ",\"gsy\":" + gsybuf + ",\"mon\":[";
      const auto &mons = kb_->get_monitors();
      for (size_t i = 0; i < mons.size(); i++) {
        if (i) json += ",";
        json += "{\"x\":" + std::to_string(mons[i].x) +
                ",\"y\":" + std::to_string(mons[i].y) +
                ",\"w\":" + std::to_string(mons[i].width) +
                ",\"h\":" + std::to_string(mons[i].height) +
                ",\"p\":" + (mons[i].primary ? "1" : "0") + "}";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "goto_last") {
      // Last mouse_goto target (Windows coords) so the Finder can mark where the
      // cursor was last sent, from any source. Lightweight — safe to poll.
      std::string json = "{\"x\":" + std::to_string(kb_->last_goto_x()) +
                         ",\"y\":" + std::to_string(kb_->last_goto_y()) + "}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "hosts") {
      // Build slot-to-name map from registered switch_host buttons
      std::map<uint8_t, std::string> slot_names;
      for (const auto &btn : kb_->get_buttons()) {
        if (btn.action.find("switch_host:") == 0) {
          int slot = -1;
          if (sscanf(btn.action.c_str(), "switch_host:%i", &slot) == 1 && slot >= 0) {
            slot_names[(uint8_t) slot] = btn.name;
          }
        }
      }
      std::string json = "{\"active\":";
      json.reserve(512);
      json += std::to_string(kb_->active_host_slot());
      json += ",\"slots\":[";
      for (uint8_t i = 0; i < kb_->host_slots(); i++) {
        if (i > 0) json += ",";
        const auto &h = kb_->get_host_slot(i);
        json += "{\"slot\":";
        json += std::to_string(i);
        json += ",\"occupied\":";
        json += h.occupied ? "true" : "false";
        if (h.occupied) {
          char addr_str[18];
          format_bd_addr(h.addr, addr_str);
          json += ",\"addr\":\"";
          json += addr_str;
          json += "\"";
          // The stable identity, which the UI shows in preference to `addr` —
          // that is only what the host connected with, and a phone rotates it.
          // Remembered on the slot once seen; the live lookup is the fallback
          // for a host that has not connected since this was added.
          esp_bd_addr_t identity;
          bool have_identity = h.has_identity;
          if (have_identity) {
            memcpy(identity, h.identity, sizeof(esp_bd_addr_t));
          } else {
            have_identity = kb_->peer_identity_addr(h.addr, identity);
          }
          if (have_identity) {
            char id_str[18];
            format_bd_addr(identity, id_str);
            json += ",\"identity\":\"";
            json += id_str;
            json += "\"";
          }
        }
        auto it = slot_names.find(i);
        if (it != slot_names.end()) {
          json += ",\"name\":\"";
          json += json_escape(it->second);  // button names are user-supplied
          json += "\"";
        }
        // Which remote style this host is drawn in. Rides along here rather than
        // on an endpoint of its own so the page's existing 5s poll of this
        // response is what re-skins the remote after a host switch.
        const std::string &style = kb_->get_remote_style(i);
        if (!style.empty()) {
          json += ",\"tpl\":\"";
          json += style;  // validated to [a-z0-9_] on the way in
          json += "\"";
        }
        json += "}";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "irk") {
      // A host's Identity Resolving Key, for feeding to whatever does presence
      // detection: Home Assistant's private BLE device tracking, or a second ESP32
      // running esp32_ble_tracker. This device cannot do that job itself —
      // esp32_ble_tracker pulls in esp32_ble, which initialises the BLE controller,
      // and so does this component. Only one of the two can own it.
      //
      // Unlike everything else this handler serves, the answer is a secret: it
      // de-anonymises that phone's rotating address for as long as the bond lives.
      // Hence its own endpoint rather than a field on /hosts, which is polled every
      // five seconds and read cross-origin by the Home Assistant cards.
      //
      // And hence the check below. web_server installs Access-Control-Allow-Origin:*
      // globally, so without it any page the user happens to have open could read
      // the key straight off the device. Same test the POST gate uses — see
      // same_origin_ok().
      if (!same_origin_ok(request, kb_)) {
        send_response(400, "text/plain", "Refused: read this from the device's own page");
        return;
      }
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &h = kb_->get_host_slot((uint8_t) slot);
      std::string json = "{\"slot\":" + std::to_string(slot) + ",\"irk\":";
      uint8_t irk[16];
      // Same fallback order as /hosts: the address the slot stores first, then the
      // identity it remembered, so a slot matched up either way still resolves.
      if (h.occupied && (kb_->peer_irk(h.addr, irk) ||
                         (h.has_identity && kb_->peer_irk(h.identity, irk)))) {
        char hex[33];
        for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", irk[i]);
        json += "\"";
        json += hex;
        json += "\"";
      } else {
        // null, not an error: an empty slot and a host that distributed no ID key
        // are both ordinary states the page explains rather than fails on.
        json += "null";
      }
      json += "}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "overrides") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      // NVS overrides first; a YAML entry of the same name is shadowed and omitted,
      // so the list shows exactly what would run.
      const auto &nvs = kb_->get_nvs_overrides((uint8_t) slot);
      const auto &yaml = kb_->get_yaml_overrides((uint8_t) slot);
      std::string json = "{\"slot\":";
      json.reserve(512);
      json += std::to_string(slot);
      json += ",\"active\":";
      json += std::to_string(kb_->active_host_slot());
      json += ",\"items\":[";
      bool first = true;
      for (const auto &o : nvs) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + json_escape(o.name) + "\",\"action\":\"" + json_escape(o.action) +
                "\",\"src\":\"nvs\"}";
      }
      for (const auto &o : yaml) {
        bool shadowed = false;
        for (const auto &n : nvs)
          if (n.name == o.name) { shadowed = true; break; }
        if (shadowed) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + json_escape(o.name) + "\",\"action\":\"" + json_escape(o.action) +
                "\",\"src\":\"yaml\"}";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "hidden") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &h = kb_->get_hidden((uint8_t) slot);
      std::string json = "{\"slot\":" + std::to_string(slot) + ",\"hidden\":[";
      json.reserve(512);
      for (size_t i = 0; i < h.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(h[i]) + "\"";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "repeat") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &r = kb_->get_repeat((uint8_t) slot);
      // "set" is what tells the page whether to use these values or fall back to
      // its own data-repeat defaults — an empty "buttons" with set:true means
      // the user deliberately turned every repeat off for this host.
      std::string json = "{\"slot\":" + std::to_string(slot) +
                         ",\"set\":" + (r.set ? "true" : "false") +
                         ",\"delay\":" + std::to_string(r.delay) +
                         ",\"rate\":" + std::to_string(r.rate) + ",\"buttons\":[";
      json.reserve(512);
      for (size_t i = 0; i < r.names.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(r.names[i]) + "\"";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "hold") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &h = kb_->get_hold((uint8_t) slot);
      const auto &r = kb_->get_repeat((uint8_t) slot);
      // The repeat set rides along so the editor can grey out the buttons that
      // are already spoken for without a second round trip.
      std::string json = "{\"slot\":" + std::to_string(slot) + ",\"buttons\":[";
      json.reserve(512);
      for (size_t i = 0; i < h.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(h[i]) + "\"";
      }
      json += "],\"repeat\":[";
      if (r.set) {
        for (size_t i = 0; i < r.names.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(r.names[i]) + "\"";
        }
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "remote_templates") {
      // The user-authored styles, each handed back as the JSON *string* it was
      // stored as rather than inlined: the device never parsed it, so embedding
      // it raw would let one malformed style break the whole response.
      // Reserve first: growing by += reallocs its way up, doubling and leaving
      // holes behind in a heap that also carries the BLE stack. Sized from what
      // is actually stored, not from the caps — over-reserving would raise the
      // very peak this is here to lower. Doubling each template covers the
      // escaping, since only " and \ expand and both go to two characters.
      size_t est = 64;
      for (uint8_t i = 0; i < EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES; i++)
        est += kb_->get_custom_template(i).size() * 2 + 24;
      std::string json = "{\"max\":";
      json.reserve(est);
      json += std::to_string(EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES);
      json += ",\"len\":";
      json += std::to_string(EspidfBleKeyboard::MAX_TEMPLATE_LEN);
      json += ",\"items\":[";
      bool first = true;
      for (uint8_t i = 0; i < EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES; i++) {
        const std::string &t = kb_->get_custom_template(i);
        if (t.empty()) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"index\":" + std::to_string(i) + ",\"tpl\":\"" + json_escape(t) + "\"}";
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    if (path == "backup") {
      // Guarded like /irk, and for a milder version of the same reason. It is
      // not a secret, but it is every macro, every override and every paired
      // host's address in one document, and web_server hands out
      // Access-Control-Allow-Origin: * globally — so without this any page the
      // user happened to have open could read the lot. The Home Assistant cards
      // read only /hosts, so nothing legitimate fetches this cross-origin, and
      // curl still can: it sends no Sec-Fetch-Site, which is allowed.
      if (!same_origin_ok(request, kb_)) {
        send_response(400, "text/plain", "Refused: read this from the device's own page");
        return;
      }
      // Everything the user can edit at runtime, in one document. Deliberately
      // excludes the passkey and the generated per-slot addresses (device
      // identity, not settings) and YAML-defined overrides (restoring those as
      // NVS entries would shadow later YAML edits). The browser adds its own
      // "ui" section before saving the file.
      // The largest document this handler serves, and the one worth reserving
      // for: built by += it reallocs about ten times on the way to 10-15 KB,
      // doubling and leaving holes each time. Measured free heap on this device
      // troughs around 23 KB, so the churn is not free. Estimated from what is
      // actually stored rather than from the caps, since a reservation larger
      // than the document raises the peak instead of lowering it.
      size_t est = 256;  // schema, device name, layout, section punctuation
      for (const auto &m : kb_->get_macros())
        est += m.name.size() + m.action.size() + 32;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        for (const auto &o : kb_->get_nvs_overrides(s))
          est += o.name.size() + o.action.size() + 16;
        for (const auto &h : kb_->get_hidden(s)) est += h.size() + 4;
        for (const auto &r : kb_->get_repeat(s).names) est += r.size() + 4;
        for (const auto &h : kb_->get_hold(s)) est += h.size() + 4;
        est += 96;  // that slot's keys, calibration, style id and host entry
      }
      std::string json = "{\"schema\":1,\"device\":\"";
      json.reserve(est);
      json += json_escape(kb_->device_name());
      json += "\",\"layout\":\"";
      json += json_escape(kb_->active_layout_id());
      json += "\",\"macros\":[";
      const auto &macros = kb_->get_macros();
      for (size_t i = 0; i < macros.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + json_escape(macros[i].name) +
                "\",\"action\":\"" + json_escape(macros[i].action) + "\"}";
      }
      json += "],\"overrides\":{";
      bool first_slot = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &ovr = kb_->get_nvs_overrides(s);
        if (ovr.empty()) continue;
        if (!first_slot) json += ",";
        first_slot = false;
        json += "\"" + std::to_string(s) + "\":{";
        for (size_t i = 0; i < ovr.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(ovr[i].name) + "\":\"" + json_escape(ovr[i].action) + "\"";
        }
        json += "}";
      }
      json += "},\"hidden\":{";
      bool first_hidden = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_hidden(s);
        if (h.empty()) continue;
        if (!first_hidden) json += ",";
        first_hidden = false;
        json += "\"" + std::to_string(s) + "\":[";
        for (size_t i = 0; i < h.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(h[i]) + "\"";
        }
        json += "]";
      }
      json += "},\"repeat\":{";
      bool first_repeat = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &r = kb_->get_repeat(s);
        if (!r.set) continue;  // unset slots restore as "reset to defaults"
        if (!first_repeat) json += ",";
        first_repeat = false;
        json += "\"" + std::to_string(s) + "\":{\"delay\":" + std::to_string(r.delay) +
                ",\"rate\":" + std::to_string(r.rate) + ",\"buttons\":[";
        for (size_t i = 0; i < r.names.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(r.names[i]) + "\"";
        }
        json += "]}";
      }
      json += "},\"hold\":{";
      bool first_hold = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_hold(s);
        if (h.empty()) continue;
        if (!first_hold) json += ",";
        first_hold = false;
        json += "\"" + std::to_string(s) + "\":[";
        for (size_t i = 0; i < h.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(h[i]) + "\"";
        }
        json += "]";
      }
      json += "},\"remote_styles\":{";
      bool first_style = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const std::string &st = kb_->get_remote_style(s);
        if (st.empty()) continue;  // an unset slot restores as the default style
        if (!first_style) json += ",";
        first_style = false;
        json += "\"" + std::to_string(s) + "\":\"" + st + "\"";
      }
      json += "},\"remote_templates\":[";
      bool first_tpl = true;
      for (uint8_t i = 0; i < EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES; i++) {
        const std::string &t = kb_->get_custom_template(i);
        if (t.empty()) continue;
        if (!first_tpl) json += ",";
        first_tpl = false;
        json += "{\"index\":" + std::to_string(i) + ",\"tpl\":\"" + json_escape(t) + "\"}";
      }
      json += "],\"goto_scale\":{";
      bool first_scale = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        float gx = 0, gy = 0;
        if (!kb_->get_saved_goto_scale(s, gx, gy)) continue;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s\"%u\":{\"x\":%.4f,\"y\":%.4f}",
                 first_scale ? "" : ",", (unsigned) s, gx, gy);
        first_scale = false;
        json += buf;
      }
      json += "},\"hosts\":[";
      bool first_host = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_host_slot(s);
        if (!h.occupied) continue;
        // This is the backup export: it records the address the host connected
        // with, because that is what restoring a slot needs. Deliberately not the
        // identity — restore feeds this straight back into the slot, and
        // advertising keys its behaviour off the connection address.
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "%s{\"slot\":%u,\"addr\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"type\":%u,\"bonded\":%s}",
                 first_host ? "" : ",", (unsigned) s,
                 h.addr[0], h.addr[1], h.addr[2], h.addr[3], h.addr[4], h.addr[5],
                 (unsigned) h.addr_type, kb_->host_slot_bonded(s) ? "true" : "false");
        first_host = false;
        json += buf;
      }
      json += "]}";
      send_response(200, "application/json", json);
      return;
    }

    // Remaining endpoints — POST only
    if (request->method() != HTTP_POST) {
      // 400 rather than the 405 this deserves. The web server maps a fixed set of
      // status codes (200, 204, 400, 401, 404, 409, 422) and sends anything else
      // as 500 — so the 405 that used to be here arrived as "500 Internal Server
      // Error", which reads as a firmware crash rather than a request that used
      // the wrong verb. The body is what actually tells the caller what to fix.
      send_response(400, "text/plain", "This endpoint requires POST");
      return;
    }

    // Everything past here changes something — types on the host, switches host,
    // rewrites stored settings. Refuse the ones a browser was made to send from
    // somewhere else; see same_origin_ok() for why this is checkable at all and
    // what it does not cover. GET endpoints are above this gate and unaffected,
    // so the Home Assistant cards, which only read /hosts, keep working.
    if (!same_origin_ok(request, kb_)) {
      send_response(400, "text/plain", "Refused: cross-site request");
      return;
    }

    if (path == "mouse_move") {
      int x = request->hasArg("x") ? atoi(request->arg("x").c_str()) : 0;
      int y = request->hasArg("y") ? atoi(request->arg("y").c_str()) : 0;
      kb_->send_mouse_move(clamp_i8(x), clamp_i8(y));
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_click") {
      int btn = request->hasArg("btn") ? atoi(request->arg("btn").c_str()) : 1;
      kb_->send_mouse_click((uint8_t) btn);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_hold") {
      int btn = request->hasArg("btn") ? atoi(request->arg("btn").c_str()) : 1;
      kb_->send_mouse_click_start((uint8_t) btn);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_release") {
      kb_->send_mouse_click_release();
      send_response(200, "text/plain", "OK");

    } else if (path == "hold_action") {
      // Press and hold, for as long as the remote's button is held. The paired
      // "release" below is sent on pointerup; max_key_hold_ms is the backstop
      // if the browser goes away before it arrives.
      //
      // Not "hold": the GET block above claims that name for the per-host set,
      // and it is matched before the POST-only check, so a POST there would
      // read back JSON instead of holding anything.
      if (!request->hasArg("action")) {
        send_response(400, "text/plain", "Missing action");
        return;
      }
      std::string action = request->arg("action").c_str();
      if (!kb_->hold_action(action)) {
        send_response(400, "text/plain", "Action cannot be held");
        return;
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "hold_key") {
      // The on-screen keyboard's press-and-hold. Unlike hold_action above this
      // takes a key rather than an action name: nothing here is remappable, and
      // nothing here is a macro — it is a keyboard, and a keyboard's keys mean
      // exactly what is printed on them.
      //
      // Holding rather than tapping is the whole point. A tap (send_key_combo)
      // is down-30ms-up, so the host never sees a key held and its own auto
      // repeat never starts. Left down, the host repeats it at whatever rate its
      // keyboard settings say — which is what a real keyboard does, and is why
      // the page does not run a repeat timer of its own.
      //
      // Two shapes, because the page knows two kinds of key. Special keys carry
      // a HID keycode; character keys carry only the character, since which key
      // types it depends on the layout — so those are resolved device-side, the
      // same way send_string resolves them.
      if (request->hasArg("char")) {
        if (!kb_->hold_char(request->arg("char").c_str())) {
          send_response(400, "text/plain", "That character cannot be held on this layout");
          return;
        }
        send_response(200, "text/plain", "OK");
        return;
      }
      // Digits only, and not empty: atoi would read a stray "abc" as 0 and hold
      // a modifier with no key, which looks like a stuck Ctrl rather than a
      // rejected request. Parsed one argument at a time and straight into an
      // int — two std::strings held side by side is 60-odd bytes of a stack this
      // handler has very little of, and adding them is what pushed the headroom
      // warning below into firing.
      int mod = parse_byte_arg(request, "modifier", 0);
      int key = parse_byte_arg(request, "keycode", -1);
      if (mod < 0 || key <= 0) {
        send_response(400, "text/plain",
                      "modifier must be 0-255 and keycode 1-255, or pass char=");
        return;
      }
      // Lifts the key first if it is somehow still down — see key_repress.
      kb_->key_repress((uint8_t) mod, (uint8_t) key);
      send_response(200, "text/plain", "OK");

    } else if (path == "release") {
      kb_->release_held();
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_scroll") {
      int amount = request->hasArg("amount") ? atoi(request->arg("amount").c_str()) : 0;
      kb_->send_mouse_scroll(clamp_i8(amount));
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_abs") {
      // Move the pointer to an absolute position. x/y are percent by default,
      // pixels when unit=px; monitor=<idx> selects a declared monitor region.
      //
      // These are spliced into an action string, and execute_action() splits on
      // '|', so an argument carrying one used to append an entire extra step:
      // ?x=50|string:hello typed "hello" on the host. No privilege was gained —
      // /press accepts arbitrary actions anyway — but a mistyped parameter did
      // something surprising instead of failing. Digits, sign and decimal point
      // only, and not empty, which would build a malformed action of its own.
      auto numeric_ok = [](const std::string &v) {
        return !v.empty() && v.find_first_not_of("-0123456789.") == std::string::npos;
      };
      std::string sx = request->hasArg("x") ? request->arg("x").c_str() : "0";
      std::string sy = request->hasArg("y") ? request->arg("y").c_str() : "0";
      std::string smon = request->hasArg("monitor") ? request->arg("monitor").c_str() : "";
      if (!numeric_ok(sx) || !numeric_ok(sy) ||
          (request->hasArg("monitor") && !numeric_ok(smon))) {
        send_response(400, "text/plain", "x, y and monitor must be numeric");
        return;
      }
      std::string action;
      if (request->hasArg("monitor")) {
        action = "mouse_abs_mon:" + smon + ":" + sx + ":" + sy;
      } else if (request->hasArg("unit") && std::string(request->arg("unit").c_str()) == "px") {
        action = "mouse_abs_px:" + sx + ":" + sy;
      } else {
        action = "mouse_abs:" + sx + ":" + sy;
      }
      kb_->execute_action(action);
      if (request->hasArg("btn")) {
        int btn = atoi(request->arg("btn").c_str());
        if (btn != 0) kb_->send_mouse_click((uint8_t) btn);  // clicks at the new position
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "goto_scale") {
      // Live mouse_goto calibration. v sets both axes; vx/vy set each; reset
      // restores the YAML defaults; save persists to the active host (NVS).
      if (request->hasArg("reset")) {
        kb_->reset_goto_scale_for_host();
        send_response(200, "text/plain", "OK");
      } else {
        // A value outside the range used to be dropped while the endpoint still
        // answered OK, so the Finder's live preview could not tell a rejected
        // value from an applied one. Answers like goto_scale_slot now.
        //
        // Validated before anything is applied, so a request carrying one good
        // and one bad axis changes neither — the old code set the good one and
        // reported success for both. And strtof rather than atof: atof turns
        // "abc" into 0.0, which the range check rejects with a message about the
        // range, when the actual problem is that it is not a number.
        bool bad_number = false, out_of_range = false, anything = false;
        float fv = 0.0f, fx = 0.0f, fy = 0.0f;
        auto take = [&](const char *name, float &out) {
          if (!request->hasArg(name)) return false;
          anything = true;
          const std::string s = request->arg(name).c_str();
          char *end = nullptr;
          const float parsed = strtof(s.c_str(), &end);
          if (end == s.c_str() || *end != '\0') { bad_number = true; return false; }
          if (parsed < 0.05f || parsed > 20.0f) { out_of_range = true; return false; }
          out = parsed;
          return true;
        };
        const bool has_v = take("v", fv), has_vx = take("vx", fx), has_vy = take("vy", fy);
        const bool save = request->hasArg("save");
        if (save) anything = true;
        if (bad_number) {
          send_response(400, "text/plain", "Scale must be a number");
        } else if (out_of_range) {
          send_response(400, "text/plain", "Scale must be 0.05-20.0");
        } else if (!anything) {
          send_response(400, "text/plain", "Nothing to do - pass v, vx, vy, reset or save");
        } else {
          if (has_v) kb_->set_mouse_goto_scale(fv);
          if (has_vx) kb_->set_mouse_goto_scale_x(fx);
          if (has_vy) kb_->set_mouse_goto_scale_y(fy);
          if (save) kb_->save_goto_scale_for_host();  // persist to active host
          send_response(200, "text/plain", "OK");
        }
      }

    } else if (path == "string") {
      if (request->hasArg("keys")) {
        std::string keys = request->arg("keys").c_str();
        ESP_LOGD(TAG, "WEB /string keys=\"%s\"", keys.c_str());
        kb_->send_string(keys);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "key") {
      int modifier = request->hasArg("modifier") ? atoi(request->arg("modifier").c_str()) : 0;
      int keycode = request->hasArg("keycode") ? atoi(request->arg("keycode").c_str()) : 0;
      ESP_LOGD(TAG, "WEB /key mod=%d keycode=%d", modifier, keycode);
      kb_->send_key_combo((uint8_t) modifier, (uint8_t) keycode);
      send_response(200, "text/plain", "OK");

    } else if (path == "press") {
      // Press a button by action string
      if (request->hasArg("action")) {
        std::string action = request->arg("action").c_str();
        kb_->execute_action(action);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "switch_host") {
      // Checked before the narrowing cast, as every other slot endpoint does.
      // The cast used to happen first, so ?slot=256 became 0 and quietly
      // switched to host 0 — and a slot this device simply does not have was
      // answered OK while nothing happened at all. Missing slot is no longer
      // treated as slot 0 either; the page has always sent one.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      kb_->switch_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else if (path == "forget_host") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      kb_->forget_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else if (path == "macro_add") {
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (name.empty() || action.empty()) {
        send_response(400, "text/plain", "name and action required");
      } else if (name.size() > 31 || action.size() > 255) {
        send_response(400, "text/plain", "name max 31, action max 255 chars");
      } else if (name.find('|') != std::string::npos) {
        send_response(400, "text/plain", "Name cannot contain '|'");
      } else if (!kb_->macro_name_available(name, -1)) {
        send_response(400, "text/plain", "A macro with that name already exists");
      } else if (!kb_->add_macro(name, action)) {
        send_response(400, "text/plain", "Max macros reached");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "macro_update") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (index < 0 || name.empty() || action.empty()) {
        send_response(400, "text/plain", "index, name, and action required");
      } else if (name.size() > 31 || action.size() > 255) {
        send_response(400, "text/plain", "name max 31, action max 255 chars");
      } else if (name.find('|') != std::string::npos) {
        send_response(400, "text/plain", "Name cannot contain '|'");
      } else if (!kb_->macro_name_available(name, index)) {
        send_response(400, "text/plain", "A macro with that name already exists");
      } else if (!kb_->update_macro((uint8_t) index, name, action)) {
        send_response(404, "text/plain", "Invalid index");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "set_layout") {
      std::string id = request->hasArg("id") ? request->arg("id").c_str() : "";
      if (id.empty()) {
        send_response(400, "text/plain", "id required");
      } else {
        const KeyboardLayout *prev = kb_->active_layout();
        kb_->set_runtime_layout(id);
        if (kb_->active_layout() == prev && (prev == nullptr || id != prev->id)) {
          send_response(400, "text/plain", "Unknown layout");
        } else {
          send_response(200, "text/plain", "OK");
        }
      }

    } else if (path == "override_set") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (!EspidfBleKeyboard::valid_override_name(name)) {
        send_response(400, "text/plain",
                      "Invalid action name: max 31 chars, no '=', '|' or spaces. Only named "
                      "actions can be overridden (e.g. record, play_pause, stop).");
      } else if (action.empty() || action.size() > 255) {
        send_response(400, "text/plain", "Action required, max 255 chars");
      } else if (!kb_->set_override((uint8_t) slot, name, action)) {
        send_response(400, "text/plain", "Max overrides reached for this host (8)");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "hidden_set") {
      // Replaces the whole set for one slot; an empty "names" clears it.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (list.size() > EspidfBleKeyboard::MAX_HIDDEN) {
        // Built from the constant, not spelled out: these messages said "max 40"
        // for a while after the cap moved, which is worse than no number.
        std::string msg = "Too many hidden buttons (max " +
                          std::to_string(EspidfBleKeyboard::MAX_HIDDEN) + ")";
        send_response(400, "text/plain", msg);
      } else if (!kb_->set_hidden((uint8_t) slot, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "repeat_set") {
      // Replaces the whole set for one slot. An empty "names" is a real setting
      // ("nothing repeats here"); "reset=1" is what returns the slot to the
      // page's built-in defaults.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      // Via c_str() rather than comparing arg() directly: the return type differs
      // between the Async and esp-idf web server backends, .c_str() does not.
      bool reset = request->hasArg("reset") &&
                   std::string(request->arg("reset").c_str()) == "1";
      int delay = request->hasArg("delay") ? atoi(request->arg("delay").c_str()) : 400;
      int rate = request->hasArg("rate") ? atoi(request->arg("rate").c_str()) : 180;
      // Ceiling applied here, before the narrowing cast below: set_repeat() would
      // clamp too, but only after a value past 65535 had already wrapped into
      // something plausible-looking. The floor is left to set_repeat().
      if (delay > EspidfBleKeyboard::REPEAT_DELAY_MAX) delay = EspidfBleKeyboard::REPEAT_DELAY_MAX;
      if (rate > EspidfBleKeyboard::REPEAT_RATE_MAX) rate = EspidfBleKeyboard::REPEAT_RATE_MAX;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      // Resolved before the chain so the message can name the button; the setter
      // makes the same check but only reports pass/fail.
      std::string clash = (slot >= 0 && slot < kb_->host_slots())
                              ? kb_->hold_repeat_conflict((uint8_t) slot, list, false) : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (reset) {
        kb_->clear_repeat((uint8_t) slot);
        send_response(200, "text/plain", "OK");
      } else if (list.size() > EspidfBleKeyboard::MAX_REPEAT_BUTTONS) {
        std::string msg = "Too many repeat buttons (max " +
                          std::to_string(EspidfBleKeyboard::MAX_REPEAT_BUTTONS) + ")";
        send_response(400, "text/plain", msg);
      } else if (delay < 0 || rate < 0) {
        send_response(400, "text/plain", "Invalid timing");
      } else if (!clash.empty()) {
        std::string msg = "\"" + clash + "\" is set to hold on this host — untick it there first";
        send_response(400, "text/plain", msg);
      } else if (!kb_->set_repeat((uint8_t) slot, (uint16_t) delay, (uint16_t) rate, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "hold_set") {
      // Replaces the whole press-and-hold set for one slot. Empty clears it.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      std::string clash = (slot >= 0 && slot < kb_->host_slots())
                              ? kb_->hold_repeat_conflict((uint8_t) slot, list, true) : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (list.size() > EspidfBleKeyboard::MAX_HOLD) {
        std::string msg = "Too many hold buttons (max " +
                          std::to_string(EspidfBleKeyboard::MAX_HOLD) + ")";
        send_response(400, "text/plain", msg);
      } else if (!clash.empty()) {
        std::string msg = "\"" + clash + "\" is set to repeat on this host — untick it there first";
        send_response(400, "text/plain", msg);
      } else if (!kb_->set_hold((uint8_t) slot, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "remote_style_set") {
      // Which style the web remote draws for one host. An empty id clears the
      // slot back to the default. The id is never interpreted here — see
      // set_remote_style() for why the firmware stays out of the layout.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string id = request->hasArg("id") ? request->arg("id").c_str() : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (!kb_->set_remote_style((uint8_t) slot, id)) {
        send_response(400, "text/plain", "Invalid style id (max 15 chars: a-z, 0-9, _)");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "remote_tpl_chunk") {
      // One piece of a custom style upload. Chunked because the web server
      // takes 512 bytes of URL and an encoded style runs several times that;
      // seq=0 starts a fresh upload, so a failed one leaves nothing to clean up.
      int seq = request->hasArg("seq") ? atoi(request->arg("seq").c_str()) : -1;
      std::string data = request->hasArg("data") ? request->arg("data").c_str() : "";
      if (seq < 0 || seq > 65535) {
        send_response(400, "text/plain", "Invalid chunk number");
      } else if (!kb_->stage_template_chunk((uint16_t) seq, data)) {
        send_response(400, "text/plain",
                      "Chunk rejected — start the upload again (styles are capped at 1500 characters)");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "remote_tpl_save") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      if (index < 0 || index >= EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES) {
        send_response(400, "text/plain", "Invalid style slot");
      } else if (!kb_->commit_template((uint8_t) index)) {
        send_response(400, "text/plain", "Nothing uploaded to save");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "remote_tpl_delete") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      if (index < 0 || index >= EspidfBleKeyboard::MAX_CUSTOM_TEMPLATES) {
        send_response(400, "text/plain", "Invalid style slot");
      } else {
        // Deliberately does not touch the hosts pointing at it: the page falls
        // back to the default for an id it can't resolve, so re-importing the
        // style under the same id puts every one of them back.
        kb_->delete_template((uint8_t) index);
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "goto_scale_slot") {
      // Restore calibration for any slot, not just the active one.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      float x = request->hasArg("x") ? atof(request->arg("x").c_str()) : 0.0f;
      float y = request->hasArg("y") ? atof(request->arg("y").c_str()) : 0.0f;
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (x < 0.05f || x > 20.0f || y < 0.05f || y > 20.0f) {
        send_response(400, "text/plain", "Scale must be 0.05-20.0");
      } else {
        kb_->set_saved_goto_scale((uint8_t) slot, x, y);
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "set_host_slot") {
      // Restores the slot -> address mapping only. BLE bonding keys live in the
      // stack's own NVS and cannot be exported, so a slot restored without a
      // surviving bond will advertise at a host that refuses encryption until
      // it is re-paired. The UI warns about this before calling here.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string addr_str = request->hasArg("addr") ? request->arg("addr").c_str() : "";
      int type = request->hasArg("type") ? atoi(request->arg("type").c_str()) : 0;
      unsigned int b[6];
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (addr_str.size() != 17 ||
                 sscanf(addr_str.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
                        &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
        send_response(400, "text/plain", "addr must be AA:BB:CC:DD:EE:FF");
      } else if (type < 0 || type > 3) {
        send_response(400, "text/plain", "Invalid address type");
      } else {
        esp_bd_addr_t addr;
        for (int i = 0; i < 6; i++) addr[i] = (uint8_t) b[i];
        kb_->assign_host_slot_((uint8_t) slot, addr, (esp_ble_addr_type_t) type);
        kb_->save_host_slots_();
        send_response(200, "text/plain", kb_->host_slot_bonded((uint8_t) slot) ? "OK" : "OK-NOBOND");
      }

    } else if (path == "override_clear") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      if (slot < 0 || slot >= kb_->host_slots() || name.empty()) {
        send_response(400, "text/plain", "slot and name required");
      } else if (!kb_->clear_override((uint8_t) slot, name)) {
        send_response(404, "text/plain", "No saved override with that name");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "macro_delete") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      if (index < 0) {
        send_response(400, "text/plain", "index required");
      } else if (!kb_->delete_macro((uint8_t) index)) {
        send_response(404, "text/plain", "Invalid index");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else {
      send_response(404, "text/plain", "Unknown endpoint");
    }
  }

 protected:
  EspidfBleKeyboard *kb_;
};

// ── Setup ──────────────────────────────────────────────────────────

void BleKeyboardWebControl::setup() {
  auto *handler = new BleKbWebHandler(this->keyboard_);
  this->base_->add_handler(handler);
  ESP_LOGI(TAG, "Web control registered at /ble_keyboard");
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome

#endif  // USE_BLE_KEYBOARD_WEB_CONTROL



