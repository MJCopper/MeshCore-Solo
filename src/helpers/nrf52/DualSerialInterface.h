#pragma once

#include "../BaseSerialInterface.h"
#include "../ArduinoSerialInterface.h"
#include "SerialBLEInterface.h"

// Wraps BLE + USB serial interfaces: BLE takes priority when connected,
// USB is always ready as a fallback. Both state machines run continuously.
class DualSerialInterface : public BaseSerialInterface {
  SerialBLEInterface _ble;
  ArduinoSerialInterface _usb;
  uint8_t _ble_buf[MAX_FRAME_SIZE];
  uint8_t _usb_buf[MAX_FRAME_SIZE];

public:
  void begin(const char* ble_prefix, char* node_name, uint32_t pin_code, Stream& usb_stream) {
    _ble.begin(ble_prefix, node_name, pin_code);
    _usb.begin(usb_stream);
  }

  void enable() override  { _ble.enable();  _usb.enable(); }
  void disable() override { _ble.disable(); _usb.disable(); }
  bool isEnabled() const override { return _ble.isEnabled() || _usb.isEnabled(); }

  // Reports BLE connection state (used for UI indicator and buzzer Auto mode)
  bool isConnected() const override { return _ble.isConnected(); }

  bool isWriteBusy() const override {
    return _ble.isConnected() ? _ble.isWriteBusy() : _usb.isWriteBusy();
  }

  size_t writeFrame(const uint8_t src[], size_t len) override {
    return _ble.isConnected() ? _ble.writeFrame(src, len) : _usb.writeFrame(src, len);
  }

  size_t checkRecvFrame(uint8_t dest[]) override {
    // Always pump both state machines: BLE needs this for TX queue drain and
    // advertising watchdog; USB needs it to drain its input buffer.
    size_t ble_len = _ble.checkRecvFrame(_ble_buf);
    size_t usb_len = _usb.checkRecvFrame(_usb_buf);

    if (_ble.isConnected()) {
      if (ble_len > 0) { memcpy(dest, _ble_buf, ble_len); return ble_len; }
    } else {
      if (usb_len > 0) { memcpy(dest, _usb_buf, usb_len); return usb_len; }
    }
    return 0;
  }
};
