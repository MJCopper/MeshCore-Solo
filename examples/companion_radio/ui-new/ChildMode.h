#pragma once

#include "../NodePrefs.h"
#include <Utils.h>
#include <helpers/ContactInfo.h>
#include <helpers/ui/DisplayDriver.h>
#include "DigitEditor.h"

// Small policy helper kept independent of the screens so upstream UI changes
// only need to call these predicates. This is intentionally a practical UI
// lock; physical flash access remains a recovery/bypass path.
namespace childmode {

// PIN entry uses the normal screen colours for unselected digits and inverts
// only the active digit (white background, black glyph). DigitEditor itself is
// left unchanged because its usual callers render inside highlighted rows.
static inline void renderPinEditor(DisplayDriver& display, DigitEditor& editor, int x, int y) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%06lu", (unsigned long)editor.value);
  const int cw = display.getCharWidth();
  for (int i = 0; i < 6; i++) {
    int cx = x + i * cw;
    if (i == editor.cursor) {
      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(cx, y - 1, cw, display.getLineHeight() + 1);
      display.setColor(DisplayDriver::DARK);
    } else {
      display.setColor(DisplayDriver::LIGHT);
    }
    char digit[2] = { buf[i], '\0' };
    display.setCursor(cx, y);
    display.print(digit);
  }
  display.setColor(DisplayDriver::LIGHT);
}

static inline uint32_t pinHash(uint32_t pin) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) {
    h ^= (uint8_t)(pin % 10);
    h *= 16777619u;
    pin /= 10;
  }
  return h ^ 0x4348494Cu;  // "CHIL", avoids the unset value being a useful PIN
}

static inline bool contactAllowed(const NodePrefs* prefs, const ContactInfo& contact) {
  return !prefs || !prefs->child_mode_enabled || (contact.flags & 0x01);
}

static inline bool privateChannel(const char* name, const uint8_t* secret) {
  static const uint8_t PUBLIC_SECRET[16] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72
  };
  if (!name || !secret || memcmp(secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET)) == 0) return false;

  bool all_zero = true;
  for (int i = 0; i < 16 && all_zero; i++) all_zero = secret[i] == 0;
  if (all_zero) return false;

  // A hashtag channel is public only when both its conventional name and
  // derived key match. A private channel may legitimately use a leading '#'
  // with a separately shared secret and remains private.
  if (name[0] == '#') {
    uint8_t digest[32];
    mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)name, strlen(name));
    if (memcmp(secret, digest, 16) == 0) return false;
  }
  return true;
}

static inline bool channelAllowed(const NodePrefs* prefs, uint8_t index,
                                  const char* name, const uint8_t* secret) {
  if (!prefs || !prefs->child_mode_enabled) return true;
  return prefs->child_channels_enabled && index < 64 &&
         (prefs->ch_fav_bitmask & (1ULL << index)) != 0 &&
         privateChannel(name, secret);
}

static inline bool contactNotificationAllowed(bool locked, const NodePrefs* prefs,
                                              const ContactInfo* contact) {
  return !locked || (contact && contactAllowed(prefs, *contact));
}

static inline bool channelNotificationAllowed(bool locked, const NodePrefs* prefs,
                                              uint8_t index, const char* name,
                                              const uint8_t* secret) {
  return !locked || channelAllowed(prefs, index, name, secret);
}

} // namespace childmode
