#pragma once

#include <Mesh.h>
#include <Utils.h>

// Passive decoder for MeshCore's built-in Public channel. Storage and client
// delivery remain room-server concerns; this class owns only the channel key
// and validates decrypted group-text payloads.
class PublicChannelArchive {
  mesh::GroupChannel _channel;

public:
  PublicChannelArchive() {
    static const uint8_t PUBLIC_SECRET[16] = {
      0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
      0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
    };
    memset(&_channel, 0, sizeof(_channel));
    memcpy(_channel.secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET));
    mesh::Utils::sha256(_channel.hash, sizeof(_channel.hash),
                        _channel.secret, sizeof(PUBLIC_SECRET));
  }

  int find(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) const {
    if (!hash || !channels || max_matches < 1 || hash[0] != _channel.hash[0]) return 0;
    channels[0] = _channel;
    return 1;
  }

  bool extractText(uint8_t type, uint8_t* data, size_t len, uint32_t& timestamp,
                   const char*& text) const {
    timestamp = 0;
    text = nullptr;
    if (type != PAYLOAD_TYPE_GRP_TXT || !data || len < 6 || len >= MAX_PACKET_PAYLOAD) return false;
    if ((data[4] >> 2) != 0) return false;
    data[len] = 0;
    if (data[5] == 0) return false;
    memcpy(&timestamp, data, sizeof(timestamp));
    text = reinterpret_cast<const char*>(&data[5]);
    return true;
  }
};
