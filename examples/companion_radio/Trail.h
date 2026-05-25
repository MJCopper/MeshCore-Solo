#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// RAM-only GPS trail ring buffer.
// Storage cost: 256 × 12 B = 3 KB. The trail survives auto-off (only the
// display blanks) but is lost on reboot — user explicitly snapshots to a
// LittleFS slot before powering down to keep it.

struct TrailPoint {
  int32_t  lat_1e6;
  int32_t  lon_1e6;
  uint32_t ts;            // epoch seconds (RTC)
  uint8_t  flags;         // bit 0 = SEG_START (don't draw a line from the previous point)
};

static const uint8_t TRAIL_FLAG_SEG_START = 0x01;

class TrailStore {
public:
  static const int CAPACITY = 512;

  // Fixed sampling cadence — matches the sensor manager's default GPS update
  // rate (1 s). Density is controlled by the min-delta gate (settings) rather
  // than by throttling the GPS poll. The NodePrefs::trail_interval_idx field
  // is retained as reserved for backwards compatibility but no longer used.
  static const uint16_t SAMPLING_SECS = 1;

  // Min-delta (metres) gates samples too close to the previous one.
  // Default 5 m: keeps walking jitter out, dense enough for a visible trail.
  static const uint8_t MIN_DELTA_COUNT = 4;
  static uint16_t minDeltaMeters(uint8_t idx) {
    static const uint16_t OPTS[MIN_DELTA_COUNT] = { 5, 10, 25, 100 };
    return OPTS[idx < MIN_DELTA_COUNT ? idx : 0];
  }
  static const char* minDeltaLabel(uint8_t idx) {
    static const char* L[MIN_DELTA_COUNT] = { "5 m", "10 m", "25 m", "100 m" };
    return L[idx < MIN_DELTA_COUNT ? idx : 0];
  }

  bool isActive() const { return _active; }
  void setActive(bool a) {
    if (a && !_active) {
      // off → on: start a new session timer.
      _session_start_ms = millis();
    } else if (_active && !a) {
      // on → off: bank the elapsed of this session and arm a segment break
      // so the renderer doesn't draw a straight line through the dead time.
      if (_session_start_ms != 0) {
        _accumulated_ms   += millis() - _session_start_ms;
        _session_start_ms  = 0;
      }
      _pending_seg_break = true;
    }
    _active = a;
  }

  int  count() const { return _count; }
  bool empty() const { return _count == 0; }

  // i = 0 → oldest entry, i = count()-1 → newest.
  const TrailPoint& at(int i) const { return _buf[(_head + i) % CAPACITY]; }
  const TrailPoint& first() const   { return at(0); }
  const TrailPoint& last()  const   { return at(_count - 1); }

  void clear() {
    _head = 0; _count = 0;
    _pending_seg_break = false;
    _accumulated_ms    = 0;
    _session_start_ms  = 0;
  }

  // Returns true if the point was stored (passed the min-delta gate).
  // First point of the ring and the first point after a stop/start cycle
  // get flagged TRAIL_FLAG_SEG_START so the map renderer breaks the line.
  bool addPoint(int32_t lat_1e6, int32_t lon_1e6, uint32_t ts, uint16_t min_delta_m) {
    if (_count > 0 && !_pending_seg_break) {
      float d = haversineMeters(last().lat_1e6, last().lon_1e6, lat_1e6, lon_1e6);
      if (d < (float)min_delta_m) return false;
    }
    uint8_t flags = (_count == 0 || _pending_seg_break) ? TRAIL_FLAG_SEG_START : 0;
    _pending_seg_break = false;

    int pos;
    if (_count < CAPACITY) {
      pos = (_head + _count) % CAPACITY;
      _count++;
    } else {
      pos = _head;
      _head = (_head + 1) % CAPACITY;
    }
    _buf[pos].lat_1e6 = lat_1e6;
    _buf[pos].lon_1e6 = lon_1e6;
    _buf[pos].ts      = ts;
    _buf[pos].flags   = flags;
    return true;
  }

  // Sum of pairwise Haversine deltas across the whole ring, skipping segment
  // boundaries (a SEG_START point isn't reached from its predecessor).
  uint32_t totalDistanceMeters() const {
    float d = 0;
    for (int i = 1; i < _count; i++) {
      if (at(i).flags & TRAIL_FLAG_SEG_START) continue;
      d += haversineMeters(at(i - 1).lat_1e6, at(i - 1).lon_1e6,
                            at(i).lat_1e6,     at(i).lon_1e6);
    }
    return (uint32_t)d;
  }

  // Cumulative active tracking time across all start→stop sessions, in
  // seconds. Counts ticks while the trail is on, freezes while it's off.
  // millis()-based so it doesn't depend on RTC sync.
  uint32_t elapsedSeconds() const {
    uint32_t ms = _accumulated_ms;
    if (_active && _session_start_ms != 0) ms += millis() - _session_start_ms;
    return ms / 1000;
  }

  // Average speed in km/h = total distance / cumulative active time.
  uint16_t avgSpeedKmh() const {
    uint32_t es = elapsedSeconds();
    if (es == 0) return 0;
    return (uint16_t)((float)totalDistanceMeters() / (float)es * 3.6f);
  }

  // Compute bounding box across all points. Returns false if empty.
  bool boundingBox(int32_t& min_lat, int32_t& min_lon,
                   int32_t& max_lat, int32_t& max_lon) const {
    if (_count == 0) return false;
    min_lat = max_lat = first().lat_1e6;
    min_lon = max_lon = first().lon_1e6;
    for (int i = 1; i < _count; i++) {
      const auto& p = at(i);
      if (p.lat_1e6 < min_lat) min_lat = p.lat_1e6;
      if (p.lat_1e6 > max_lat) max_lat = p.lat_1e6;
      if (p.lon_1e6 < min_lon) min_lon = p.lon_1e6;
      if (p.lon_1e6 > max_lon) max_lon = p.lon_1e6;
    }
    return true;
  }

  // Approximate great-circle distance in metres (Haversine).
  static float haversineMeters(int32_t la1, int32_t lo1, int32_t la2, int32_t lo2) {
    const float R   = 6371000.0f;
    const float D2R = (float)M_PI / 180.0f;
    float lat1 = (la1 / 1.0e6f) * D2R;
    float lat2 = (la2 / 1.0e6f) * D2R;
    float dlat = ((la2 - la1) / 1.0e6f) * D2R;
    float dlon = ((lo2 - lo1) / 1.0e6f) * D2R;
    float sdl  = sinf(dlat * 0.5f);
    float sdo  = sinf(dlon * 0.5f);
    float a    = sdl * sdl + cosf(lat1) * cosf(lat2) * sdo * sdo;
    float c    = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return R * c;
  }

private:
  TrailPoint _buf[CAPACITY];
  int      _head             = 0;
  int      _count            = 0;
  bool     _active           = false;
  bool     _pending_seg_break = false;  // next addPoint flags itself SEG_START
  uint32_t _accumulated_ms   = 0;       // banked active time across previous sessions
  uint32_t _session_start_ms = 0;       // millis() of the current active session, 0 if none
};
