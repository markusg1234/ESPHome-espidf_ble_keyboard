// Screen keyboards: typing on an app that only takes a remote.
//
// Some apps on a TV draw their own search keyboard — a grid of letters walked
// with the arrows — and ignore a Bluetooth keyboard's letters altogether. For
// those, text is spelled: for each character, the arrows that take the
// highlight from the key it is on to that character's key, then ok.
//
// A screen keyboard describes one app's grid and belongs to a host. It is made
// on the web page and kept in storage, each a blob of its own, read when it is
// asked for: nothing of one sits in RAM until it is in use. `screen_search`
// runs the action that reaches the search screen of the app last opened — the
// host's station — and arms that app's grid; from then on text for the host is
// spelled instead of typed, until something says the search screen has been left.
//
// It is blind. Nothing comes back from the host, so where the highlight is has
// to be assumed: from the grid's start key when it is armed, then from every
// arrow sent since, by a spell or by hand. A press the app drops puts everything
// after it on the wrong key, which is what the gap between presses is for, and
// pressing screen_search again starts over from a place that is known.
#include "espidf_ble_keyboard.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *const TAG = "espidf_ble_keyboard.screen";

using Grid = EspidfBleKeyboard::ScreenGrid;
using Cfg = EspidfBleKeyboard::ScreenKeysCfg;

// What a cell of the grid holds. A key that types a character holds the
// character; these are the rest. None can be mistaken for one: a character below
// 0x21 or above 0xFFFD is never a key.
static const uint16_t SK_WALL = 0;         // "." — no key there, and no way through
static const uint16_t SK_BKSP = 0x08;      // "<" — the grid's Backspace
static const uint16_t SK_SPACE = 0x20;     // "_" — its Space
static const uint16_t SK_MORE = 0xFFFE;    // "=" — more of the key to its left
static const uint16_t SK_UNUSED = 0xFFFF;  // "#" — a key, but never one to press

static const uint8_t SK_MAX_ROWS = 12, SK_MAX_COLS = 40;
// A record with every field full: name, app key, action, grid, and the rest.
static const size_t SK_RECORD_MAX = 720;
static const char SK_FIELD = '\x1F';
static const uint32_t SK_IDLE_MS = 600000;

enum : uint8_t { SK_UP, SK_DOWN, SK_LEFT, SK_RIGHT };
static const char *const SK_DIR[] = {"up", "down", "left", "right"};

// One codepoint, i moved past it. Lenient: a broken sequence comes back as
// U+FFFF, which is never a key.
static uint32_t sk_utf8(const std::string &s, size_t &i) {
  const uint8_t b = (uint8_t) s[i++];
  if (b < 0x80)
    return b;
  const int more = (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
  if (more < 0)
    return 0xFFFF;
  uint32_t cp = b & (0x3F >> more);
  for (int n = 0; n < more; n++) {
    if (i >= s.size() || ((uint8_t) s[i] & 0xC0) != 0x80)
      return 0xFFFF;
    cp = (cp << 6) | ((uint8_t) s[i++] & 0x3F);
  }
  return cp;
}

// One key as it is written in the Keys box: its cell code, i moved past it.
// A backslash makes the next character itself, for a grid with a real "_" key.
static bool sk_symbol(const std::string &s, size_t &i, uint16_t &code) {
  uint32_t cp = sk_utf8(s, i);
  if (cp == '\\') {
    if (i >= s.size())
      return false;
    cp = sk_utf8(s, i);
  } else {
    switch (cp) {
      case '_': code = SK_SPACE; return true;
      case '<': code = SK_BKSP; return true;
      case '.': code = SK_WALL; return true;
      case '#': code = SK_UNUSED; return true;
      case '=': code = SK_MORE; return true;
    }
  }
  if (cp < 0x21 || cp == 0x7F || cp > 0xFFFD)
    return false;
  code = (uint16_t) cp;
  return true;
}

static bool sk_types(uint16_t code) { return code != SK_WALL && code != SK_MORE && code != SK_UNUSED; }

// The Keys box, a line a row, blanks between keys ignored. Without `cell` it is
// only measured, and refused if it cannot be a grid; with it, the cells are
// filled in — over gaps, so a short row ends in them. nullptr, or what is wrong.
static const char *sk_rows(const std::string &text, uint8_t &rows, uint8_t &cols, uint16_t *cell) {
  uint8_t r = 0, widest = 0;
  size_t i = 0;
  while (i < text.size()) {
    uint8_t c = 0;
    uint16_t before = SK_WALL;
    while (i < text.size() && text[i] != '\n') {
      if (text[i] == ' ' || text[i] == '\t' || text[i] == '\r') {
        i++;
        continue;
      }
      uint16_t code;
      if (!sk_symbol(text, i, code))
        return "Keys: a row has something that cannot be a key";
      if (code == SK_MORE && before == SK_WALL)
        return "Keys: = widens the key before it, so it cannot begin a row or follow a gap";
      if (r >= SK_MAX_ROWS || c >= SK_MAX_COLS)
        return "Keys: at most 12 rows of 40 keys";
      if (cell != nullptr)
        cell[(size_t) r * cols + c] = code;
      before = code;
      c++;
    }
    if (i < text.size())
      i++;   // the line break
    if (c == 0)
      continue;   // an empty line is not a row
    if (c > widest)
      widest = c;
    r++;
  }
  if (cell == nullptr) {
    if (r == 0)
      return "Keys: the grid is empty";
    if ((unsigned) r * widest > EspidfBleKeyboard::MAX_SCREEN_CELLS)
      return "Keys: at most 200 keys, gaps counted";
    rows = r;
    cols = widest;
  }
  return nullptr;
}

static int sk_find(const Grid &g, uint16_t code) {
  for (int i = 0; i < g.rows * g.cols; i++)
    if (g.cell[i] == code)
      return i;
  return -1;
}

// The cell of the key that types a character, or -1. Space and Backspace are
// the grid's own keys; a letter the grid has only in the other case is that key.
static int sk_find_char(const Grid &g, uint32_t cp) {
  if (cp == ' ')
    return sk_find(g, SK_SPACE);
  if (cp == '\b')
    return sk_find(g, SK_BKSP);
  if (cp < 0x21 || cp == 0x7F || cp > 0xFFFD)
    return -1;
  const int at = sk_find(g, (uint16_t) cp);
  if (at >= 0)
    return at;
  const bool ascii = (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
  const bool latin = cp >= 0xC0 && cp <= 0xFE && cp != 0xD7 && cp != 0xF7;
  return ascii || latin ? sk_find(g, (uint16_t) (cp ^ 0x20)) : -1;
}

// A grid from what was saved: one block holding it, its cells and the
// path-finder's working room, the highlight on its start key. nullptr and `why`
// when it cannot be one. free() lets it go.
static Grid *sk_build(const Cfg &k, const char *&why) {
  uint8_t rows = 0, cols = 0;
  why = sk_rows(k.rows, rows, cols, nullptr);
  if (why != nullptr)
    return nullptr;
  const size_t n = (size_t) rows * cols;
  auto *g = static_cast<Grid *>(calloc(1, sizeof(Grid) + n * 6));
  if (g == nullptr) {
    why = "Low on memory, try again shortly";
    return nullptr;
  }
  g->cell = reinterpret_cast<uint16_t *>(g + 1);   // zeroed: every cell a gap
  g->work = reinterpret_cast<uint8_t *>(g->cell + n);
  g->rows = rows;
  g->cols = cols;
  sk_rows(k.rows, rows, cols, g->cell);
  int first = -1, at = -1;
  for (int i = 0; i < (int) n && first < 0; i++)
    if (sk_types(g->cell[i]))
      first = i;
  if (first < 0) {
    why = "Keys: none of them types anything";
  } else if (k.start.empty()) {
    at = first;
  } else {
    size_t i = 0;
    uint16_t code = SK_WALL;
    if (sk_symbol(k.start, i, code) && i == k.start.size() && code != SK_WALL && code != SK_MORE)
      at = sk_find(*g, code);
    if (at < 0)
      why = "Starts on: one key from Keys, written as it is there";
  }
  if (at < 0) {
    free(g);
    return nullptr;
  }
  strncpy(g->name, k.name.c_str(), sizeof(g->name) - 1);
  g->gap = k.gap;
  g->wide = k.wide;
  g->r = (uint8_t) (at / cols);
  g->c = (uint8_t) (at % cols);
  return g;
}

// The columns a key covers: itself, and every "=" after it.
static void sk_span(const Grid &g, uint8_t r, uint8_t c, uint8_t &c1, uint8_t &c2) {
  const uint16_t *row = &g.cell[(size_t) r * g.cols];
  c1 = c;
  while (c1 > 0 && row[c1] == SK_MORE)
    c1--;
  c2 = c;
  while (c2 + 1 < g.cols && row[c2 + 1] == SK_MORE)
    c2++;
}

// Where one arrow takes the highlight from (r,c), or false when there is no key
// that way. Across, it steps off the end of the key it is on. Up and down, a
// wide key leaves from the column the grid's rule names: its first, its middle,
// or the one it was come to by — which is why c is kept for a wide key at all.
// Never round the edge: an app may wrap there or may leave its keyboard, and
// neither is something to rely on.
static bool sk_move(const Grid &g, uint8_t r, uint8_t c, uint8_t dir, uint8_t &nr, uint8_t &nc) {
  uint8_t c1, c2;
  sk_span(g, r, c, c1, c2);
  int tr = r, tc;
  if (dir == SK_LEFT) {
    tc = (int) c1 - 1;
  } else if (dir == SK_RIGHT) {
    tc = (int) c2 + 1;
  } else {
    tr += dir == SK_UP ? -1 : 1;
    tc = g.wide == 0 ? c1 : g.wide == 1 ? (c1 + c2) / 2 : c;
  }
  if (tr < 0 || tr >= g.rows || tc < 0 || tc >= g.cols || g.cell[(size_t) tr * g.cols + tc] == SK_WALL)
    return false;
  uint8_t t1, t2;
  sk_span(g, (uint8_t) tr, (uint8_t) tc, t1, t2);
  nr = (uint8_t) tr;
  nc = g.wide != 2 ? t1 : dir == SK_LEFT ? t2 : dir == SK_RIGHT ? t1 : (uint8_t) tc;
  return true;
}

// The fewest arrows from where the highlight is to the key at cell `goal`,
// breadth first, and where that leaves it. How many, or -1 for no way there;
// the arrows themselves are left in the last quarter of g.work. There are at
// most 200 places to be, so a place fits a byte.
static int sk_path(Grid &g, int goal, uint8_t &er, uint8_t &ec) {
  const int n = g.rows * g.cols;
  uint8_t *via = g.work, *from = via + n, *todo = from + n, *path = todo + n;
  memset(via, 0xFF, n);
  const uint8_t start = (uint8_t) (g.r * g.cols + g.c);
  via[start] = 0xFE;
  todo[0] = start;
  for (int head = 0, tail = 1; head < tail; head++) {
    const uint8_t at = todo[head], r = at / g.cols, c = at % g.cols;
    uint8_t c1, c2;
    sk_span(g, r, c, c1, c2);
    if (r * g.cols + c1 == goal) {
      int len = 0;
      for (uint8_t p = at; p != start; p = from[p])
        len++;
      int i = len;
      for (uint8_t p = at; p != start; p = from[p])
        path[--i] = via[p];
      er = r;
      ec = c;
      return len;
    }
    for (uint8_t dir = 0; dir < 4; dir++) {
      uint8_t nr, nc;
      if (!sk_move(g, r, c, dir, nr, nc))
        continue;
      const uint8_t next = (uint8_t) (nr * g.cols + nc);
      if (via[next] != 0xFF)
        continue;
      via[next] = dir;
      from[next] = at;
      todo[tail++] = next;
    }
  }
  return -1;
}

static bool sk_name_ok(const std::string &name) {
  if (name.empty() || name.size() > 15 || name == "off")
    return false;
  for (char ch : name)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_'))
      return false;
  return true;
}

// ── Storage ─────────────────────────────────────────────────────────────────
// A blob to a screen keyboard, sk<slot>_<place>: its fields in the order of
// get_screen_keys below, a 0x1F between them. Six places to a host.

static const char *const SK_NVS = "espidf_ble_kb";

static void sk_key(char *key, uint8_t slot, uint8_t index) {
  snprintf(key, 12, "sk%u_%u", (unsigned) slot, (unsigned) index);
}

static bool sk_read(uint8_t slot, uint8_t index, std::string &blob) {
  blob.clear();
  nvs_handle_t handle;
  if (nvs_open(SK_NVS, NVS_READONLY, &handle) != ESP_OK)
    return false;   // nothing has ever been stored
  char key[12];
  sk_key(key, slot, index);
  size_t len = 0;
  bool ok = false;
  if (nvs_get_blob(handle, key, nullptr, &len) == ESP_OK && len > 0 && len <= SK_RECORD_MAX) {
    // Read whole and then taken apart, so twice its size for a moment.
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 2 * len + 1024) {
      ESP_LOGW(TAG, "A screen keyboard (%u B) not read: low on memory", (unsigned) len);
    } else {
      blob.resize(len);
      ok = nvs_get_blob(handle, key, &blob[0], &len) == ESP_OK;
    }
  }
  nvs_close(handle);
  return ok;
}

// Writes a place, or with no blob empties it.
static bool sk_write(uint8_t slot, uint8_t index, const std::string *blob) {
  nvs_handle_t handle;
  if (nvs_open(SK_NVS, NVS_READWRITE, &handle) != ESP_OK)
    return false;
  char key[12];
  sk_key(key, slot, index);
  bool ok = true;
  if (blob == nullptr)
    nvs_erase_key(handle, key);
  else
    ok = nvs_set_blob(handle, key, blob->data(), blob->size()) == ESP_OK;
  ok = nvs_commit(handle) == ESP_OK && ok;
  nvs_close(handle);
  return ok;
}

size_t EspidfBleKeyboard::screen_keys_size(uint8_t slot) const {
  size_t total = 0;
  nvs_handle_t handle;
  if (nvs_open(SK_NVS, NVS_READONLY, &handle) != ESP_OK)
    return 0;
  for (uint8_t i = 0; i < MAX_SCREEN_KEYS; i++) {
    char key[12];
    sk_key(key, slot, i);
    size_t len = 0;
    if (nvs_get_blob(handle, key, nullptr, &len) == ESP_OK)
      total += len;
  }
  nvs_close(handle);
  return total;
}

bool EspidfBleKeyboard::get_screen_keys(uint8_t slot, uint8_t index, ScreenKeysCfg &out) const {
  std::string blob;
  if (slot >= MAX_HOST_SLOTS || index >= MAX_SCREEN_KEYS || !sk_read(slot, index, blob))
    return false;
  // name, app, open, gap, wide, start, rows — the last runs to the end.
  std::string *text[7] = {&out.name, &out.app, &out.open, nullptr, nullptr, &out.start, &out.rows};
  size_t p = 0;
  for (int n = 0; n < 7; n++) {
    const size_t q = n < 6 ? blob.find(SK_FIELD, p) : blob.size();
    if (q == std::string::npos)
      return false;
    if (text[n] != nullptr)
      text[n]->assign(blob, p, q - p);
    else if (n == 3)
      out.gap = (uint16_t) atoi(blob.c_str() + p);   // stops at the separator
    else
      out.wide = (uint8_t) atoi(blob.c_str() + p);
    p = q + 1;
  }
  return sk_name_ok(out.name);
}

// The place holding the host's screen keyboard of that name, or -1; and, where
// asked, the first place holding none.
int EspidfBleKeyboard::find_screen_keys_(uint8_t slot, const std::string &name, int *spare) const {
  int at = -1;
  if (spare != nullptr)
    *spare = -1;
  std::string blob;
  for (uint8_t i = 0; i < MAX_SCREEN_KEYS; i++) {
    if (!sk_read(slot, i, blob)) {
      if (spare != nullptr && *spare < 0)
        *spare = i;
    } else if (at < 0 && blob.size() > name.size() && blob[name.size()] == SK_FIELD &&
               blob.compare(0, name.size(), name) == 0) {
      at = i;
    }
  }
  return at;
}

const char *EspidfBleKeyboard::set_screen_keys(uint8_t slot, const std::string &was, const ScreenKeysCfg &k) {
  if (slot >= host_slots_)
    return "Invalid slot";
  if (!sk_name_ok(k.name))
    return "Name: 1 to 15 of a-z, 0-9 and _, and not off";
  if (!k.app.empty() && !is_spare_action(is_long_name(k.app) ? k.app.substr(0, k.app.size() - 5) : k.app))
    return "App key: a spare key such as spare3 or spare3@long, or empty";
  if (k.open.size() > MAX_ACTION_LEN || k.open.find_first_of("\r\n\x1F") != std::string::npos)
    return "Open search: an action of up to 255 characters, on one line";
  if (k.rows.size() > MAX_SCREEN_ROWS_LEN || k.rows.find(SK_FIELD) != std::string::npos)
    return "Keys: at most 400 characters";
  if (k.gap < 50 || k.gap > 2000)
    return "Press gap: 50 to 2000 ms";
  if (k.wide > 2)
    return "Wide keys: 0, 1 or 2";
  const char *why = k.start.size() > 8 ? "Starts on: one key from Keys, written as it is there" : nullptr;
  Grid *g = why == nullptr ? sk_build(k, why) : nullptr;
  if (g == nullptr)
    return why;
  free(g);

  // In the place of the one it was before the edit, else of one of its own
  // name, else in the first place free. Renamed onto another's name, it takes
  // that one's place in the list too.
  int spare = -1;
  const int own = find_screen_keys_(slot, k.name, &spare);
  const int old = was.empty() || was == k.name ? -1 : find_screen_keys_(slot, was, nullptr);
  const int at = old >= 0 ? old : own >= 0 ? own : spare;
  if (at < 0)
    return "A host holds 6 screen keyboards";
  char num[16];
  snprintf(num, sizeof(num), "%c%u%c%u%c", SK_FIELD, (unsigned) k.gap, SK_FIELD, (unsigned) k.wide, SK_FIELD);
  std::string blob;
  blob.reserve(k.name.size() + k.app.size() + k.open.size() + k.start.size() + k.rows.size() + 16);
  blob += k.name;
  blob += SK_FIELD;
  blob += k.app;
  blob += SK_FIELD;
  blob += k.open;
  blob += num;
  blob += k.start;
  blob += SK_FIELD;
  blob += k.rows;
  if (!sk_write(slot, (uint8_t) at, &blob))
    return "Could not save the screen keyboard to storage";
  if (old >= 0 && own >= 0 && own != old)
    sk_write(slot, (uint8_t) own, nullptr);
  // The one in use may be the one just changed.
  if (skb_slot_.load() == (int8_t) slot)
    skb_disarm_("its host's screen keyboards were edited");
  return nullptr;
}

bool EspidfBleKeyboard::delete_screen_keys(uint8_t slot, const std::string &name) {
  const int at = slot < host_slots_ ? find_screen_keys_(slot, name, nullptr) : -1;
  if (at < 0 || !sk_write(slot, (uint8_t) at, nullptr))
    return false;
  if (skb_slot_.load() == (int8_t) slot)
    skb_disarm_("its host's screen keyboards were edited");
  return true;
}

// ── In use ──────────────────────────────────────────────────────────────────

std::string EspidfBleKeyboard::screen_keys_armed() const {
  std::string name;
  if (skb_mutex_ == nullptr || skb_slot_.load() != (int8_t) active_slot_)
    return name;
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  if (skb_ != nullptr)
    name = skb_->name;
  xSemaphoreGive(skb_mutex_);
  return name;
}

// Armed for the host that is active, and still worth believing. A host that
// has gone, and a search screen nobody has touched for ten minutes, are let go:
// the highlight is no longer where it was left.
bool EspidfBleKeyboard::skb_live_() {
  const int8_t slot = skb_slot_.load();
  if (slot < 0 || slot != (int8_t) active_slot_)
    return false;
  if (slot_broadcasts(active_slot_) && !is_connected_) {
    skb_disarm_("its host disconnected");
    return false;
  }
  if (millis() - skb_used_ms_ > SK_IDLE_MS) {
    skb_disarm_("unused for ten minutes");
    return false;
  }
  return true;
}

void EspidfBleKeyboard::skb_arm_(uint8_t slot, ScreenGrid *grid) {
  if (skb_mutex_ == nullptr) {
    free(grid);
    return;
  }
  ESP_LOGI(TAG, "Screen keyboard %s in use on slot %u: text is spelled with the arrows", grid->name, (unsigned) slot);
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  free(skb_);
  skb_ = grid;
  skb_used_ms_ = millis();
  skb_slot_.store((int8_t) slot);
  xSemaphoreGive(skb_mutex_);
  pending_lcd_publish_.store(true);
}

void EspidfBleKeyboard::skb_disarm_(const char *why) {
  if (skb_mutex_ == nullptr || skb_slot_.load() < 0)
    return;
  char name[sizeof(skb_->name)] = "";
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  if (skb_ != nullptr)
    memcpy(name, skb_->name, sizeof(name));
  free(skb_);
  skb_ = nullptr;
  skb_slot_.store(-1);
  // What was still waiting to be spelled was for this grid, and is not typed
  // as keys in its place.
  std::string().swap(skb_text_);
  xSemaphoreGive(skb_mutex_);
  pending_lcd_publish_.store(true);
  ESP_LOGI(TAG, "Screen keyboard %s let go: %s", name, why);
}

bool EspidfBleKeyboard::screen_keys_busy(size_t more) {
  if (skb_mutex_ == nullptr || skb_slot_.load() != (int8_t) active_slot_)
    return false;
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  // Whatever its length, text is taken when nothing is waiting.
  const bool busy = !skb_text_.empty() && skb_text_.size() + more > MAX_SPELL_BACKLOG;
  xSemaphoreGive(skb_mutex_);
  return busy;
}

// Text for the action task to spell, behind whatever is already waiting there.
void EspidfBleKeyboard::skb_queue_(const std::string &text) {
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  const bool room = skb_text_.empty() || skb_text_.size() + text.size() <= MAX_SPELL_BACKLOG;
  if (room)
    skb_text_ += text;
  xSemaphoreGive(skb_mutex_);
  if (!room) {
    ESP_LOGW(TAG, "Still spelling: \"%s\" was not taken", text.c_str());
    return;
  }
  if (skb_job_.exchange(true) || queue_action(std::string(1, SPELL_JOB)))
    return;
  // The action queue is full, so nothing will come for it.
  skb_job_.store(false);
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  skb_text_.clear();
  xSemaphoreGive(skb_mutex_);
}

// The action task's end of that: spells what has gathered, and again whatever
// arrived meanwhile, until there is none.
void EspidfBleKeyboard::skb_spell_queued_() {
  std::string text;
  while (true) {
    xSemaphoreTake(skb_mutex_, portMAX_DELAY);
    text.swap(skb_text_);
    if (text.empty())
      skb_job_.store(false);
    xSemaphoreGive(skb_mutex_);
    if (text.empty())
      return;
    skb_spell_(text);
    text.clear();
  }
}

// A key pressed by hand while a grid is in use — the outermost press only, and
// never one of a spell's own. An arrow moves the highlight this keyboard
// believes in; one that would take it off the keys, and any key that leaves a
// search screen, end the belief.
void EspidfBleKeyboard::skb_note_press_(const std::string &pressed) {
  int dir = -1;
  for (int d = 0; d < 4; d++)
    if (pressed == SK_DIR[d])
      dir = d;
  if (dir < 0) {
    if (pressed == "back" || pressed == "home" || pressed == "exit" || pressed == "remote_power" ||
        is_spare_action(is_long_name(pressed) ? pressed.substr(0, pressed.size() - 5) : pressed))
      skb_disarm_("a key that leaves the search screen was pressed");
    return;
  }
  if (skb_slot_.load() != (int8_t) active_slot_)
    return;
  bool off = false;
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  if (skb_ != nullptr) {
    uint8_t r, c;
    if (sk_move(*skb_, skb_->r, skb_->c, (uint8_t) dir, r, c)) {
      skb_->r = r;
      skb_->c = c;
      skb_used_ms_ = millis();
    } else {
      off = true;
    }
  }
  xSemaphoreGive(skb_mutex_);
  if (off)
    skb_disarm_("the highlight was moved off its keys");
}

// One press of a spell, as the remote's own key would be: through the host's
// Host Actions, whatever called for the text. A press from inside a Host Action
// would otherwise find them switched off (override_depth_).
void EspidfBleKeyboard::skb_press_(const char *name) {
  const uint8_t depth = override_depth_;
  override_depth_ = 0;
  execute_action(name);
  override_depth_ = depth;
}

// Spells text on the grid in use. On the action task, or inside a chain that is
// already holding its caller: a word is seconds of presses.
__attribute__((noinline)) void EspidfBleKeyboard::skb_spell_(const std::string &text) {
  if (!skb_live_()) {
    ESP_LOGW(TAG, "The screen keyboard was let go before \"%s\" was spelled", text.c_str());
    return;
  }
  // Its own presses are not ones made by hand (skb_note_press_).
  skb_spelling_.store(true);
  unsigned spelled = 0, skipped = 0;
  size_t i = 0;
  while (i < text.size()) {
    const uint32_t cp = sk_utf8(text, i);
    if (cp == '\n' || cp == '\r' || cp == '\t')
      continue;
    // The arrows, copied out from under the lock: the grid can be let go of by
    // another task between two presses.
    uint8_t path[MAX_SCREEN_CELLS];
    int len = -1;
    uint16_t gap = 0;
    xSemaphoreTake(skb_mutex_, portMAX_DELAY);
    if (skb_ != nullptr) {
      gap = skb_->gap;
      const int goal = sk_find_char(*skb_, cp);
      uint8_t r, c;
      if (goal >= 0 && (len = sk_path(*skb_, goal, r, c)) >= 0) {
        memcpy(path, skb_->work + 3 * skb_->rows * skb_->cols, len);
        // Where it will be once these have gone. A spell cut short is let go
        // of below, so this is never left ahead of the screen.
        skb_->r = r;
        skb_->c = c;
      }
    }
    xSemaphoreGive(skb_mutex_);
    if (gap == 0)
      break;   // let go meanwhile
    if (len < 0) {
      skipped++;
      continue;
    }
    for (int p = 0; p <= len; p++) {
      if (!skb_live_()) {
        // Gone part-way through a letter — a host switch, a disconnect. If it
        // is still armed, where the highlight is can no longer be said.
        skb_disarm_("a spell was cut short");
        skb_spelling_.store(false);
        return;
      }
      skb_press_(p < len ? SK_DIR[path[p]] : "ok");
      sleep_ms_(gap);
    }
    spelled++;
  }
  skb_spelling_.store(false);
  skb_used_ms_ = millis();
  if (skipped > 0) {
    ESP_LOGI(TAG, "Spelled %u, skipped %u the grid has no key for", spelled, skipped);
  }
}

bool EspidfBleKeyboard::skb_backspace_() {
  if (!skb_live_())
    return false;
  bool has = false;
  xSemaphoreTake(skb_mutex_, portMAX_DELAY);
  if (skb_ != nullptr)
    has = sk_find(*skb_, SK_BKSP) >= 0;
  xSemaphoreGive(skb_mutex_);
  if (has)
    send_string("\b");
  return has;
}

bool EspidfBleKeyboard::screen_keys_take_key(uint8_t modifiers, uint8_t keycode) {
  if (modifiers != 0 || skb_slot_.load() < 0 || !skb_live_())
    return false;
  if (keycode == 0x28 || keycode == 0x58)
    return true;   // Enter: it would press the highlighted letter again
  return keycode == 0x2A && skb_backspace_();
}

// ── The verbs ───────────────────────────────────────────────────────────────

// screen_search — open the search screen of the app this host was last sent to,
// and spell from there. Which app that is comes from the host's station: the
// screen keyboard whose app key was the last spare pressed, else one that names
// no app key. With neither it is the ordinary search key. screen_search:<name>
// names one outright, and screen_keys:<name> (`open` false) arms one without
// going anywhere: the highlight is already on its start key.
__attribute__((noinline)) void EspidfBleKeyboard::run_screen_search_(const std::string &name, bool open) {
  const uint8_t slot = active_slot_;
  ScreenKeysCfg one, two, *cur = &one, *pick = nullptr;
  for (uint8_t i = 0; i < MAX_SCREEN_KEYS; i++) {
    if (!get_screen_keys(slot, i, *cur))
      continue;
    if (!name.empty()) {
      if (cur->name != name)
        continue;
      pick = cur;
      break;
    }
    // The one whose app is open wins over one made for any app.
    if (!cur->app.empty() && cur->app == station_[slot]) {
      pick = cur;
      break;
    }
    if (pick == nullptr && cur->app.empty()) {
      pick = cur;
      cur = cur == &one ? &two : &one;
    }
  }
  if (pick == nullptr) {
    if (!name.empty()) {
      ESP_LOGW(TAG, "No screen keyboard '%s' on slot %u", name.c_str(), (unsigned) slot);
    } else {
      ESP_LOGI(TAG, "No screen keyboard for %s on slot %u: the ordinary search",
               station_[slot][0] ? station_[slot] : "this host", (unsigned) slot);
      execute_action("search");
    }
    return;
  }
  const char *why = nullptr;
  Grid *g = sk_build(*pick, why);
  if (g == nullptr) {
    ESP_LOGW(TAG, "Screen keyboard %s not used: %s", pick->name.c_str(), why);
    return;
  }
  // Whatever was in use is not, from the first press of the way there.
  skb_disarm_("search opened again");
  if (open && !pick->open.empty())
    execute_action(pick->open);
  if (active_slot_ == slot)
    skb_arm_(slot, g);
  else
    free(g);
}

// screen_keys:<name> — spell on that one from here on. screen_keys:off — type
// as a keyboard again.
void EspidfBleKeyboard::run_screen_keys_(const std::string &arg) {
  if (arg == "off")
    skb_disarm_("turned off");
  else if (!arg.empty())
    run_screen_search_(arg, false);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome
