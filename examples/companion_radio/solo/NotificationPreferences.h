#pragma once

#include "../NodePrefs.h"
#include <stdint.h>
#include <string.h>

// Shared interpretation of the compact notification preference storage.
// Keeping reads, writes and deletion cleanup here ensures the settings UI and
// notification dispatcher cannot disagree about the encoded state values.
namespace solo {

class NotificationPreferences {
  template <class Entry>
  static uint8_t tableGet(const Entry* table, int count, const uint8_t* pub_key,
                          uint8_t Entry::* value) {
    if (!table || !pub_key) return 0;
    for (int i = 0; i < count; i++)
      if (table[i].*value && memcmp(table[i].prefix, pub_key, 4) == 0)
        return table[i].*value;
    return 0;
  }

  template <class Entry>
  static void tableSet(Entry* table, int count, const uint8_t* pub_key,
                       uint8_t Entry::* value, uint8_t setting) {
    if (!table || !pub_key) return;
    for (int i = 0; i < count; i++) {
      if (table[i].*value && memcmp(table[i].prefix, pub_key, 4) == 0) {
        if (setting == 0) memset(&table[i], 0, sizeof(table[i]));
        else table[i].*value = setting;
        return;
      }
    }
    if (setting == 0) return;
    for (int i = 0; i < count; i++) {
      if (table[i].*value == 0) {
        memcpy(table[i].prefix, pub_key, 4);
        table[i].*value = setting;
        return;
      }
    }
    memcpy(table[0].prefix, pub_key, 4);
    table[0].*value = setting;
  }

  static uint8_t maskGet(uint64_t presence, uint64_t variant, uint8_t index,
                         uint8_t variant_set, uint8_t variant_clear) {
    if (index >= 64) return 0;
    uint64_t mask = 1ULL << index;
    if (!(presence & mask)) return 0;
    return (variant & mask) ? variant_set : variant_clear;
  }

  static void maskSet(uint64_t& presence, uint64_t& variant, uint8_t index,
                      uint8_t setting, uint8_t variant_set) {
    if (index >= 64) return;
    uint64_t mask = 1ULL << index;
    if (setting == 0) {
      presence &= ~mask;
      variant &= ~mask;
    } else {
      presence |= mask;
      if (setting == variant_set) variant |= mask;
      else variant &= ~mask;
    }
  }

public:
  // Notification state: 0=global, 1=muted, 2=force-on.
  static uint8_t dmState(const NodePrefs* prefs, const uint8_t* pub_key) {
    return prefs ? tableGet(prefs->dm_notif, NodePrefs::DM_NOTIF_TABLE_MAX,
                            pub_key, &NodePrefs::DmNotifEntry::state) : 0;
  }

  static void setDmState(NodePrefs* prefs, const uint8_t* pub_key, uint8_t state) {
    if (prefs) tableSet(prefs->dm_notif, NodePrefs::DM_NOTIF_TABLE_MAX,
                        pub_key, &NodePrefs::DmNotifEntry::state, state);
  }

  static uint8_t channelState(const NodePrefs* prefs, uint8_t index) {
    return prefs ? maskGet(prefs->ch_notif_override, prefs->ch_notif_muted,
                           index, 1, 2) : 0;
  }

  static void setChannelState(NodePrefs* prefs, uint8_t index, uint8_t state) {
    if (prefs) maskSet(prefs->ch_notif_override, prefs->ch_notif_muted,
                       index, state, 1);
  }

  static uint8_t dmMelody(const NodePrefs* prefs, const uint8_t* pub_key) {
    return prefs ? tableGet(prefs->dm_melody, NodePrefs::DM_MELODY_TABLE_MAX,
                            pub_key, &NodePrefs::DmMelodyEntry::slot) : 0;
  }

  static void setDmMelody(NodePrefs* prefs, const uint8_t* pub_key, uint8_t slot) {
    if (prefs) tableSet(prefs->dm_melody, NodePrefs::DM_MELODY_TABLE_MAX,
                        pub_key, &NodePrefs::DmMelodyEntry::slot, slot);
  }

  static uint8_t channelMelody(const NodePrefs* prefs, uint8_t index) {
    return prefs ? maskGet(prefs->ch_notif_melody_set, prefs->ch_notif_melody_2,
                           index, 2, 1) : 0;
  }

  static void setChannelMelody(NodePrefs* prefs, uint8_t index, uint8_t slot) {
    if (prefs) maskSet(prefs->ch_notif_melody_set, prefs->ch_notif_melody_2,
                       index, slot, 2);
  }

  static bool removeContact(NodePrefs* prefs, const uint8_t* pub_key) {
    if (!prefs || !pub_key) return false;
    bool changed = dmState(prefs, pub_key) || dmMelody(prefs, pub_key);
    setDmState(prefs, pub_key, 0);
    setDmMelody(prefs, pub_key, 0);
    return changed;
  }

  static bool removeChannel(NodePrefs* prefs, uint8_t index) {
    if (!prefs || index >= 64) return false;
    bool changed = channelState(prefs, index) || channelMelody(prefs, index);
    setChannelState(prefs, index, 0);
    setChannelMelody(prefs, index, 0);
    return changed;
  }
};

} // namespace solo
