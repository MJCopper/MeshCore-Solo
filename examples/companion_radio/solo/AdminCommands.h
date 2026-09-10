#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace solo {
namespace admin {

enum Group { STATUS, SETTINGS, RADIO, REPEATER, ROOM, CONSOLE, ACTIONS, GROUP_COUNT };
enum TargetKind { TARGET_REPEATER, TARGET_ROOM, TARGET_SENSOR };
enum Kind { READ, TEXT, NUMBER, TOGGLE, FREQUENCY, BANDWIDTH, SF, CR, ACTION };
struct Field {
  Group group;
  const char* label;
  const char* get;
  const char* set;
  Kind kind;
  float min, max, step;
};
static const char* const GROUP_LABELS[] = {
  "Status", "Settings", "Radio", "Repeater", "Room", "Console", "Actions"
};
// Commands verified against CommonCLI in the MeshCore baseline. Keep this
// table separate from rendering and transport; unsupported replies stay raw.
static const Field FIELDS[] = {
  {STATUS, "Version", "ver", nullptr, READ, 0, 0, 0},
  {STATUS, "Board", "board", nullptr, READ, 0, 0, 0},
  {STATUS, "Clock", "clock", nullptr, READ, 0, 0, 0},
  {SETTINGS, "Name", "get name", "set name", TEXT, 0, 31, 0},
  {SETTINGS, "Owner info", "get owner.info", "set owner.info", TEXT, 0, 119, 0},
  {SETTINGS, "Local advert (min)", "get advert.interval", "set advert.interval", NUMBER, 0, 240, 2},
  {SETTINGS, "Flood advert (hr)", "get flood.advert.interval", "set flood.advert.interval", NUMBER, 0, 168, 1},
  {RADIO, "Frequency (MHz)", "get radio", "set radio", FREQUENCY, 150, 2500, 0.001f},
  {RADIO, "Bandwidth (kHz)", "get radio", "set radio", BANDWIDTH, 0, 0, 0},
  {RADIO, "Spreading factor", "get radio", "set radio", SF, 5, 12, 1},
  {RADIO, "Coding rate", "get radio", "set radio", CR, 5, 8, 1},
  {RADIO, "TX power (dBm)", "get tx", "set tx", NUMBER, -9, 30, 1},
  {REPEATER, "Forwarding", "get repeat", "set repeat", TOGGLE, 0, 1, 1},
  {REPEATER, "Max flood hops", "get flood.max", "set flood.max", NUMBER, 0, 64, 1},
  {REPEATER, "RX delay", "get rxdelay", "set rxdelay", NUMBER, 0, 20, 0.1f},
  {REPEATER, "TX flood delay", "get txdelay", "set txdelay", NUMBER, 0, 2, 0.1f},
  {ROOM, "Guest password", "get guest.password", "set guest.password", TEXT, 0, 15, 0},
  {ROOM, "Read-only access", "get allow.read.only", "set allow.read.only", TOGGLE, 0, 1, 1},
  {ACTIONS, "Send advert", "advert", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Zero-hop advert", "advert.zerohop", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Start OTA", "start ota", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Reboot", "reboot", nullptr, ACTION, 0, 0, 0},
};
static const int FIELD_COUNT = sizeof(FIELDS) / sizeof(FIELDS[0]);

inline bool groupAllowed(TargetKind target, Group group) {
  if (group == REPEATER) return target == TARGET_REPEATER;
  if (group == ROOM) return target == TARGET_ROOM;
  return true;
}
inline int groupCount(TargetKind target) {
  int count = 0;
  for (int group = 0; group < GROUP_COUNT; group++)
    if (groupAllowed(target, (Group)group)) count++;
  return count;
}
inline Group groupAt(TargetKind target, int visible_index) {
  for (int group = 0; group < GROUP_COUNT; group++) {
    if (!groupAllowed(target, (Group)group)) continue;
    if (visible_index-- == 0) return (Group)group;
  }
  return STATUS;
}

inline bool radioTuple(Kind kind) { return kind >= FREQUENCY && kind <= CR; }
inline const char* value(const char* reply) {
  return reply && reply[0] == '>' && reply[1] == ' ' ? reply + 2 : nullptr;
}
inline bool number(const char* text, float min, float max, float& result) {
  char* end;
  float parsed = strtof(text, &end);
  if (end == text || *end || !isfinite(parsed) || parsed < min || parsed > max) return false;
  result = parsed;
  return true;
}
inline bool parseRadio(const char* text, float& freq, float& bw, uint8_t& sf, uint8_t& cr) {
  int s, c, end = 0;
  float f, b;
  if (sscanf(text, "%f,%f,%d,%d%n", &f, &b, &s, &c, &end) != 4 || text[end] ||
      !isfinite(f) || !isfinite(b) || f < 150 || f > 2500 || b < 7 || b > 500 ||
      s < 5 || s > 12 || c < 5 || c > 8) return false;
  freq = f; bw = b; sf = (uint8_t)s; cr = (uint8_t)c;
  return true;
}
inline bool formatRadio(char* out, size_t size, float f, float b, uint8_t s, uint8_t c) {
  int n = snprintf(out, size, "%.3f,%.3f,%u,%u", f, b, s, c);
  return n >= 0 && (size_t)n < size;
}
inline bool formatCommand(char* out, size_t size, const char* prefix, const char* val) {
  int n = snprintf(out, size, "%s %s", prefix, val);
  return n >= 0 && (size_t)n < size;
}
inline bool confirmed(const char* reply) {
  return strcmp(reply, "OK") == 0 || strncmp(reply, "OK - ", 5) == 0;
}
inline float stepNumber(const Field& field, float value, int direction) {
  float next = value + direction * field.step;
  // Zero disables advertising; positive intervals have baseline minima.
  float first = !strcmp(field.get, "get advert.interval") ? 60 :
                (!strcmp(field.get, "get flood.advert.interval") ? 3 : 0);
  if (first && next > 0 && next < first) next = direction > 0 ? first : 0;
  return next >= field.min && next <= field.max ? next : value;
}

} // namespace admin
} // namespace solo
