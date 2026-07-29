#pragma once

#include <Adafruit_GFX.h>
#include "DisplayDriver.h"
#include "MiscFixedFont.h"
#include "LemonIcons.h"

// Shared misc-fixed 6x9 text renderer for the monochrome OLED drivers.
//
// Both Adafruit_SH110X and Adafruit_SSD1306 derive from Adafruit_GFX and the
// font path is pure pixel plotting, so SH1106Display and SSD1306Display render
// off this one implementation instead of two copies of it. Everything here
// bypasses GFX's own print(): GFX walks bytes and would treat each byte of a
// multi-byte UTF-8 sequence as its own CP437 glyph, and it can't reach the
// custom UI icons either.
//
// Include only from a .cpp — the font tables are static const arrays, so a
// header pulling this in would land a copy of them in every translation unit.
//
// `y` is the TOP of the text row in every function below, matching the UI's
// coordinate convention (GFX fonts would use the baseline).

// Pixel advance of one codepoint at text size sz. Unmapped codepoints get the
// font's own 6px cell, same as the substitution box drawn for them.
static inline uint8_t miscFixedXAdvance(uint32_t cp, int sz) {
  uint8_t xa;
  if (cp < MiscFixed.first || cp > MiscFixed.last) xa = 6;
  else xa = pgm_read_byte(&MiscFixedGlyphs[cp - MiscFixed.first].xAdvance);
  return xa * sz;
}

// Draw one codepoint at (x, y); returns the x to continue from.
static inline int16_t miscFixedDrawGlyph(Adafruit_GFX& gfx, int16_t x, int16_t y,
                                        uint32_t cp, int sz, uint16_t color) {
  for (uint8_t i = 0; i < lemonIconCount; i++) {
    if (pgm_read_dword(&lemonIconCPs[i]) == cp) {
      const GFXglyph* g = &lemonIconGlyphs[i];
      uint8_t w = pgm_read_byte(&g->width), h = pgm_read_byte(&g->height);
      int8_t  xo = (int8_t)pgm_read_byte(&g->xOffset), yo = (int8_t)pgm_read_byte(&g->yOffset);
      uint8_t xa = pgm_read_byte(&g->xAdvance);
      uint16_t bo = pgm_read_word(&g->bitmapOffset);
      uint8_t bits = 0, bit = 0;
      for (uint8_t row = 0; row < h; row++)
        for (uint8_t col = 0; col < w; col++) {
          if (!bit) { bits = pgm_read_byte(&lemonIconBitmaps[bo++]); bit = 0x80; }
          if (bits & bit) {
            // The UI icons keep a +6 baseline; the font glyphs below use +7
            // (misc-fixed's ascent), so the icons sit 1px higher in their cell.
            if (sz == 1) gfx.drawPixel(x + xo + col, y + 6 + yo + row, color);
            else gfx.fillRect(x + xo*sz + col*sz, y + 6*sz + yo*sz + row*sz, sz, sz, color);
          }
          bit >>= 1;
        }
      return x + xa * sz;
    }
  }

  if (cp < MiscFixed.first || cp > MiscFixed.last) {
    // Substitution box for anything the font doesn't cover. Drawn at plain `y`
    // (not y - 7*sz): `y` is already the ascent top, so the box stays inside
    // its own row instead of bleeding into the one above.
    if (cp >= 0x20) gfx.fillRect(x + sz, y, 4*sz, 6*sz, color);
    return x + 6 * sz;
  }

  const GFXglyph* g = &MiscFixedGlyphs[cp - MiscFixed.first];
  uint8_t w = pgm_read_byte(&g->width), h = pgm_read_byte(&g->height);
  int8_t  xo = (int8_t)pgm_read_byte(&g->xOffset), yo = (int8_t)pgm_read_byte(&g->yOffset);
  uint8_t xa = pgm_read_byte(&g->xAdvance);
  uint16_t bo = pgm_read_word(&g->bitmapOffset);
  uint8_t bits = 0, bit = 0;
  for (uint8_t row = 0; row < h; row++)
    for (uint8_t col = 0; col < w; col++) {
      if (!bit) { bits = pgm_read_byte(&MiscFixedBitmaps[bo++]); bit = 0x80; }
      if (bits & bit) {
        if (sz == 1) gfx.drawPixel(x + xo + col, y + 7 + yo + row, color);
        else gfx.fillRect(x + xo*sz + col*sz, y + 7*sz + yo*sz + row*sz, sz, sz, color);
      }
      bit >>= 1;
    }
  return x + xa * sz;
}

// Draw a UTF-8 string from the driver's current cursor, honouring '\n', and
// leave the cursor where the text ended (same contract as GFX's print()).
static inline void miscFixedPrint(Adafruit_GFX& gfx, const char* str, int sz, uint16_t color) {
  int16_t cx = gfx.getCursorX();
  int16_t cy = gfx.getCursorY();
  const uint8_t* p = (const uint8_t*)str;
  while (*p) {
    uint32_t cp = DisplayDriver::decodeCodepoint(p);
    if (cp == '\n') { cy += MiscFixed.yAdvance * sz; cx = 0; }
    else            { cx = miscFixedDrawGlyph(gfx, cx, cy, cp, sz, color); }
  }
  gfx.setCursor(cx, cy);
}

static inline uint16_t miscFixedTextWidth(const char* str, int sz) {
  uint16_t width = 0;
  const uint8_t* p = (const uint8_t*)str;
  while (*p) width += miscFixedXAdvance(DisplayDriver::decodeCodepoint(p), sz);
  return width;
}
