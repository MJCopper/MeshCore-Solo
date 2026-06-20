#pragma once

#include <Mesh.h>
#include <RadioLib.h>

class RadioLibWrapper : public mesh::Radio {
protected:
  PhysicalLayer* _radio;
  mesh::MainBoard* _board;
  uint32_t n_recv, n_sent, n_recv_errors;
  int16_t _noise_floor, _threshold;
  uint16_t _num_floor_samples;
  int32_t _floor_sample_sum;
  uint8_t _preamble_sf;

  void idle();
  void startRecv();
  float packetScoreInt(float snr, int sf, int packet_len);
  virtual bool isReceivingPacket() =0;
  virtual void doResetAGC();

  // Power-save RX: hardware SX126x RX duty-cycle (SetRxDutyCycle). Instead of a
  // continuous receive the chip itself cycles RX↔sleep, latches a preamble, then
  // stays in RX to receive the packet (RX_DONE on DIO1) — no MCU state machine,
  // average RX current cut several-fold. Driven from armRecv()/loop(); falls back
  // to continuous RX if the modem doesn't support it.
  bool _power_save = false;
  bool _ps_active = false;       // is the radio currently armed in duty-cycle mode
  int8_t _tx_dbm = 0;            // last TX power applied (tracks APC's live value)
  void armRecv();                // arm RX: duty-cycle in power-save, else continuous
  // Arm the hardware RX duty-cycle. Base returns UNSUPPORTED → armRecv() falls
  // back to continuous RX; SX126x overrides with startReceiveDutyCycleAuto().
  virtual int16_t startPowerSaveRecv() { return RADIOLIB_ERR_UNSUPPORTED; }

public:
  RadioLibWrapper(PhysicalLayer& radio, mesh::MainBoard& board) : _radio(&radio), _board(&board), _preamble_sf(0) { n_recv = n_sent = 0; }

  void begin() override;
  // Enable/disable hardware duty-cycle RX. Takes effect on the next RX re-arm
  // (loop() re-arms once the live mode differs from this request).
  void setPowerSaving(bool en) { _power_save = en; }
  bool getPowerSaving() const { return _power_save; }
  virtual void powerOff() { _radio->sleep(); }
  int recvRaw(uint8_t* bytes, int sz) override;
  uint32_t getEstAirtimeFor(int len_bytes) override;
  bool startSendRaw(const uint8_t* bytes, int len) override;
  bool isSendComplete() override;
  void onSendFinished() override;
  bool isInRecvMode() const override;
  bool isChannelActive();

  bool isReceiving() override {
    if (isReceivingPacket()) return true;

    return isChannelActive();
  }

  virtual void setParams(float freq, float bw, uint8_t sf, uint8_t cr) = 0;
  // RadioLib's own setFrequency() silently rejects values outside the chip's
  // validated range and leaves the radio retuned to its previous frequency —
  // setParams() above doesn't check that return code, so the UI clamps to this
  // instead of letting NodePrefs drift out of sync with the actual radio.
  // Default is the generic sanity bound the app's CMD_SET_RADIO_PARAMS already
  // uses; chips with a narrower RadioLib-validated range override it.
  virtual void getFreqBounds(float& min_mhz, float& max_mhz) const { min_mhz = 150.0f; max_mhz = 2500.0f; }
  uint32_t getRngSeed();
  void setTxPower(int8_t dbm);
  int8_t getTxPower() const { return _tx_dbm; }   // actual current power (reflects APC)

  virtual float getCurrentRSSI() =0;
  virtual uint8_t getSpreadingFactor() const { return LORA_SF; }
  static uint16_t preambleLengthForSF(uint8_t sf) { return sf <= 8 ? 32 : 16; }
  // Approx SNR demod floor per SF (Semtech): SF7 -7.5 dB … SF12 -20 dB, -2.5 dB/SF.
  // Single source for both packetScore() and the APC link-margin target.
  static float snrFloorForSF(uint8_t sf) {
    if (sf < 7) sf = 7; else if (sf > 12) sf = 12;
    return -7.5f - 2.5f * (float)(sf - 7);
  }
  void updatePreamble(uint8_t sf) { _preamble_sf = sf; _radio->setPreambleLength(preambleLengthForSF(sf)); }

  int getNoiseFloor() const override { return _noise_floor; }
  void triggerNoiseFloorCalibrate(int threshold) override;
  void resetAGC() override;

  void loop() override;

  uint32_t getPacketsRecv() const { return n_recv; }
  uint32_t getPacketsRecvErrors() const { return n_recv_errors; }
  uint32_t getPacketsSent() const { return n_sent; }
  void resetStats() { n_recv = n_sent = n_recv_errors = 0; }

  virtual float getLastRSSI() const override;
  virtual float getLastSNR() const override;

  float packetScore(float snr, int packet_len) override { return packetScoreInt(snr, 10, packet_len); }  // assume sf=10

  virtual void setRxBoostedGainMode(bool) { }
  virtual bool getRxBoostedGainMode() const { return false; }
};

/**
 * \brief  an RNG impl using the noise from the LoRa radio as entropy.
 *         NOTE: this is VERY SLOW!  Use only for things like creating new LocalIdentity
*/
class RadioNoiseListener : public mesh::RNG {
  PhysicalLayer* _radio;
public:
  RadioNoiseListener(PhysicalLayer& radio): _radio(&radio) { }

  void random(uint8_t* dest, size_t sz) override {
    for (int i = 0; i < sz; i++) {
      dest[i] = _radio->randomByte() ^ (::random(0, 256) & 0xFF);
    }
  }
};
