#pragma once

#include <Arduino.h>

#define MAX_FRAME_SIZE  176   // +4 for transport codes (region scoping)

class BaseSerialInterface {
protected:
  BaseSerialInterface() { }

public:
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;

  virtual bool isConnected() const = 0;
  // Transport-specific compatibility hooks used by legacy single/dual
  // interfaces. MultiSerialInterface derives equivalent state from its
  // registered transport types.
  virtual bool isBLEConnected() const { return isConnected(); }
  virtual bool isClientConnected() const { return isConnected(); }
  virtual void loop() {};

  virtual bool isWriteBusy() const = 0;
  virtual size_t writeFrame(const uint8_t src[], size_t len) = 0;
  virtual size_t checkRecvFrame(uint8_t dest[]) = 0;
};
