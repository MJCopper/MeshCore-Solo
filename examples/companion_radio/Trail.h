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
};

class TrailStore {
public:
  static const int CAPACITY = 256;

  // Interval options (seconds) and their settings labels. Index 0 default = 1 min.
  static const uint8_t INTERVAL_COUNT = 4;
  static uint16_t intervalSecs(uint8_t idx) {
    static const uint16_t OPTS[INTERVAL_COUNT] = { 60, 30, 300, 900 };
    return OPTS[idx < INTERVAL_COUNT ? idx : 0];
  }
  static const char* intervalLabel(uint8_t idx) {
    static const char* L[INTERVAL_COUNT] = { "1 min", "30 s", "5 min", "15 min" };
    return L[idx < INTERVAL_COUNT ? idx : 0];
  }

  // Min-delta (metres) gates samples too close to the previous one.
  static const uint8_t MIN_DELTA_COUNT = 3;
  static uint16_t minDeltaMeters(uint8_t idx) {
    static const uint16_t OPTS[MIN_DELTA_COUNT] = { 25, 5, 100 };
    return OPTS[idx < MIN_DELTA_COUNT ? idx : 0];
  }
  static const char* minDeltaLabel(uint8_t idx) {
    static const char* L[MIN_DELTA_COUNT] = { "25 m", "5 m", "100 m" };
    return L[idx < MIN_DELTA_COUNT ? idx : 0];
  }

  bool isActive() const { return _active; }
  void setActive(bool a) { _active = a; }

  int  count() const { return _count; }
  bool empty() const { return _count == 0; }

  // i = 0 → oldest entry, i = count()-1 → newest.
  const TrailPoint& at(int i) const { return _buf[(_head + i) % CAPACITY]; }
  const TrailPoint& first() const   { return at(0); }
  const TrailPoint& last()  const   { return at(_count - 1); }

  void clear() { _head = 0; _count = 0; }

  // Returns true if the point was stored (passed the min-delta gate).
  bool addPoint(int32_t lat_1e6, int32_t lon_1e6, uint32_t ts, uint16_t min_delta_m) {
    if (_count > 0) {
      float d = haversineMeters(last().lat_1e6, last().lon_1e6, lat_1e6, lon_1e6);
      if (d < (float)min_delta_m) return false;
    }
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
    return true;
  }

  // Sum of pairwise Haversine deltas across the whole ring.
  uint32_t totalDistanceMeters() const {
    float d = 0;
    for (int i = 1; i < _count; i++) {
      d += haversineMeters(at(i - 1).lat_1e6, at(i - 1).lon_1e6,
                            at(i).lat_1e6,     at(i).lon_1e6);
    }
    return (uint32_t)d;
  }

  // Seconds between the first and last sample (0 if fewer than 2 points).
  uint32_t elapsedSeconds() const {
    if (_count < 2) return 0;
    return last().ts - first().ts;
  }

  // Speed in km/h from the last pair of samples; 0 if only one point.
  uint16_t currentSpeedKmh() const {
    if (_count < 2) return 0;
    const auto& a = at(_count - 2);
    const auto& b = last();
    float dt = (float)(b.ts - a.ts);
    if (dt <= 0) return 0;
    float d = haversineMeters(a.lat_1e6, a.lon_1e6, b.lat_1e6, b.lon_1e6);
    return (uint16_t)(d / dt * 3.6f);
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
  int  _head   = 0;
  int  _count  = 0;
  bool _active = false;
};
