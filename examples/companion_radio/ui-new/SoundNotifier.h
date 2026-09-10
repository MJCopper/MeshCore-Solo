#pragma once
// Fork-specific buzzer/melody dispatch — isolated here so upstream changes to
// UITask::notify() don't create merge conflicts.
#ifdef PIN_BUZZER
#include <helpers/ui/buzzer.h>
#include "../NodePrefs.h"
#include "../solo/NotificationPreferences.h"
#include "../solo/BuiltinMelodies.h"
#include "../solo/RingtoneModel.h"

class SoundNotifier {
  genericBuzzer&   _buz;
  const NodePrefs* _prefs;
  char*            _mel_buf;
  int              _mel_buf_sz;

  static void buildMelody(const NodePrefs* p, int slot, char* buf, int size) {
    const uint8_t* notes = (slot == 2) ? p->ringtone2_notes   : p->ringtone_notes;
    uint8_t        len   = (slot == 2) ? p->ringtone2_len      : p->ringtone_len;
    uint8_t        bpm_i = (slot == 2) ? p->ringtone2_bpm_idx  : p->ringtone_bpm_idx;
    solo::RingtoneModel::buildRTTTL(notes, len, bpm_i, buf, size);
  }

  void playSelection(uint8_t selection, bool force, uint8_t empty_fallback) {
    selection = solo::BuiltinMelodies::validate(selection, empty_fallback);
    if (selection == solo::BuiltinMelodies::NONE) return;
    if ((selection == solo::BuiltinMelodies::CUSTOM1 ||
         selection == solo::BuiltinMelodies::CUSTOM2) && _prefs) {
      if (_buz.isPlaying()) _buz.stop();
      buildMelody(_prefs, selection == solo::BuiltinMelodies::CUSTOM2 ? 2 : 1,
                  _mel_buf, _mel_buf_sz);
      if (_mel_buf[0]) {
        if (force) _buz.playForced(_mel_buf); else _buz.play(_mel_buf);
        return;
      }
    }
    const char* melody = solo::BuiltinMelodies::melody(selection);
    if (!melody) melody = solo::BuiltinMelodies::melody(empty_fallback);
    if (force) _buz.playForced(melody); else _buz.play(melody);
  }

public:
  SoundNotifier(genericBuzzer& buz, const NodePrefs* prefs, char* buf, int sz)
    : _buz(buz), _prefs(prefs), _mel_buf(buf), _mel_buf_sz(sz) {}

  void preview(uint8_t selection, uint8_t empty_fallback) {
    if (_buz.isPlaying()) _buz.stop();
    playSelection(selection, true, empty_fallback);
  }

  void playDM(bool dm_valid, const uint8_t* dm_prefix) {
    bool play = false, force = false;
    if (dm_valid && _prefs) {
      uint8_t state = solo::NotificationPreferences::dmState(_prefs, dm_prefix);
      if (state == 2) { play = true; force = true; }
      else if (state == 1) { /* muted */ }
      else { play = !_buz.isQuiet(); }
    } else {
      play = !_buz.isQuiet();
    }
    if (!play) return;

    uint8_t slot = _prefs ? _prefs->notif_melody_dm : solo::BuiltinMelodies::MESSAGE;
    if (dm_valid && _prefs) {
      uint8_t override_slot = solo::NotificationPreferences::dmMelody(_prefs, dm_prefix);
      if (override_slot) slot = override_slot - 1;
    }
    playSelection(slot, force, solo::BuiltinMelodies::MESSAGE);
  }

  void playLowBattery() {
    // Single 62 ms note, respecting the global buzzer mode and volume.
    _buz.play("LowBat:d=32,o=6,b=120:c");
  }

  void playCH(int ch_idx) {
    bool play = false, force = false;
    if (ch_idx >= 0 && ch_idx < 64 && _prefs) {
      uint8_t state = solo::NotificationPreferences::channelState(_prefs, ch_idx);
      if (state == 2) { play = true; force = true; }
      else if (state == 0) play = !_buz.isQuiet();
    } else {
      play = !_buz.isQuiet();
    }
    if (!play) return;

    uint8_t slot = _prefs ? _prefs->notif_melody_ch : solo::BuiltinMelodies::KERPLOP;
    if (ch_idx >= 0 && ch_idx < 64 && _prefs) {
      uint8_t override_slot = solo::NotificationPreferences::channelMelody(_prefs, ch_idx);
      if (override_slot) slot = override_slot - 1;
    }
    playSelection(slot, force, solo::BuiltinMelodies::KERPLOP);
  }

  void playAD(bool is_flood) {
    if (_prefs && _prefs->advert_sound_scope == ADVERT_SOUND_SCOPE_ZERO_HOP && is_flood) return;
    if (_buz.isQuiet()) return;
    uint8_t slot = _prefs ? _prefs->notif_melody_ad : solo::BuiltinMelodies::MESSAGE;
    playSelection(slot, false, solo::BuiltinMelodies::MESSAGE);
  }
};
#endif
