#pragma once

#include "CustomSX1262.h"
#include "RadioLibWrappers.h"
#include "SX126xReset.h"

#ifndef USE_SX1262
#define USE_SX1262
#endif

class CustomSX1262Wrapper : public RadioLibWrapper {
public:
  CustomSX1262Wrapper(CustomSX1262& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }

  void setParams(float freq, float bw, uint8_t sf, uint8_t cr) override {
    ((CustomSX1262 *)_radio)->setFrequency(freq);
    ((CustomSX1262 *)_radio)->setSpreadingFactor(sf);
    ((CustomSX1262 *)_radio)->setBandwidth(bw);
    ((CustomSX1262 *)_radio)->setCodingRate(cr);
    updatePreamble(sf);
  }

  // From RadioLib's SX1262::setFrequency(): RADIOLIB_CHECK_RANGE(freq, 150.0f, 960.0f, ...).
  void getFreqBounds(float& min_mhz, float& max_mhz) const override { min_mhz = 150.0f; max_mhz = 960.0f; }

  bool isReceivingPacket() override { 
    return ((CustomSX1262 *)_radio)->isReceiving();
  }
  float getCurrentRSSI() override {
    return ((CustomSX1262 *)_radio)->getRSSI(false);
  }
  float getLastRSSI() const override { return ((CustomSX1262 *)_radio)->getRSSI(); }
  float getLastSNR() const override { return ((CustomSX1262 *)_radio)->getSNR(); }

  float packetScore(float snr, int packet_len) override {
    int sf = ((CustomSX1262 *)_radio)->spreadingFactor;
    return packetScoreInt(snr, sf, packet_len);
  }
  uint8_t getSpreadingFactor() const override { return ((CustomSX1262 *)_radio)->spreadingFactor; }
  virtual void powerOff() override {
    ((CustomSX1262 *)_radio)->sleep(false);
  }

  void doResetAGC() override { sx126xResetAGC((SX126x *)_radio); }

  // Power-save RX = hardware RX duty-cycle (SX126x SetRxDutyCycle, datasheet
  // 13.1.7). The chip's sequencer cycles RX↔sleep on its own, latches a preamble
  // of the configured length and then stays in RX to receive the packet, raising
  // RX_DONE on DIO1 — handled by the normal recvRaw() path, no MCU polling.
  // minSymbols=8 is the reliable preamble-latch count for SF7-12. If the
  // configured preamble is too short for a real duty-cycle (senderPreamble <
  // 2*minSymbols+1), RadioLib transparently falls back to a continuous receive.
  int16_t startPowerSaveRecv() override {
    return ((SX126x *)_radio)->startReceiveDutyCycleAuto(preambleLengthForSF(_preamble_sf), 8);
  }

  void setRxBoostedGainMode(bool en) override {
    ((CustomSX1262 *)_radio)->setRxBoostedGainMode(en);
  }
  bool getRxBoostedGainMode() const override {
    return ((CustomSX1262 *)_radio)->getRxBoostedGainMode();
  }
};
