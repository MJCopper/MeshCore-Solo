#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    advertReceivedFlood,
    advertReceivedZeroHop,
    ack
};

class AbstractUITask {
protected:
  mesh::MainBoard* _board;
  BaseSerialInterface* _serial;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, BaseSerialInterface* serial) : _board(board), _serial(serial) {
    _connected = false;
  }

public:
  void setHasConnection(bool connected) {
    bool prev = _connected;
    _connected = connected;
    if (prev && !connected) onBLEDisconnected();
  }
  bool hasConnection() const { return _connected; }
  virtual void onBLEDisconnected() {}
  // An end-to-end ACK (CRC) arrived for one of our sent messages — drives the
  // DM delivery-status marker. Default no-op for UIs that don't track it.
  virtual void onMsgAck(uint32_t ack_crc) { (void)ack_crc; }
  // A repeater rebroadcast of one of our channel sends was heard (seq from
  // lastChannelRelaySeq()) — drives the channel "relayed into mesh" marker.
  virtual void onChannelRelayed(uint32_t seq) { (void)seq; }
  // True only when a BLE central is actually bonded/connected. On a dual
  // (BLE+USB) interface hasConnection() is always true (USB counts), so use
  // this for BLE-specific UI like the pairing-PIN prompt.
  bool isBLEConnected() const { return _serial->isBLEConnected(); }
  // True when a companion app is connected over any transport (BLE bonded or an
  // open USB-CDC port). For app-connected behaviour like Auto buzzer mute.
  bool isClientConnected() const { return _serial->isClientConnected(); }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isSerialEnabled() const { return _serial->isEnabled(); }
  void enableSerial() { _serial->enable(); }
  void disableSerial() { _serial->disable(); }
  virtual void msgRead(int msgcount) = 0;
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, uint8_t contact_type = 0, const uint8_t* pub_key = nullptr) = 0;
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  virtual void addChannelMsg(uint8_t channel_idx, const char* text) {}
  virtual void addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text, uint32_t sender_timestamp = 0) {}
  // A node shared its current position via a [LOC] message. pub_key is the
  // sender's key prefix for a verified DM share, or null for a channel share
  // (keyed by name, best-effort). Default no-op so UI variants opt in.
  virtual void onSharedLocation(const uint8_t* pub_key, const char* name,
                                int32_t lat_1e6, int32_t lon_1e6,
                                uint32_t ts, bool verified) {}
  virtual void loop() = 0;
};
