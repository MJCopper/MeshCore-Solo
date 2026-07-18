#pragma once

#include "../BaseSerialInterface.h"
#include "../ArduinoSerialInterface.h"
#include "SerialBLEInterface.h"

// Wraps BLE + USB serial interfaces: BLE takes priority when connected,
// USB is the fallback while companion access is enabled.
// enable()/disable() control both companion transports. USB power/charging is
// unaffected; disabling only stops the framed companion protocol.
// BLE state machine is only pumped when BLE is enabled; USB is not read while BLE is connected.
class DualSerialInterface : public BaseSerialInterface {
  SerialBLEInterface _ble;
  ArduinoSerialInterface _usb;
  uint8_t _ble_buf[MAX_FRAME_SIZE];
  uint8_t _usb_buf[MAX_FRAME_SIZE];
  bool _ble_enabled;
  bool _ble_was_connected;

public:
  DualSerialInterface() : _ble_enabled(false), _ble_was_connected(false) {}

  void begin(const char* ble_prefix, char* node_name, uint32_t pin_code, Stream& usb_stream) {
    _ble.begin(ble_prefix, node_name, pin_code);
    _usb.begin(usb_stream);
    _usb.enable();
  }

  void enable() override  { _usb.enable(); _ble.enable(); _ble_enabled = true; }
  void disable() override { _ble.disable(); _usb.disable(); _ble_enabled = false; }
  bool isEnabled() const override { return _ble_enabled; }

  // Keep the historical send-fallback contract. disable() prevents framed USB
  // input; writes made while locked are harmless because no client can request them.
  bool isConnected() const override { return true; }
  // True only when a BLE companion app is paired and connected.
  bool isBLEConnected() const override { return _ble_enabled && _ble.isConnected(); }
  // A companion app is actually connected if BLE is bonded OR a host holds the
  // USB-CDC port open. (bool)Serial == tud_cdc_n_connected() (DTR asserted) — a
  // serial monitor counts too, but charging-only (no host) does not.
  bool isClientConnected() const override { return isBLEConnected() || (bool)Serial; }

  bool isWriteBusy() const override {
    return (_ble_enabled && _ble.isConnected()) ? _ble.isWriteBusy() : _usb.isWriteBusy();
  }

  size_t writeFrame(const uint8_t src[], size_t len) override {
    return (_ble_enabled && _ble.isConnected()) ? _ble.writeFrame(src, len) : _usb.writeFrame(src, len);
  }

  size_t checkRecvFrame(uint8_t dest[]) override {
    if (_ble_enabled) {
      size_t ble_len = _ble.checkRecvFrame(_ble_buf);
      bool ble_now = _ble.isConnected();

      if (ble_now) {
        _ble_was_connected = true;
        if (ble_len > 0) { memcpy(dest, _ble_buf, ble_len); return ble_len; }
        return 0;  // BLE active — don't read USB to keep its state machine clean
      }

      if (_ble_was_connected) {
        // BLE just disconnected — reset USB state machine so stale partial frames don't block it
        _ble_was_connected = false;
        _usb.enable();
        return 0;
      }
    }

    size_t usb_len = _usb.checkRecvFrame(_usb_buf);
    if (usb_len > 0) { memcpy(dest, _usb_buf, usb_len); return usb_len; }
    return 0;
  }
};
