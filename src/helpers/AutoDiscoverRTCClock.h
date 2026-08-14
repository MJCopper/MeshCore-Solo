#pragma once

#include <Mesh.h>
#include <Arduino.h>
#include <Wire.h>

class AutoDiscoverRTCClock : public mesh::RTCClock {
  mesh::RTCClock* _fallback;
  uint32_t _set_generation = 0;

  bool i2c_probe(TwoWire& wire, uint8_t addr);
public:
  AutoDiscoverRTCClock(mesh::RTCClock& fallback) : _fallback(&fallback) { }

  void begin(TwoWire& wire);
  uint32_t getCurrentTime() override;
  void setCurrentTime(uint32_t time) override;
  // Incremented for every explicit live time update (GPS, companion app,
  // network or CLI). Callers can snapshot this after restoring saved time and
  // detect the next authoritative sync without knowing which source supplied it.
  uint32_t getSetGeneration() const { return _set_generation; }

  void tick() override {
    _fallback->tick();   // is typically VolatileRTCClock, which now needs tick()
  }
};
