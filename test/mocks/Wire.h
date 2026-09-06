#pragma once
#include <stdint.h>

class TwoWire {
public:
  bool connected = true;
  uint8_t next = 0;
  unsigned reads = 0;
  void beginTransmission(uint8_t) { }
  int endTransmission() { return connected ? 0 : 1; }
  int requestFrom(uint8_t, uint8_t) { reads++; return connected ? 1 : 0; }
  int available() { return connected ? 1 : 0; }
  int read() { uint8_t value = next; next = 0; return value; }
};
