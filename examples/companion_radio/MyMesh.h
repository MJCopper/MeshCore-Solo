#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"
#include <helpers/ui/DisplayDriver.h>

// Forward declaration for UITask
class UITask;

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "6 Jun 2026"
#endif

// Versioning: vX.Y = upstream base, solo.N = fork revision
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.16-solo.0"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

struct DiscoverResult {
  char    name[32];   // contact name if known, "" if unknown (use type label)
  uint8_t type;       // ADV_TYPE_REPEATER / ADV_TYPE_SENSOR / ADV_TYPE_ROOM
  bool    is_known;   // true = in contacts[], false = new unknown node
  int8_t  rssi;       // RSSI of the response as received by us (dBm)
  int8_t  snr_x4;     // SNR of the response as received by us (dB × 4)
  int8_t  remote_snr_x4; // SNR at which responder heard our request (dB × 4)
  uint8_t pub_key[PUB_KEY_SIZE];
  uint32_t timestamp;
};

#define EXPECTED_ACK_TABLE_SIZE 8

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  void sendNodeDiscoverReq();
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);
  int  getDiscoverResults(DiscoverResult dest[], int max_count);

  // Ping/Trace functionality
  #define PING_RESULT_MAX 4
  typedef void (*PingCallback)(uint32_t tag, int16_t snr_out_x4, int16_t snr_back_x4, uint32_t rtt_ms);
  
  struct PingResult {
    uint32_t tag;
    uint32_t auth_code;
    int16_t snr_out_x4;    // SNR out (to first hop) × 4
    int16_t snr_back_x4;   // SNR back (from last hop to us) × 4
    uint32_t rtt_ms;       // Round-trip time in milliseconds
    bool received;
    unsigned long sent_ms;
  };
  
  uint32_t sendPing(const uint8_t* dest_pubkey, uint8_t hash_width = 1);
  void setPingCallback(PingCallback cb, void* arg);
  void clearPingResult(uint32_t tag);
  PingResult* getPingResult(uint32_t tag);
  PingCallback getPingCallback() const { return _ping_callback; }
  AbstractUITask* getUITask() const { return _ui; }

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onDiscoveredAdvert(bool was_flood) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;
  void onAckRecv(mesh::Packet* packet, uint32_t ack_crc) override;          // APC: ACK SNR sample
  // APC internals (see MyMesh.cpp): one reverse-link SNR sample, a lost-confirmation
  // ramp-up, and tracking an originated flood so its echo (or absence) can be scored.
  // The heard-echo sampling itself lives in filterRecvFloodPacket() (declared below).
  void apcSampleSnr(float snr);
  void apcOnFailure();
  void apcTrackFloodSend(const mesh::Packet* pkt);
  void trackRelaySend(const mesh::Packet* pkt);   // arm the UI relayed-into-mesh tracker
public:
  // Seq of the most recently tracked channel send — the UI records it on the
  // outgoing history entry so a heard echo (onChannelRelayed) can match it back.
  uint32_t lastChannelRelaySeq() const { return _last_relay_seq; }
private:

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() { _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon); }
  void saveRTCTime() { _store->saveRTCTime(); }
  DataStore* getDataStore() const { return _store; }
  void applyApc();   // (re)initialise Adaptive Power Control from prefs

  bool isAckPending(uint32_t expected_ack) const {
    for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++)
      if (expected_ack_table[i].ack == expected_ack) return true;
    return false;
  }


#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

private:
  void tryBotReplyDM(const ContactInfo& from, const char* text);
  void tryBotReplyChannel(uint8_t channel_idx, const char* text);

  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  bool isValidClientRepeatFreq(uint32_t f) const;
#ifdef ENABLE_SCREENSHOT
  void handleScreenshotRequest();
  void sendScreenshotResponse(DisplayDriver* display, const uint8_t* buffer, uint16_t bufferSize);
#endif

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts();

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  int8_t _apc_cur_dbm;       // APC current TX power (≤ tx_power_dbm ceiling) when tx_apc on
  float _apc_margin_ewma;    // APC smoothed reverse-link SNR margin above the SF demod floor
  uint8_t _apc_fail_count;   // APC consecutive lost-confirmation count (graduated ramp-up)
  uint8_t _apc_flood_hash[MAX_HASH_SIZE];  // hash of the channel/flood send awaiting a repeater echo
  uint16_t _apc_flood_len;                 // its payload length — cheap pre-filter before hashing
  uint32_t _apc_flood_deadline;            // echo-wait deadline for that send
  bool _apc_flood_pending;                 // a tracked flood send is awaiting its echo
  // UI "relayed into mesh" tracker for channel sends — independent of APC. A small
  // ring so a quick burst of channel sends are each tracked (not just the latest).
  // Hashing on receive only runs while at least one slot is pending, so the hot
  // flood-recv path is untouched otherwise.
  static const int RELAY_RING = 4;
  struct RelaySlot {
    uint8_t  hash[MAX_HASH_SIZE];
    uint16_t len;
    uint32_t deadline;
    uint32_t seq;
    bool     pending;
  };
  RelaySlot _relay[RELAY_RING];
  int      _relay_head;        // next ring slot to overwrite
  int      _relay_active;      // number of slots currently pending (cheap gate)
  uint32_t _relay_seq;         // monotonic id counter for tracked sends
  uint32_t _last_relay_seq;    // seq of the most recent tracked send (for the UI to record)
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;
  unsigned long _bot_last_dm_reply_ms;
  unsigned long _bot_last_ch_reply_ms;
  unsigned long _next_auto_advert_ms;

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table

  #define DISCOVER_RESULTS_MAX 16
  DiscoverResult  _discover_results[DISCOVER_RESULTS_MAX];
  int             _discover_count;
  uint32_t        _pending_node_discover_tag;
  unsigned long   _pending_node_discover_until;

  // ── Ping/Trace state ──────────────────────────────────────────────────────
  PingResult _ping_results[PING_RESULT_MAX];
  PingCallback _ping_callback;
  void* _ping_callback_arg;
};

extern MyMesh the_mesh;
