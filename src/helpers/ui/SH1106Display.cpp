#include "SH1106Display.h"
#include <Adafruit_GrayOLED.h>
#include "Adafruit_SH110X.h"
#include "MiscFixedFont.h"
#include "LemonIcons.h"

bool SH1106Display::i2c_probe(TwoWire &wire, uint8_t addr)
{
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

bool SH1106Display::begin()
{
  return display.begin(DISPLAY_ADDRESS, true) && i2c_probe(Wire, DISPLAY_ADDRESS);
}

void SH1106Display::turnOn()
{
  display.oled_command(SH110X_DISPLAYON);
  uint8_t pre[] = { 0xD9, _precharge };
  display.oled_commandList(pre, 2);
  display.setContrast(_contrast);
  _isOn = true;
  _force_redraw = true;   // panel was off — guarantee the next endFrame() flushes
}

void SH1106Display::turnOff()
{
  display.oled_command(SH110X_DISPLAYOFF);
  _isOn = false;
}

void SH1106Display::clear()
{
  display.clearDisplay();
  display.display();
  _force_redraw = true;   // next endFrame() must flush even if its CRC matches a pre-clear frame
}

void SH1106Display::startFrame(Color bkg)
{
  display.clearDisplay(); // TODO: apply 'bkg'
  _color = SH110X_WHITE;
  display.setTextColor(_color);
  display.setTextSize(1);
  _text_sz = 1;
  display.cp437(true); // Use full 256 char 'Code Page 437' font
}

void SH1106Display::setTextSize(int sz)
{
  _text_sz = sz;
  _vw_dirty = true;
  display.setTextSize(sz);
}

void SH1106Display::setColor(Color c)
{
  _color = (c != 0) ? SH110X_WHITE : SH110X_BLACK;
  display.setTextColor(_color);
}

void SH1106Display::setCursor(int x, int y)
{
  display.setCursor(x, y);
}

int16_t SH1106Display::drawLemonChar(int16_t x, int16_t y, uint32_t cp) {
  int sz = _text_sz;
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
            if (sz == 1) display.drawPixel(x + xo + col, y + 6 + yo + row, _color);
            else display.fillRect(x + xo*sz + col*sz, y + 6*sz + yo*sz + row*sz, sz, sz, _color);
          }
          bit >>= 1;
        }
      return x + xa * sz;
    }
  }

  // Font glyphs come from misc-fixed 6x9 (full Latin/Greek/Cyrillic, ascent 7) —
  // baseline +7. The custom UI icons above keep +6, so they sit 1px higher.
  if (cp < MiscFixed.first || cp > MiscFixed.last) {
    // y here is the top of the current text row, i.e. already baseline minus
    // the font's 7px ascent (see real-glyph rendering just below: y + 7 + yo +
    // row) -- unlike GxEPDDisplay's drawLemonChar, where y IS the baseline and
    // the box sits at y - 7*sc for the same reason. Drawing this box at plain
    // `y` (not y - 7*sz) lands it in the same ascent-top position, within its
    // own row instead of bleeding into the row above.
    if (cp >= 0x20) display.fillRect(x + sz, y, 4*sz, 6*sz, _color);
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
        if (sz == 1) display.drawPixel(x + xo + col, y + 7 + yo + row, _color);
        else display.fillRect(x + xo*sz + col*sz, y + 7*sz + yo*sz + row*sz, sz, sz, _color);
      }
      bit >>= 1;
    }
  return x + xa * sz;
}

uint8_t SH1106Display::lemonXAdvance(uint32_t cp) {
  uint8_t xa;
  if (cp < MiscFixed.first || cp > MiscFixed.last) xa = 6;
  else xa = pgm_read_byte(&MiscFixedGlyphs[cp - MiscFixed.first].xAdvance);
  return xa * _text_sz;
}

void SH1106Display::translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
  if (_use_lemon) {
    size_t n = strlen(src);
    if (n >= dest_size) n = dest_size - 1;
    memcpy(dest, src, n);
    dest[n] = '\0';
  } else {
    DisplayDriver::translateUTF8ToBlocks(dest, src, dest_size);
  }
}

void SH1106Display::print(const char *str)
{
  if (!_use_lemon) { display.print(str); return; }

  int16_t cx = display.getCursorX();
  int16_t cy = display.getCursorY();
  const uint8_t* p = (const uint8_t*)str;
  while (*p) {
    uint32_t cp = decodeCodepoint(p);
    if (cp == '\n') { cy += MiscFixed.yAdvance; cx = 0; }
    else            { cx = drawLemonChar(cx, cy, cp); }
  }
  display.setCursor(cx, cy);
}

void SH1106Display::fillRect(int x, int y, int w, int h)
{
  display.fillRect(x, y, w, h, _color);
}

void SH1106Display::drawRect(int x, int y, int w, int h)
{
  display.drawRect(x, y, w, h, _color);
}

void SH1106Display::drawXbm(int x, int y, const uint8_t *bits, int w, int h)
{
  display.drawBitmap(x, y, bits, w, h, SH110X_WHITE);
}

uint16_t SH1106Display::getTextWidth(const char *str)
{
  if (_use_lemon) {
    uint16_t width = 0;
    const uint8_t* p = (const uint8_t*)str;
    while (*p) width += lemonXAdvance(decodeCodepoint(p));
    return width;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void SH1106Display::setBrightness(uint8_t level)
{
  // Contrast alone has limited effect on some OLED panels; combining with
  // pre-charge period (0xD9) gives a wider perceptible dimming range.
  // Pre-charge 0x11 = phase1=1,phase2=1 (minimum drive); 0x1F = default.
  static const uint8_t contrast_values[]  = {   0,  25,  60, 150, 255 };
  static const uint8_t precharge_values[] = { 0x11, 0x15, 0x1F, 0x1F, 0x1F };
  uint8_t idx = level < 5 ? level : 4;
  _contrast  = contrast_values[idx];
  _precharge = precharge_values[idx];
  uint8_t pre[] = { 0xD9, _precharge };
  display.oled_commandList(pre, 2);
  display.setContrast(_contrast);
}

void SH1106Display::endFrame()
{
  // Skip the I²C flush when the frame is byte-identical to the last one pushed.
  // The most-shown screens (clock, home) are static between updates, so this
  // cuts redundant display() traffic and a little power. FNV-1a over the 1 KB
  // GFX buffer (~1k xor+mul, cheap); _force_redraw guarantees the first frame
  // and the frame after wake/clear.
  const uint8_t* buf = display.getBuffer();
  uint16_t n = (uint16_t)((width() * height()) / 8);
  uint32_t h = 2166136261u;
  for (uint16_t i = 0; i < n; i++) { h ^= buf[i]; h *= 16777619u; }
  if (!_force_redraw && h == _last_frame_hash) return;
  _force_redraw = false;
  _last_frame_hash = h;
  display.display();
}
