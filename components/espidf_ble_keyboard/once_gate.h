// The parts of the once: step, and the time format it shares with after:, that
// need nothing from ESPHome: a test built for a PC includes this file as it is
// (local/once-host).
#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace espidf_ble_keyboard {

// "90", "90s", "30m", "1h30m", "2d": seconds when bare. False for anything else,
// and outside 1 s to 24 days — how far apart two millis() readings can be and
// still compare the right way round.
inline bool parse_after_time(const std::string &s, uint32_t &ms) {
  uint64_t total = 0;
  size_t i = 0;
  if (s.empty())
    return false;
  while (i < s.size()) {
    if (s[i] < '0' || s[i] > '9')
      return false;
    uint64_t n = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      n = n * 10 + (uint64_t) (s[i] - '0');
      if (n > 100000000ULL)
        return false;
      i++;
    }
    uint64_t unit = 1000;
    if (i < s.size()) {
      switch (s[i]) {
        case 's': unit = 1000; break;
        case 'm': unit = 60000; break;
        case 'h': unit = 3600000; break;
        case 'd': unit = 86400000; break;
        default: return false;
      }
      i++;
    } else if (total != 0) {
      return false;  // "1h30": a bare number only on its own
    }
    total += n * unit;
    if (total > 24ULL * 86400000ULL)
      return false;
  }
  if (total < 1000)
    return false;
  ms = (uint32_t) total;
  return true;
}

inline std::string once_trim(const std::string &s) {
  const size_t b = s.find_first_not_of(" \t");
  if (b == std::string::npos)
    return std::string();
  return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

// once:<time>:[<source>:]<action>, or once:<source>:<action> with no time,
// given what follows "once:". The token after the time is the source when
// is_source says one has that key; otherwise it starts the action, colons and
// all ("combo:0:40"). With no time, period is 0: never due by time, only when
// the source switches on. False when the first token is neither a time nor a
// source, or nothing is left to run.
template<typename IsSource>
bool split_once(const std::string &body, IsSource is_source, uint32_t &period, std::string &source,
                std::string &action) {
  const size_t c1 = body.find(':');
  if (c1 == std::string::npos)
    return false;
  const std::string first = once_trim(body.substr(0, c1));
  std::string rest = once_trim(body.substr(c1 + 1));
  source.clear();
  if (!parse_after_time(first, period)) {
    if (first.empty() || !is_source(first))
      return false;
    period = 0;
    source = first;
    action = rest;
    return !action.empty();
  }
  const size_t c2 = rest.find(':');
  const std::string head = once_trim(c2 == std::string::npos ? rest : rest.substr(0, c2));
  if (!head.empty() && is_source(head)) {
    if (c2 == std::string::npos)
      return false;  // a source, and nothing to run
    source = head;
    rest = once_trim(rest.substr(c2 + 1));
  }
  action = rest;
  return !action.empty();
}

// Where the step that starts at `start` ends: at its '|', or the end of the
// string. A once: step whose action is a group in brackets runs to the bracket
// that closes it, the bars inside belonging to the group:
//   once:20h:(delay:750 | ok) | switch_host:back
//   once:20h:panel:(delay:750 | ok) | switch_host:back
// The bracket must open the action, straight after the time or after the token
// that follows it. Brackets mean nothing in any other step, and a group that is
// never closed is not one, so everything else splits exactly as it always has.
inline size_t once_step_end(const std::string &s, size_t start) {
  const size_t plain = s.find('|', start);
  const size_t stop = plain == std::string::npos ? s.size() : plain;
  size_t i = start;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    i++;
  if (s.compare(i, 5, "once:") != 0)
    return stop;
  // Where the action may open: after the time's colon, or after one more token.
  size_t open = std::string::npos;
  size_t colon = s.find(':', i + 5);
  for (int tries = 0; tries < 2 && colon != std::string::npos && colon < stop; tries++) {
    size_t k = colon + 1;
    while (k < s.size() && (s[k] == ' ' || s[k] == '\t'))
      k++;
    if (k < s.size() && s[k] == '(') {
      open = k;
      break;
    }
    colon = s.find(':', colon + 1);
  }
  if (open == std::string::npos)
    return stop;
  int depth = 0;
  for (size_t k = open; k < s.size(); k++) {
    if (s[k] == '(') {
      depth++;
    } else if (s[k] == ')' && --depth == 0) {
      const size_t next = s.find('|', k);
      return next == std::string::npos ? s.size() : next;
    }
  }
  return stop;  // never closed: not a group
}

// A once: step's action as it runs: a group's steps without their brackets,
// anything else as it is. False for a group that is never closed, or empty.
inline bool once_unwrap(const std::string &action, std::string &out) {
  if (action.empty() || action[0] != '(') {
    out = action;
    return true;
  }
  if (action.back() != ')')
    return false;
  out = once_trim(action.substr(1, action.size() - 2));
  return !out.empty();
}

// When a once: step last ran, in milliseconds since boot, and how many times its
// source had been switched on by then. Never run: as if it ran at boot, {0, 0}
// — a restart counts as a run, so a step after a flash waits rather than pressing
// a key on a screen it cannot see.
struct OnceRec {
  uint64_t ran_ms{0};
  uint32_t ons_seen{0};
};

// A binary-sensor source's new state, counted: off to on is a switch-on. The
// first state heard after boot (last still -1) is not one, so a display already
// on when the keyboard restarted opens no once: step.
inline void once_note_state(int8_t &last, uint32_t &ons, bool on) {
  if (on && last == 0)
    ons = ons + 1;
  last = on ? 1 : 0;
}

// Due once the period has passed since it last ran (a period of 0: never by
// time), or the source has been switched on since (its count of switch-ons has
// moved). 64-bit time: no wrap.
inline bool once_due(const OnceRec &was, uint64_t now_ms, uint32_t period, bool has_source, uint32_t ons_now) {
  return (period != 0 && now_ms - was.ran_ms >= period) || (has_source && ons_now != was.ons_seen);
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome
