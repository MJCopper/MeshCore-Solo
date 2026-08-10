#pragma once

#include "../NodePrefs.h"
#include <Utils.h>
#include <helpers/ContactInfo.h>
#include <helpers/ui/DisplayDriver.h>
#include "DigitEditor.h"
#include "ChildModePolicy.h"
#include "../solo/SoloPolicy.h"

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
  const bool locked = solo::Features::CHILD_MODE && prefs && prefs->child_mode_enabled;
  return solo::Policy::contactAllowed(prefs, locked, &contact);
}

static inline bool privateChannel(const char* name, const uint8_t* secret) {
  return solo::Policy::privateChannel(name, secret);
}

static inline bool channelAllowed(const NodePrefs* prefs, uint8_t index,
                                  const char* name, const uint8_t* secret) {
  const bool locked = solo::Features::CHILD_MODE && prefs && prefs->child_mode_enabled;
  return solo::Policy::channelAllowed(prefs, locked, index, name, secret);
}

} // namespace childmode
