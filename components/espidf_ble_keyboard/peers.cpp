// Linked keyboards: the peer: and to: verbs, and the cache of each peer's /state that
// lets this keyboard's page show another keyboard's hosts. Compiled only when
// the config lists peers: — a keyboard without peers carries none of this.
//
// Everything that talks to a peer runs on the action task. Presses go out in
// the order they were queued, so a macro's delay: still means what it says, and
// the cache is refreshed only on that task's quiet ticks and only while a page
// is asking for it. The web task never waits on the network: /peers takes a
// reference to the cache under peer_mutex_ and sends it from there, uncopied.
//
// The HTTP here is a few hundred lines on plain sockets rather than ESP-IDF's
// esp_http_client, which linked its TLS stack and a DNS resolver for requests
// that are neither — about 86 KB of flash, and several KB of heap per request on
// a keyboard whose heap is what runs out first.
#include "espidf_ble_keyboard.h"

#ifdef USE_BLE_KB_PEERS

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_rom_md5.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#ifdef USE_MDNS
#include <mdns.h>
#endif

namespace esphome {
namespace espidf_ble_keyboard {

static const char *const TAG = "espidf_ble_keyboard.peer";

// After a press fails, further presses to that peer are dropped for this long
// rather than each waiting out the timeout — five presses at a dead keyboard
// would otherwise hold the action task for over seven seconds.
static const uint32_t PEER_DOWN_MS = 5000;
// A reply's status line and headers. ESPHome's run to a few hundred bytes.
static const size_t PEER_MAX_HEAD = 1536;
static const uint32_t PEER_MDNS_TIMEOUT_MS = 2000;

namespace {

// application/x-www-form-urlencoded, the way the page's own apiPost sends it:
// the far keyboard reads its parameters from the body.
void form_encode_append(std::string &out, const std::string &s) {
  static const char HEX[] = "0123456789ABCDEF";
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '*') {
      out += (char) c;
    } else if (c == ' ') {
      out += '+';
    } else {
      out += '%';
      out += HEX[c >> 4];
      out += HEX[c & 15];
    }
  }
}

void base64_append(std::string &out, const std::string &in) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0;
  for (; i + 2 < in.size(); i += 3) {
    const uint32_t v = ((uint8_t) in[i] << 16) | ((uint8_t) in[i + 1] << 8) | (uint8_t) in[i + 2];
    out += T[(v >> 18) & 63];
    out += T[(v >> 12) & 63];
    out += T[(v >> 6) & 63];
    out += T[v & 63];
  }
  if (i < in.size()) {
    uint32_t v = (uint8_t) in[i] << 16;
    if (i + 1 < in.size())
      v |= (uint8_t) in[i + 1] << 8;
    out += T[(v >> 18) & 63];
    out += T[(v >> 12) & 63];
    out += i + 1 < in.size() ? T[(v >> 6) & 63] : '=';
    out += '=';
  }
}

// MD5 through the chip's ROM, which ESPHome's own web server already uses to
// check the same logins. Lowercase hex, which is what that check compares.
class Md5Hex {
 public:
  Md5Hex() { esp_rom_md5_init(&ctx_); }
  Md5Hex &add(const char *s, size_t n) {
    esp_rom_md5_update(&ctx_, s, n);
    return *this;
  }
  Md5Hex &add(const char *s) { return add(s, strlen(s)); }
  Md5Hex &add(const std::string &s) { return add(s.data(), s.size()); }
  void hex(char out[33]) {
    uint8_t d[16];
    esp_rom_md5_final(d, &ctx_);
    for (int i = 0; i < 16; i++)
      snprintf(out + i * 2, 3, "%02x", d[i]);
  }

 private:
  md5_context_t ctx_;
};

// One parameter of a WWW-Authenticate header, quoted or bare; empty if absent.
std::string auth_param(const std::string &h, const char *key) {
  const size_t klen = strlen(key);
  size_t i = 0;
  while (i < h.size()) {
    while (i < h.size() && (h[i] == ' ' || h[i] == ','))
      i++;
    const size_t name = i;
    while (i < h.size() && h[i] != '=' && h[i] != ',' && h[i] != ' ')
      i++;
    const bool match = i - name == klen && strncasecmp(h.c_str() + name, key, klen) == 0;
    while (i < h.size() && h[i] == ' ')
      i++;
    if (i >= h.size() || h[i] != '=')
      continue;
    i++;
    std::string val;
    if (i < h.size() && h[i] == '"') {
      const size_t end = h.find('"', i + 1);
      val = h.substr(i + 1, (end == std::string::npos ? h.size() : end) - i - 1);
      i = end == std::string::npos ? h.size() : end + 1;
    } else {
      const size_t end = h.find(',', i);
      val = h.substr(i, (end == std::string::npos ? h.size() : end) - i);
      i = end == std::string::npos ? h.size() : end;
    }
    if (match)
      return val;
  }
  return std::string();
}

// Closes the socket on every way out of a function.
struct SocketGuard {
  int fd;
  explicit SocketGuard(int f) : fd(f) {}
  ~SocketGuard() {
    if (fd >= 0)
      lwip_close(fd);
  }
};

bool send_all(int fd, const char *data, size_t len) {
  while (len > 0) {
    const int n = lwip_send(fd, data, len, 0);
    if (n <= 0)
      return false;
    data += n;
    len -= (size_t) n;
  }
  return true;
}

}  // namespace

void EspidfBleKeyboard::add_peer(const std::string &name, const std::string &url, const std::string &user,
                                 const std::string &pass) {
  if (peer_mutex_ == nullptr)
    peer_mutex_ = xSemaphoreCreateMutex();
  Peer p;
  p.name = name;
  p.user = user;
  p.pass = pass;
  // http://<host>[:port], checked by the schema. An address is used as it is; a
  // .local name is looked up over mDNS the first time it is needed.
  std::string host = url.rfind("http://", 0) == 0 ? url.substr(7) : url;
  const size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    p.port = (uint16_t) atoi(host.c_str() + colon + 1);
    host.resize(colon);
  }
  p.host = host;
  ip4_addr_t addr;
  if (ip4addr_aton(host.c_str(), &addr))
    p.ip = addr.addr;
  else
    p.by_name = true;
  peers_.push_back(std::move(p));
}

// Never 0, which is what "nobody has asked yet" reads as.
void EspidfBleKeyboard::note_peer_interest() { peer_interest_ms_.store(millis() | 1); }
void EspidfBleKeyboard::note_web_busy() { web_busy_ms_.store(millis() | 1); }
void EspidfBleKeyboard::note_page_load() {
  const uint32_t now = millis() | 1;
  page_load_ms_.store(now);
  web_busy_ms_.store(now);
}

// The peer's address: given, or found over mDNS for a .local name and kept until
// a connection to it fails.
bool EspidfBleKeyboard::peer_resolve_(Peer &p) {
  if (p.ip != 0)
    return true;
#ifdef USE_MDNS
  const size_t dot = p.host.find('.');
  const std::string label = p.host.substr(0, dot);
  esp_ip4_addr_t addr{};
  if (mdns_query_a(label.c_str(), PEER_MDNS_TIMEOUT_MS, &addr) == ESP_OK && addr.addr != 0) {
    p.ip = addr.addr;
    const auto *b = reinterpret_cast<const uint8_t *>(&addr.addr);  // network order
    ESP_LOGI(TAG, "Peer %s is at %u.%u.%u.%u", p.name.c_str(), b[0], b[1], b[2], b[3]);
    return true;
  }
  ESP_LOGW(TAG, "Peer %s: nothing answered for %s", p.name.c_str(), p.host.c_str());
#else
  ESP_LOGW(TAG, "Peer %s: %s needs mDNS, which this config has turned off — give its address instead",
           p.name.c_str(), p.host.c_str());
#endif
  return false;
}

// The Authorization header for the next request, from the challenge the peer
// last sent. ESPHome issues a fresh nonce with each 401 and accepts a response
// built on any of them, so one challenge serves every request after it; if a
// peer ever does refuse it, peer_request_() takes the new challenge and asks
// once more.
void EspidfBleKeyboard::peer_auth_append_(Peer &p, const char *method, const char *path, std::string &out) {
  if (p.user.empty() || p.auth == Peer::AUTH_NONE)
    return;
  if (p.auth == Peer::AUTH_BASIC) {
    out += "Authorization: Basic ";
    base64_append(out, p.user + ":" + p.pass);
    out += "\r\n";
    return;
  }
  char ha1[33], ha2[33], resp[33], nc[9], cnonce[17];
  p.nc++;
  snprintf(nc, sizeof(nc), "%08x", (unsigned) p.nc);
  snprintf(cnonce, sizeof(cnonce), "%08x%08x", (unsigned) esp_random(), (unsigned) esp_random());
  Md5Hex().add(p.user).add(":").add(p.realm).add(":").add(p.pass).hex(ha1);
  Md5Hex().add(method).add(":").add(path).hex(ha2);
  Md5Hex r;
  r.add(ha1, 32).add(":").add(p.nonce).add(":");
  if (p.qop_auth)
    r.add(nc).add(":").add(cnonce).add(":auth:");
  r.add(ha2, 32).hex(resp);
  out += "Authorization: Digest username=\"";
  out += p.user;
  out += "\", realm=\"";
  out += p.realm;
  out += "\", nonce=\"";
  out += p.nonce;
  out += "\", uri=\"";
  out += path;
  out += "\", algorithm=MD5, response=\"";
  out += resp;
  out += '"';
  if (p.qop_auth) {
    out += ", qop=auth, nc=";
    out += nc;
    out += ", cnonce=\"";
    out += cnonce;
    out += '"';
  }
  if (!p.opaque.empty()) {
    out += ", opaque=\"";
    out += p.opaque;
    out += '"';
  }
  out += "\r\n";
}

// Keeps what a 401 asked for. False if it asked for something this can't do.
bool EspidfBleKeyboard::peer_take_challenge_(Peer &p, const std::string &challenge) {
  if (strncasecmp(challenge.c_str(), "Digest ", 7) == 0) {
    const std::string params = challenge.substr(7);
    p.realm = auth_param(params, "realm");
    p.nonce = auth_param(params, "nonce");
    p.opaque = auth_param(params, "opaque");
    p.qop_auth = auth_param(params, "qop").find("auth") != std::string::npos;
    p.nc = 0;
    p.auth = p.nonce.empty() ? Peer::AUTH_NONE : Peer::AUTH_DIGEST;
  } else if (strncasecmp(challenge.c_str(), "Basic", 5) == 0) {
    p.auth = Peer::AUTH_BASIC;
  } else {
    p.auth = Peer::AUTH_NONE;
  }
  return p.auth != Peer::AUTH_NONE;
}

// One request on a connection of its own, which the peer closes after its
// reply. `status` is 0 when no complete reply came back.
EspidfBleKeyboard::PeerResult EspidfBleKeyboard::peer_exchange_(Peer &p, bool post, const char *path,
                                                                const std::string &body, std::string *out,
                                                                int &status, std::string &challenge) {
  status = 0;
  const uint32_t timeout = post ? PEER_TIMEOUT_MS : PEER_READ_TIMEOUT_MS;
  const int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    ESP_LOGW(TAG, "Peer %s: no socket free", p.name.c_str());
    return PEER_BUSY;
  }
  SocketGuard guard(fd);
  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  // Connect with a time limit: a keyboard that is off would otherwise hold the
  // action task for as long as TCP keeps retrying.
  struct sockaddr_in sa {};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(p.port);
  sa.sin_addr.s_addr = p.ip;
  const int flags = lwip_fcntl(fd, F_GETFL, 0);
  lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  int r = lwip_connect(fd, reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa));
  if (r < 0 && errno != EINPROGRESS)
    return PEER_UNREACHABLE;
  if (r < 0) {
    fd_set wr;
    FD_ZERO(&wr);
    FD_SET(fd, &wr);
    struct timeval ctv = tv;
    int err = 0;
    socklen_t len = sizeof(err);
    if (lwip_select(fd + 1, nullptr, &wr, nullptr, &ctv) <= 0 ||
        lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
      return PEER_UNREACHABLE;
  }
  lwip_fcntl(fd, F_SETFL, flags);
  lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  std::string head;
  head.reserve(p.auth == Peer::AUTH_DIGEST ? 512 : 192);
  head += post ? "POST " : "GET ";
  head += path;
  head += " HTTP/1.1\r\nHost: ";
  head += p.host;
  if (p.port != 80) {
    head += ':';
    head += std::to_string(p.port);
  }
  head += "\r\nConnection: close\r\n";
  peer_auth_append_(p, post ? "POST" : "GET", path, head);
  if (post) {
    head += "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ";
    head += std::to_string(body.size());
    head += "\r\n";
  }
  head += "\r\n";
  if (!send_all(fd, head.data(), head.size()) || (post && !send_all(fd, body.data(), body.size())))
    return PEER_NO_REPLY;
  std::string().swap(head);

  // The reply: status line and headers into `hdr`, then the body — kept only
  // when it is a 200 someone asked for, and never past PEER_MAX_REPLY.
  std::string hdr;
  hdr.reserve(256);
  char buf[256];
  bool in_body = false;
  long want = -1;  // Content-Length, or -1 to read until the peer closes
  size_t got = 0;
  bool too_big = false;
  while (true) {
    if (in_body && want >= 0 && got >= (size_t) want)
      break;
    const int n = lwip_recv(fd, buf, sizeof(buf), 0);
    if (n == 0)
      break;  // closed: the reply is complete
    if (n < 0)
      return PEER_NO_REPLY;  // timed out or reset part-way
    size_t off = 0;
    if (!in_body) {
      hdr.append(buf, (size_t) n);
      const size_t end = hdr.find("\r\n\r\n");
      if (end == std::string::npos) {
        if (hdr.size() > PEER_MAX_HEAD)
          return PEER_NO_REPLY;
        continue;
      }
      in_body = true;
      // Status line, then the two headers this needs.
      const size_t sp = hdr.find(' ');
      status = sp == std::string::npos ? 0 : atoi(hdr.c_str() + sp + 1);
      size_t line = hdr.find("\r\n");
      while (line != std::string::npos && line < end) {
        const size_t start = line + 2;
        const size_t next = hdr.find("\r\n", start);
        const size_t colon = hdr.find(':', start);
        if (colon != std::string::npos && colon < next) {
          size_t v = colon + 1;
          while (v < next && hdr[v] == ' ')
            v++;
          const size_t nlen = colon - start;
          if (nlen == 14 && strncasecmp(hdr.c_str() + start, "content-length", 14) == 0)
            want = atol(hdr.c_str() + v);
          else if (nlen == 16 && strncasecmp(hdr.c_str() + start, "www-authenticate", 16) == 0)
            challenge = hdr.substr(v, next - v);
        }
        line = next;
      }
      // Whatever came in past the headers is the start of the body.
      const size_t body_at = end + 4;
      const size_t extra = hdr.size() - body_at;
      off = (size_t) n - extra;
    }
    const size_t len = (size_t) n - off;
    got += len;
    if (out != nullptr && status == 200 && !too_big) {
      if (out->size() + len > PEER_MAX_REPLY)
        too_big = true;
      else
        out->append(buf + off, len);
    }
  }
  if (!in_body || status == 0)
    return PEER_NO_REPLY;
  if (too_big) {
    ESP_LOGW(TAG, "Peer %s: reply to %s over %u bytes, ignored", p.name.c_str(), path, (unsigned) PEER_MAX_REPLY);
    return PEER_NO_REPLY;
  }
  if (want >= 0 && got < (size_t) want)
    return PEER_NO_REPLY;  // closed early
  return PEER_OK;
}

// A request, logging in when the peer asks: its first reply is a 401 carrying
// the challenge, and every request after that answers it up front.
EspidfBleKeyboard::PeerResult EspidfBleKeyboard::peer_request_(Peer &p, bool post, const char *path,
                                                               const std::string &body, std::string *out) {
  if (!peer_resolve_(p))
    return PEER_UNREACHABLE;
  const uint32_t start = millis();
  int status = 0;
  PeerResult r = PEER_NO_REPLY;
  for (int attempt = 0; attempt < 2; attempt++) {
    std::string challenge;
    if (out != nullptr)
      out->clear();
    r = peer_exchange_(p, post, path, body, out, status, challenge);
    if (r != PEER_OK || status != 401 || p.user.empty() || attempt > 0 || !peer_take_challenge_(p, challenge))
      break;
  }
  const uint32_t took = millis() - start;
  // Normal is well under this; a slower answer is what fills the action queue.
  if (took > PEER_SLOW_MS)
    ESP_LOGW(TAG, "Peer %s: %s %s took %u ms", p.name.c_str(), post ? "POST" : "GET", path, (unsigned) took);
  if (r == PEER_UNREACHABLE) {
    ESP_LOGW(TAG, "Peer %s: could not connect to %s", p.name.c_str(), p.host.c_str());
    // A .local name may have moved to another address; look it up again.
    if (p.by_name)
      p.ip = 0;
    return r;
  }
  if (r != PEER_OK) {
    if (r == PEER_NO_REPLY)
      ESP_LOGW(TAG, "Peer %s: %s %s got no reply in time", p.name.c_str(), post ? "POST" : "GET", path);
    return r;
  }
  if (status == 200)
    return PEER_OK;
  if (status == 409)
    return PEER_BUSY;  // it logs that itself, with its heap figures
  if (status == 401)
    ESP_LOGW(TAG, "Peer %s refused the login — check username and password under peers:", p.name.c_str());
  else
    ESP_LOGW(TAG, "Peer %s: %s %s answered HTTP %d", p.name.c_str(), post ? "POST" : "GET", path, status);
  return PEER_NO_REPLY;
}

// peer:<name>:<action> — run <action> on that keyboard, exactly as its own page
// would: a press, hold:<a>, release, switch_host:N, a macro, anything.
void EspidfBleKeyboard::run_peer_action_(const std::string &action) {
  const size_t sep = action.find(':', 5);
  if (sep == std::string::npos || sep == 5 || sep + 1 >= action.size()) {
    ESP_LOGW(TAG, "peer: needs a name and an action, e.g. peer:bedroom:volume_up — got %s", action.c_str());
    return;
  }
  Peer *p = nullptr;
  for (auto &cand : peers_) {
    if (action.compare(5, sep - 5, cand.name) == 0 && cand.name.size() == sep - 5) {
      p = &cand;
      break;
    }
  }
  if (p == nullptr) {
    ESP_LOGW(TAG, "No peer called '%s' — peers: in this keyboard's YAML names them",
             action.substr(5, sep - 5).c_str());
    return;
  }
  // A YAML run_action runs on the loop, and the loop must never wait on the
  // network. Handed over, this step runs after whatever the loop does next
  // instead of in line with it — the price of not stalling everything else.
  if (action_task_ != nullptr && xTaskGetCurrentTaskHandle() != action_task_) {
    queue_action(action);
    return;
  }
  // Text goes to its /string, which types it as it is. Its /press would run it
  // through its own execute_action, which splits at a '|' and runs whatever
  // follows as an action over there.
  const bool text = action.compare(sep + 1, 7, "string:") == 0;
  const size_t from = text ? sep + 8 : sep + 1;
  std::string body = text ? "keys=" : "action=";
  body.reserve(body.size() + (action.size() - from) * 3);
  form_encode_append(body, action.substr(from));
  const PeerResult r =
      peer_post_(*p, text ? "/api/ble_keyboard/string" : "/api/ble_keyboard/press", body, action.c_str() + sep + 1);
  // A press can switch its host or re-skin its remote, so read it again soon
  // instead of on the usual timer — only happens while a page is looking.
  // Text does neither.
  if (r == PEER_OK && !text)
    p->next_due_ms = (millis() + 300) | 1;
}

// to:<name>:<action> — run <action> on this keyboard, with everything it would
// send to a host sent to that keyboard's host instead: this keyboard's macro,
// conditions and buttons, that keyboard's keys and Host Actions. What a tab
// driving a linked keyboard sends for a macro. What runs here and what goes
// over is gather_for_peer_()'s call. What goes over travels as one chain, so it
// runs over there in order, delays and all, each step waiting for the text an
// earlier one typed — sent a step at a time, an Enter would land mid-text.
void EspidfBleKeyboard::run_to_peer_(const std::string &action) {
  const size_t sep = action.find(':', 3);
  if (sep == std::string::npos || sep == 3 || sep + 1 >= action.size()) {
    ESP_LOGW(TAG, "to: needs a name and an action, e.g. to:bedroom:macro:Netflix — got %s", action.c_str());
    return;
  }
  const int index = peer_index(action.substr(3, sep - 3));
  if (index < 0) {
    ESP_LOGW(TAG, "No peer called '%s' — peers: in this keyboard's YAML names them",
             action.substr(3, sep - 3).c_str());
    return;
  }
  // On the action task only: the loop must never wait on the network, and a
  // chain the loop runs meanwhile must not have its keys gathered into this one.
  if (xTaskGetCurrentTaskHandle() != action_task_) {
    if (action_task_ != nullptr) {
      queue_action(action);
    } else {
      ESP_LOGW(TAG, "%s needs the action task, which could not be started", action.c_str());
    }
    return;
  }
  // A to: inside another: what that one gathered went before this step reached
  // here (gather_for_peer_), so this one starts empty, and the outer one carries
  // on once it is done — on its visit, if it was on one. Aimed at the same
  // keyboard, it may still be typing what the outer one sent.
  const int8_t outer_peer = out_peer_, outer_away = out_away_;
  const int8_t outer_target = outer_peer >= 0 ? outer_peer : outer_away;
  const bool outer_sent = out_sent_;
  out_sent_ = outer_target == index && outer_sent;
  out_peer_ = (int8_t) index;
  out_away_ = -1;
  execute_action(action.substr(sep + 1));
  flush_for_peer_(false);
  out_sent_ = outer_sent || (outer_target == index && out_sent_);
  out_peer_ = outer_peer;
  out_away_ = outer_away;
  if (out_peer_ < 0 && out_away_ < 0) {
    out_body_.clear();
    out_body_.shrink_to_fit();
  }
}

// A step of a to: run, before execute_action acts on it. True when it was taken
// for the peer. False leaves it to execute_action: a chain to split, or a macro
// to look up — their steps come back here — or something this keyboard does
// itself: press one of its buttons, call Home Assistant, or a step that names a
// keyboard of its own. Whatever was gathered is sent before those, so the order
// holds.
bool EspidfBleKeyboard::gather_for_peer_(const std::string &step) {
  if (out_peer_ < 0) {
    // On a visit to one of this keyboard's hosts (below): every step runs here
    // until switch_host:back or last_host ends it, and the keys after it go to
    // the linked keyboard again.
    if (step != "switch_host:back" && step != "last_host")
      return false;
    const int8_t home = out_away_;
    out_away_ = -1;  // so this step runs here like the rest of the visit
    execute_action(step);
    out_peer_ = home;
    return true;
  }
  const bool named = step.rfind("peer:", 0) == 0;
  if (named || step.rfind("to:", 0) == 0) {
    flush_for_peer_(true);
    // A step for this run's own keyboard: what is gathered for it next waits
    // behind any text that step types there.
    const std::string &name = peers_[out_peer_].name;
    if (named && step.size() > 5 + name.size() && step.compare(5, name.size(), name) == 0 &&
        step[5 + name.size()] == ':')
      out_sent_ = true;
    return false;
  }
  if (step.find('|') != std::string::npos || step.rfind("macro:", 0) == 0)
    return false;
  // A host by number is one of this keyboard's: the numbers in its macros are
  // its own, and the linked keyboard's are peer:<name>:switch_host:N. So
  // switch_host:N starts a visit — what follows is for that host and runs here,
  // until switch_host:back hands the rest back. A macro that visits host 5 does
  // so whichever keyboard the tab drives. forget_host:N is this keyboard's too.
  int slot = 0;
  const bool visit = step.rfind("switch_host:", 0) == 0 && sscanf(step.c_str() + 12, "%i", &slot) == 1;
  if (visit || (step.rfind("forget_host:", 0) == 0 && sscanf(step.c_str() + 12, "%i", &slot) == 1) ||
      step.rfind("press_button:", 0) == 0 || step.rfind("ha_action:", 0) == 0) {
    flush_for_peer_(true);
    if (visit) {
      out_away_ = out_peer_;
      out_peer_ = -1;
    }
    return false;
  }
  // Everything else reaches a host — keys, text, the mouse, holds, the
  // relative host switches, panels — and so do the delay: and wait:connected
  // between them, which only keep time if they run over there with the keys.
  std::string enc;
  form_encode_append(enc, step);
  if (!out_body_.empty() && out_body_.size() + 3 + enc.size() > PEER_MAX_TEXT_BODY)
    flush_for_peer_(false);
  if (out_body_.empty()) {
    // After the first request of a run, a step that does nothing comes first.
    // Between a chain's steps that keyboard waits for text an earlier one queued,
    // so this keeps the next keys out of text the last request is still typing.
    out_body_ = out_sent_ ? "action=delay%3A0%7C" : "action=";
    out_head_ = out_tail_ = out_body_.size();
    out_tail_ms_ = 0;
  } else {
    out_body_ += "%7C";  // '|'
  }
  out_body_ += enc;
  if (step.rfind("delay:", 0) == 0) {
    // Added up for flush_for_peer_(), counting only what execute_action waits.
    int ms = 0;
    if (sscanf(step.c_str(), "delay:%i", &ms) == 1 && ms > 0 && ms <= 10000)
      out_tail_ms_ += (uint32_t) ms;
  } else {
    out_tail_ = out_body_.size();
    out_tail_ms_ = 0;
  }
  return true;
}

// Sends what a to: run has gathered. Before a step this keyboard does itself,
// the delays it ends with are waited out here instead of over there, so that
// step comes when it would have.
void EspidfBleKeyboard::flush_for_peer_(bool before_local) {
  if (out_body_.empty() || out_peer_ < 0)
    return;
  uint32_t wait = 0;
  if (before_local) {
    wait = out_tail_ms_;
    out_body_.resize(out_tail_);
  }
  if (out_body_.size() > out_head_) {
    Peer &p = peers_[out_peer_];
    // Its host or its remote's style may have changed; see run_peer_action_.
    if (peer_post_(p, "/api/ble_keyboard/press", out_body_, "the keys of a to: step") == PEER_OK)
      p.next_due_ms = (millis() + 300) | 1;
    out_sent_ = true;
  }
  out_body_.clear();
  out_head_ = out_tail_ = 0;
  out_tail_ms_ = 0;
  if (wait > 0)
    sleep_ms_(wait);
}

// Sends one request to a peer and keeps its standing up to date — shared by the
// peer: verb and by the keyboard and mouse requests passed on for a page.
EspidfBleKeyboard::PeerResult EspidfBleKeyboard::peer_post_(Peer &p, const std::string &path,
                                                            const std::string &body, const char *what) {
  if (p.down_until_ms != 0 && (int32_t) (millis() - p.down_until_ms) < 0) {
    ESP_LOGW(TAG, "Peer %s is not answering; dropped %s", p.name.c_str(), what);
    return PEER_UNREACHABLE;
  }
  // A failed allocation aborts in this build, and a press arriving in the middle
  // of a page load can find the heap nearly empty. Better one press lost, said
  // so, than the keyboard rebooting.
  const size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (block < PEER_PRESS_MIN_BLOCK) {
    ESP_LOGW(TAG, "Peer %s: no memory to send %s just now (largest free block %u B); dropped", p.name.c_str(), what,
             (unsigned) block);
    return PEER_BUSY;
  }
  const PeerResult r = peer_request_(p, true, path.c_str(), body, nullptr);
  // Only a keyboard that could not be connected to is taken as gone. A press
  // sent and not answered in time has probably run over there, and a busy one
  // is busy — neither is a reason to drop the presses behind it.
  if (r == PEER_OK)
    p.down_until_ms = 0;
  else if (r == PEER_UNREACHABLE)
    p.down_until_ms = (millis() + PEER_DOWN_MS) | 1;
  if (r == PEER_OK || r == PEER_UNREACHABLE) {
    xSemaphoreTake(peer_mutex_, portMAX_DELAY);
    p.ok = r == PEER_OK;
    xSemaphoreGive(peer_mutex_);
  }
  return r;
}

int EspidfBleKeyboard::peer_index(const std::string &name) const {
  for (size_t i = 0; i < peers_.size(); i++) {
    if (peers_[i].name == name)
      return (int) i;
  }
  return -1;
}

static std::string peer_job(int index, const char *ep) {
  std::string job(1, '\x1F');
  job += std::to_string(index);
  job += '\x1F';
  job += ep;
  job += '\x1F';
  return job;
}

bool EspidfBleKeyboard::peer_forward(int index, const std::string &ep,
                                     const std::vector<std::pair<std::string, std::string>> &params) {
  if (index < 0 || (size_t) index >= peers_.size())
    return false;
  std::string job = peer_job(index, ep.c_str());
  if (ep == "string") {
    // Kept as text, so consecutive keystrokes can be joined before they go.
    for (const auto &kv : params) {
      if (kv.first == "keys")
        job += kv.second;
    }
  } else {
    for (size_t i = 0; i < params.size(); i++) {
      if (i > 0)
        job += '&';
      job += params[i].first;
      job += '=';
      form_encode_append(job, params[i].second);
    }
  }
  return queue_action(job);
}

void EspidfBleKeyboard::peer_add_motion(int index, int dx, int dy, int scroll) {
  if (index < 0 || (size_t) index >= peers_.size() || (size_t) index >= MAX_PEERS)
    return;
  peer_dx_[index] += dx;
  peer_dy_[index] += dy;
  peer_scroll_[index] += scroll;
  // One job waiting per peer at most; what arrives while it waits or while its
  // request is out simply adds to the totals it will send.
  if (!peer_motion_queued_[index].exchange(true) && !queue_action(peer_job(index, "motion")))
    peer_motion_queued_[index] = false;
}

void EspidfBleKeyboard::flush_peer_motion_(int index) {
  // Cleared before reading, so motion arriving from here on queues a fresh job
  // rather than being lost behind this one.
  peer_motion_queued_[index] = false;
  Peer &p = peers_[index];
  // A HID report carries -127..127 a report; anything past that stays in the
  // totals for the next send.
  auto take = [](std::atomic<int32_t> &total) {
    const int32_t v = total.exchange(0);
    const int32_t c = v > 127 ? 127 : (v < -127 ? -127 : v);
    if (c != v)
      total += v - c;
    return c;
  };
  const int32_t dx = take(peer_dx_[index]), dy = take(peer_dy_[index]), sc = take(peer_scroll_[index]);
  if (dx != 0 || dy != 0) {
    peer_post_(p, "/api/ble_keyboard/mouse_move", "x=" + std::to_string(dx) + "&y=" + std::to_string(dy),
               "a mouse move");
  }
  if (sc != 0)
    peer_post_(p, "/api/ble_keyboard/mouse_scroll", "amount=" + std::to_string(sc), "a scroll");
  if ((peer_dx_[index] != 0 || peer_dy_[index] != 0 || peer_scroll_[index] != 0) &&
      !peer_motion_queued_[index].exchange(true) && !queue_action(peer_job(index, "motion")))
    peer_motion_queued_[index] = false;
}

// "\x1F<index>\x1F<endpoint>\x1F<payload>", from peer_forward() or peer_add_motion().
void EspidfBleKeyboard::run_peer_forward_(const std::string &job) {
  const size_t a = job.find('\x1F', 1);
  const size_t b = a == std::string::npos ? a : job.find('\x1F', a + 1);
  const int index = atoi(job.c_str() + 1);
  if (b == std::string::npos || index < 0 || (size_t) index >= peers_.size())
    return;
  const std::string ep = job.substr(a + 1, b - a - 1);
  if (ep == "motion") {
    flush_peer_motion_(index);
    return;
  }
  std::string body;
  if (ep == "string") {
    // Typing sends a keystroke a request; joined up with the ones already
    // waiting behind it, a burst goes as one — in order, since only the run
    // at the front of the queue is taken.
    std::string text = job.substr(b + 1);
    size_t encoded = text.size() * 3;
    const size_t head = b + 1;
    std::string *next = nullptr;
    while (encoded < PEER_MAX_TEXT_BODY && xQueuePeek(action_queue_, &next, 0) == pdTRUE && next != nullptr &&
           next->size() > head && next->compare(0, head, job, 0, head) == 0 &&
           encoded + (next->size() - head) * 3 <= PEER_MAX_TEXT_BODY &&
           xQueueReceive(action_queue_, &next, 0) == pdTRUE) {
      text.append(*next, head, std::string::npos);
      encoded += (next->size() - head) * 3;
      delete next;
    }
    body = "keys=";
    form_encode_append(body, text);
  } else {
    body = job.substr(b + 1);
  }
  // A copied macro is read back soon, which is how the page sees it arrived.
  if (peer_post_(peers_[index], "/api/ble_keyboard/" + ep, body, ep.c_str()) == PEER_OK && ep == "macro_set")
    peers_[index].next_due_ms = (millis() + 300) | 1;
  // A Host Action copied there leaves any copy of that host's list read before it
  // out of date.
  if (ep == "override_set") {
    xSemaphoreTake(peer_mutex_, portMAX_DELAY);
    peers_[index].read_have.clear();
    peers_[index].read_reply.reset();
    xSemaphoreGive(peer_mutex_);
  }
}

// Holding a repeating key on a linked keyboard's remote queues the same press
// every couple of hundred milliseconds, faster than one request each can always
// go. Identical presses waiting behind this one are taken off the queue and sent
// with it as one chain, which that keyboard runs in order — every press arrives,
// just grouped. Plain key names only: a hold, a release, a switch or a sequence
// is never merged. True when it merged, and the job is then a chain for the
// peer that must not go through execute_action().
bool EspidfBleKeyboard::coalesce_peer_presses_(std::string &job) {
  if (job.rfind("peer:", 0) != 0 || job.size() > PEER_MAX_CHAIN / 2)
    return false;
  const size_t sep = job.find(':', 5);
  if (sep == std::string::npos || sep + 1 >= job.size())
    return false;
  const std::string key = job.substr(sep + 1);
  // A Home Assistant card sends one action per movement, and a drag would fill
  // the queue before any of them had gone. The run of them waiting behind this
  // one is added up instead — stopping short of what a HID report carries, so
  // nothing is lost and the rest stay queued. (The web page's own mouse never
  // comes through here: peer_forward sums before it queues anything.)
  const bool move = key.rfind("mouse_move:", 0) == 0;
  if (move || key.rfind("mouse_scroll:", 0) == 0) {
    int x = 0, y = 0;
    if (move ? sscanf(key.c_str(), "mouse_move:%i:%i", &x, &y) != 2
             : sscanf(key.c_str(), "mouse_scroll:%i", &x) != 1)
      return false;
    const std::string head = job.substr(0, sep + 1);
    std::string *next = nullptr;
    unsigned merged = 1;
    while (xQueuePeek(action_queue_, &next, 0) == pdTRUE && next != nullptr && next->size() > head.size() &&
           next->compare(0, head.size(), head) == 0) {
      int nx = 0, ny = 0;
      const char *rest = next->c_str() + head.size();
      if (move ? sscanf(rest, "mouse_move:%i:%i", &nx, &ny) != 2 : sscanf(rest, "mouse_scroll:%i", &nx) != 1)
        break;
      if (x + nx > 127 || x + nx < -127 || y + ny > 127 || y + ny < -127)
        break;  // a report cannot carry more; the next one takes it
      if (xQueueReceive(action_queue_, &next, 0) != pdTRUE)
        break;
      delete next;
      x += nx;
      y += ny;
      merged++;
    }
    if (merged == 1)
      return false;
    job = head;
    if (move) {
      job += "mouse_move:" + std::to_string(x) + ":" + std::to_string(y);
    } else {
      job += "mouse_scroll:" + std::to_string(x);
    }
    ESP_LOGD(TAG, "Peer %s merged into one request", job.c_str());
    return true;
  }
  if (key.find_first_of(":| ") != std::string::npos || key == "release" || key == "key_release")
    return false;
  const std::string first = job;
  std::string *next = nullptr;
  unsigned merged = 1;
  while (job.size() + 1 + key.size() <= PEER_MAX_CHAIN &&
         xQueuePeek(action_queue_, &next, 0) == pdTRUE && next != nullptr && *next == first &&
         xQueueReceive(action_queue_, &next, 0) == pdTRUE) {
    delete next;
    job += '|';
    job += key;
    merged++;
  }
  if (merged > 1) {  // braced: below DEBUG, ESP_LOGD is nothing
    ESP_LOGD(TAG, "Peer press %s sent %u times in one request", first.c_str(), merged);
  }
  return merged > 1;
}

// On the action task's quiet ticks. One peer per tick, so a press queued behind
// a refresh waits on one request at most.
void EspidfBleKeyboard::refresh_peers_() {
  const uint32_t now = millis();
  const uint32_t interest = peer_interest_ms_.load();
  if (interest == 0 || now - interest > PEER_INTEREST_MS) {
    // Nobody is looking: no traffic, and give the memory back.
    if (peer_cache_live_) {
      xSemaphoreTake(peer_mutex_, portMAX_DELAY);
      for (auto &p : peers_) {
        p.state.reset();
        p.fetched_ms = 0;
        p.next_due_ms = 0;
        p.read_reply.reset();
        p.read_want.clear();
        p.read_have.clear();
      }
      xSemaphoreGive(peer_mutex_);
      peer_cache_live_ = false;
    }
    return;
  }
  // A read holds a socket, its buffers and the reply at once, for as long as
  // the other keyboard takes to answer. A page loading here asks for a
  // dozen things together, and a read landing in the middle of that ran the
  // heap out on 2026-09-19. So no read within a few seconds of a page-load
  // request, and none while memory is short; the cache just stays as it was.
  if (now - web_busy_ms_.load() < PEER_QUIET_MS) {
    // A page is loading: the cached state (~3 KB per peer) is memory its burst
    // of requests needs more. The page's first /peers asks come back empty and
    // the bars fill in a few seconds later, once the load has settled.
    if (peer_cache_live_ && now - page_load_ms_.load() < PEER_QUIET_MS) {
      xSemaphoreTake(peer_mutex_, portMAX_DELAY);
      for (auto &p : peers_) {
        p.state.reset();
        p.next_due_ms = 0;
      }
      xSemaphoreGive(peer_mutex_);
      peer_cache_live_ = false;
    }
    return;
  }
  const size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const size_t free_now = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const bool starved = block < PEER_READ_MIN_BLOCK || free_now < PEER_READ_MIN_FREE;
  if (starved != peer_starved_) {
    peer_starved_ = starved;
    if (starved)
      ESP_LOGW(TAG, "Peer reads paused: largest free block %u B, heap %u B free", (unsigned) block,
               (unsigned) free_now);
    else
      ESP_LOGI(TAG, "Peer reads resumed");
  }
  if (starved) {
    // A read the page is waiting on says why it isn't coming, once; otherwise
    // the page only times out.
    for (auto &p : peers_) {
      std::string held;
      xSemaphoreTake(peer_mutex_, portMAX_DELAY);
      if (!p.read_want.empty() && !p.read_held) {
        p.read_held = true;
        held = p.read_want;
      }
      xSemaphoreGive(peer_mutex_);
      if (!held.empty()) {
        ESP_LOGW(TAG, "Peer %s: the page's read of %s waits for memory (largest free block %u B, heap %u B free)",
                 p.name.c_str(), held.c_str(), (unsigned) block, (unsigned) free_now);
      }
    }
    return;
  }
  // A read the page is waiting on goes before the routine ones. Only once the
  // queue is empty, as every read here: a host's Host Actions read after a copy
  // to it is read after that copy's writes have all gone.
  for (auto &p : peers_) {
    xSemaphoreTake(peer_mutex_, portMAX_DELAY);
    const std::string path = p.read_want;
    xSemaphoreGive(peer_mutex_);
    if (path.empty())
      continue;
    std::string body;
    body.reserve(2048);
    const PeerResult r = peer_request_(p, false, path.c_str(), std::string(), &body);
    const bool ok = r == PEER_OK && body.size() >= 2 && body.front() == '{' && body.back() == '}';
    // peer_request_ has logged what went wrong with the request itself; this
    // says it was the page's read that failed, and catches a reply that is not
    // one JSON object.
    if (!ok) {
      ESP_LOGW(TAG, "Peer %s: the page's read of %s failed%s", p.name.c_str(), path.c_str(),
               r == PEER_OK ? " — the reply was not one JSON object" : "");
    } else {
      ESP_LOGD(TAG, "Peer %s: read %s for the page, %u B", p.name.c_str(), path.c_str(), (unsigned) body.size());
    }
    std::shared_ptr<const std::string> reply;
    if (ok)
      reply = std::make_shared<const std::string>(std::move(body));
    xSemaphoreTake(peer_mutex_, portMAX_DELAY);
    if (p.read_want == path) {  // and not asked for something else meanwhile
      p.read_want.clear();
      p.read_have = path;
      p.read_reply.swap(reply);
      p.read_failed = !ok;
      p.read_at = millis() | 1;
    }
    xSemaphoreGive(peer_mutex_);
    peer_cache_live_ = true;
    return;
  }
  for (auto &p : peers_) {
    if (p.next_due_ms != 0 && (int32_t) (now - p.next_due_ms) < 0)
      continue;
    // Sized from the last answer, so the reply lands in one allocation rather
    // than a string doubling its way up through the fragmented heap. Only this
    // task writes `state`, so reading its size here needs no lock.
    std::string body;
    body.reserve(p.state ? p.state->size() + 128 : 2048);
    const PeerResult r = peer_request_(p, false, "/api/ble_keyboard/state", std::string(), &body);
    // Embedded as-is in /peers, so anything that is not one JSON object is
    // treated as no answer rather than passed on to break the page.
    const bool ok = r == PEER_OK && body.size() >= 2 && body.front() == '{' && body.back() == '}';
    const uint32_t done = millis();
    // Not answering means two reads in a row went unanswered: one slow reply
    // from a keyboard whose radio is shared with Bluetooth is not an outage,
    // and greying it for that is what made it flicker offline. A 409 is it
    // short of memory for a moment, and never counts.
    if (ok)
      p.read_fails = 0;
    else if (r != PEER_BUSY && p.read_fails < 255)
      p.read_fails++;
    // Built outside the lock; the old state is let go inside it, and freed
    // only once a /peers reply still sending it has finished.
    std::shared_ptr<const std::string> fresh;
    if (ok)
      fresh = std::make_shared<const std::string>(std::move(body));
    xSemaphoreTake(peer_mutex_, portMAX_DELAY);
    if (ok) {
      p.state.swap(fresh);
      p.fetched_ms = done | 1;
      p.ok = true;
    } else if (p.read_fails >= 2) {
      p.ok = false;
    }
    xSemaphoreGive(peer_mutex_);
    // After a miss, try again soon, then back off once it really is gone.
    const uint32_t wait = ok ? PEER_POLL_MS : (p.read_fails < 3 ? 2000 : PEER_RETRY_MS);
    p.next_due_ms = (done + wait) | 1;
    if (ok)
      p.down_until_ms = 0;
    peer_cache_live_ = true;
    return;
  }
}

// The web task's side of a read for the page: what has arrived, or a note of
// what to fetch. One per keyboard at a time — asking for another path replaces
// it, so the page asks for them one after another.
int EspidfBleKeyboard::peer_read(int index, const std::string &path, bool fresh,
                                 std::shared_ptr<const std::string> &out) {
  if (index < 0 || (size_t) index >= peers_.size())
    return -1;
  int r = 0;
  xSemaphoreTake(peer_mutex_, portMAX_DELAY);
  Peer &p = peers_[index];
  if (p.read_want == path) {
    // Already on its way.
  } else if (!fresh && p.read_have == path && p.read_reply && millis() - p.read_at < PEER_READ_KEEP_MS) {
    out = p.read_reply;
    r = 1;
  } else if (!fresh && p.read_have == path && p.read_failed) {
    // Said once; asking again reads again.
    p.read_have.clear();
    p.read_failed = false;
    r = -1;
  } else {
    p.read_want = path;
    p.read_have.clear();
    p.read_reply.reset();
    p.read_failed = false;
    p.read_held = false;
  }
  xSemaphoreGive(peer_mutex_);
  return r;
}

// Under the lock only long enough to take a reference to each state, so a slow
// client of /peers never holds up the action task's next swap.
void EspidfBleKeyboard::peer_snapshot(std::vector<PeerSnapshot> &out) {
  const uint32_t now = millis();
  out.clear();
  out.reserve(peers_.size());
  xSemaphoreTake(peer_mutex_, portMAX_DELAY);
  for (const auto &p : peers_) {
    out.push_back({p.name.c_str(), p.ok,
                   p.fetched_ms != 0 ? (int32_t) ((now - p.fetched_ms) / 1000) : -1, p.state});
  }
  xSemaphoreGive(peer_mutex_);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome

#endif  // USE_BLE_KB_PEERS
