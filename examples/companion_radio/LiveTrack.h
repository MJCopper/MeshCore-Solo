#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// RAM-only table of positions other nodes have shared via [LOC] messages
// (see geo::parseLocShare / LOCATION_MSG_TAG). Lets the device show a live
// bearing/distance to people who broadcast their location on a DM or channel,
// independent of how often they advert.
//
// Keying:
//   DM share      → keyed by pubkey prefix, verified == true  (reliable).
//   Channel share → keyed by sender name,   verified == false (best-effort:
//                    channel sender names are unsigned and not unique).
//
// The table never persists (positions go stale fast) and entries expire after
// EXPIRY_SECS without a fresh update. When full, the oldest entry is evicted.
class LiveTrackStore {
public:
  static const int      CAPACITY    = 16;            // max simultaneously tracked nodes
  static const uint32_t EXPIRY_SECS = 20UL * 60UL;   // drop a track 20 min after its last update
  static const int      NAME_LEN    = 16;            // display name buffer (incl. NUL)
  static const int      KEY_LEN     = 6;             // pubkey prefix bytes (matches favourites)

  struct Entry {
    uint8_t  key[KEY_LEN];      // pubkey prefix (verified DM); all-zero for channel entries
    char     name[NAME_LEN];    // display name, always set
    int32_t  lat_1e6, lon_1e6;
    uint32_t ts;                // RTC epoch seconds of the share
    bool     verified;          // true = DM/pubkey, false = channel/name
    bool     used;
  };

  // Insert or refresh a shared position. `key` may be null/ignored when not
  // verified. `name` is required (used as the channel key and for display).
  void update(const uint8_t* key, const char* name,
              int32_t lat_1e6, int32_t lon_1e6, uint32_t ts, bool verified) {
    int slot = find(key, name, verified);
    if (slot < 0) slot = allocSlot();
    Entry& e = _e[slot];
    if (verified && key) memcpy(e.key, key, KEY_LEN);
    else                 memset(e.key, 0, KEY_LEN);
    if (name) { strncpy(e.name, name, NAME_LEN - 1); e.name[NAME_LEN - 1] = '\0'; }
    else      { e.name[0] = '\0'; }
    e.lat_1e6  = lat_1e6;
    e.lon_1e6  = lon_1e6;
    e.ts       = ts;
    e.verified = verified;
    e.used     = true;
  }

  // Drop entries not refreshed within EXPIRY_SECS. `now` is RTC epoch seconds.
  // Guarded against now < ts (RTC stepped backwards) so a clock fix can't wipe
  // the table.
  void expire(uint32_t now) {
    for (int i = 0; i < CAPACITY; i++) {
      if (_e[i].used && now > _e[i].ts && (now - _e[i].ts) > EXPIRY_SECS) {
        _e[i].used = false;
      }
    }
  }

  void clear() { for (int i = 0; i < CAPACITY; i++) _e[i].used = false; }

  // Slot-wise access (caller skips inactive slots via isActive()).
  const Entry& slotAt(int i) const { return _e[i]; }
  bool isActive(int i, uint32_t now) const {
    const Entry& e = _e[i];
    if (!e.used) return false;
    return !(now > e.ts && (now - e.ts) > EXPIRY_SECS);
  }

  // Number of currently non-expired entries.
  int active(uint32_t now) const {
    int n = 0;
    for (int i = 0; i < CAPACITY; i++) if (isActive(i, now)) n++;
    return n;
  }

  // Latest active position for a verified (DM/pubkey) share, or null if that
  // node hasn't shared recently. Used by the locator engine to follow a
  // moving contact.
  const Entry* activeByKey(const uint8_t* key, uint32_t now) const {
    if (!key) return nullptr;
    for (int i = 0; i < CAPACITY; i++) {
      if (!isActive(i, now)) continue;
      const Entry& e = _e[i];
      if (e.verified && memcmp(e.key, key, KEY_LEN) == 0) return &e;
    }
    return nullptr;
  }

private:
  Entry _e[CAPACITY] = {};

  // Match an existing entry: verified shares by key, channel shares by name.
  int find(const uint8_t* key, const char* name, bool verified) const {
    for (int i = 0; i < CAPACITY; i++) {
      if (!_e[i].used) continue;
      if (verified) {
        if (_e[i].verified && key && memcmp(_e[i].key, key, KEY_LEN) == 0) return i;
      } else {
        if (!_e[i].verified && name && strncmp(_e[i].name, name, NAME_LEN - 1) == 0) return i;
      }
    }
    return -1;
  }

  // First free slot, else evict the oldest (smallest ts).
  int allocSlot() {
    int oldest = 0;
    for (int i = 0; i < CAPACITY; i++) {
      if (!_e[i].used) return i;
      if (_e[i].ts < _e[oldest].ts) oldest = i;
    }
    return oldest;
  }
};
