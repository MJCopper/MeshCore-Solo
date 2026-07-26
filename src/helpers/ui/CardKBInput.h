#pragma once

#include <Arduino.h>
#include <Wire.h>

// Low-power transport for an optional M5Stack CardKB. Key interpretation stays
// in UITask; this adapter owns only boot detection, bounded polling and failure
// state so the policy can be reused without coupling it to the Solo UI.
class CardKBInput {
  static const uint32_t POLL_INTERVAL_MS = 20;
  static const uint8_t MAX_READ_FAILURES = 3;

  TwoWire* _wire = nullptr;
  uint8_t _address = 0x5F;
  uint8_t _read_failures = 0;
  uint32_t _next_poll_ms = 0;
  bool _present = false;

  static bool due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
  }

public:
  void begin(TwoWire& wire, uint8_t address = 0x5F) {
    _wire = &wire;
    _address = address;
    _read_failures = 0;
    _next_poll_ms = 0;

    // Detection is intentionally boot-only. An absent accessory generates no
    // recurring I2C traffic; connecting it later requires a device reboot.
    _wire->beginTransmission(_address);
    _present = (_wire->endTransmission() == 0);
  }

  bool isPresent() const { return _present; }

  // Returns true when a sample was read, including raw=0 for key release.
  // The caller decides when polling is permitted (Solo suspends it whenever
  // the display is off) and retains upstream's key-edge interpretation.
  bool poll(uint8_t& raw) {
    if (!_wire || !_present) return false;

    uint32_t now = millis();
    if (!due(now, _next_poll_ms)) return false;
    _next_poll_ms = now + POLL_INTERVAL_MS;

    if (_wire->requestFrom(_address, (uint8_t)1) != 1 || !_wire->available()) {
      if (++_read_failures >= MAX_READ_FAILURES) _present = false;
      return false;
    }

    _read_failures = 0;
    raw = (uint8_t)_wire->read();
    return true;
  }
};
