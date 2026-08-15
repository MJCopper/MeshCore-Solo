#pragma once

#include <cstdint>

namespace solo {

// Source-agnostic boot time-sync state. Hardware control stays in UITask; this
// class only schedules temporary GPS claims and decides when to release them.
class BootTimeSync {
public:
  enum class Action : uint8_t { NONE, START_TEMP_GPS, STOP_TEMP_GPS };
  static constexpr uint32_t GPS_TIMEOUT_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t GPS_RETRY_TIMEOUT_MS = 90UL * 1000UL;
  static constexpr uint32_t RETRY_INTERVAL_MS = 60UL * 60UL * 1000UL;
  static constexpr uint32_t RETRY_WINDOW_MS = 24UL * 60UL * 60UL * 1000UL;

private:
  bool _pending = false;
  bool _has_gps = false;
  bool _owns_gps = false;
  uint32_t _generation = 0;
  uint32_t _deadline = 0;
  uint32_t _retry_at = 0;
  uint32_t _stop_at = 0;

public:
  void begin(uint32_t generation, bool has_gps, bool gps_configured_on, uint32_t now) {
    _pending = true;
    _has_gps = has_gps;
    _generation = generation;
    _owns_gps = has_gps && !gps_configured_on;
    _deadline = now + GPS_TIMEOUT_MS;
    _retry_at = now + RETRY_INTERVAL_MS;
    _stop_at = now + RETRY_WINDOW_MS;
  }

  bool pending() const { return _pending; }
  bool shouldStartGps() const { return _owns_gps; }

  Action tick(uint32_t generation, bool gps_configured_on, bool gps_enabled, uint32_t now) {
    if (!_pending) return Action::NONE;

    if (generation != _generation) {
      _pending = false;
      bool stop = _owns_gps && !gps_configured_on;
      _owns_gps = false;
      return stop ? Action::STOP_TEMP_GPS : Action::NONE;
    }

    // Give up after one day. If a retry owns the receiver at the boundary,
    // release it just as we would at the normal attempt deadline.
    if ((int32_t)(now - _stop_at) >= 0) {
      _pending = false;
      bool stop = _owns_gps && !gps_configured_on;
      _owns_gps = false;
      return stop ? Action::STOP_TEMP_GPS : Action::NONE;
    }

    // A manual enable takes ownership from the boot helper. A manual disable
    // has already stopped the receiver, so there is nothing left to release.
    if (_owns_gps && (gps_configured_on || !gps_enabled)) _owns_gps = false;

    if (_owns_gps && (int32_t)(now - _deadline) >= 0) {
      _owns_gps = false;
      return Action::STOP_TEMP_GPS;
    }

    // Still unsynchronised: once per hour, retry the same bounded temporary GPS
    // claim used at boot. GPS configured on is already the user's responsibility;
    // never toggle it behind their back.
    if (_has_gps && !_owns_gps && !gps_configured_on && !gps_enabled &&
        (int32_t)(now - _retry_at) >= 0) {
      _owns_gps = true;
      _deadline = now + GPS_RETRY_TIMEOUT_MS;
      _retry_at = now + RETRY_INTERVAL_MS;
      return Action::START_TEMP_GPS;
    }
    return Action::NONE;
  }
};

} // namespace solo
