#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>
#include <helpers/RefCountedDigitalPin.h>

#ifndef PIN_OLED_RESET
  #define PIN_OLED_RESET        21 // Reset pin # (or -1 if sharing Arduino reset pin)
#endif

#ifndef DISPLAY_ADDRESS
  #define DISPLAY_ADDRESS   0x3C
#endif

class SSD1306Display : public DisplayDriver {
  Adafruit_SSD1306 display;
  bool _isOn;
  uint8_t _color;
  int _text_sz = 1;
  RefCountedDigitalPin* _peripher_power;

  bool i2c_probe(TwoWire& wire, uint8_t addr);
#ifdef OLED_MISC_FIXED_FONT
  // Thin wrapper over the shared misc-fixed renderer (MiscFixedRenderer.h), kept
  // out of this header so the font tables land in one translation unit only.
  uint8_t glyphXAdvance(uint32_t cp);
#endif
public:
  SSD1306Display(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64), 
      display(128, 64, &Wire, PIN_OLED_RESET),
      _peripher_power(peripher_power)
  {
    _isOn = false; 
  }
  bool begin();

  int getCharWidth()  const override { return 6 * _text_sz; }
#ifdef OLED_MISC_FIXED_FONT
  // Same misc-fixed 6x9 font the SH1106 and e-ink drivers use: full
  // Latin/Greek/Cyrillic instead of the built-in font's ASCII-with-blocks, so
  // the keyboard's alphabets and accented names render as themselves. Opt-in
  // per variant (the font costs ~14 KB of flash) — see the boards that set
  // OLED_MISC_FIXED_FONT in their platformio.ini.
  int getLineHeight() const override { return 9 * _text_sz; }   // 6x9 box height
  // The font's own glyph table is ink-tight, unlike the built-in font (below).
  int textWidthTrailingGap() const override { return 0; }
  bool isSingleFont() const override { return true; }
  void setSingleFont(bool) override { }   // single-font: ignore toggles
  void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) override {
    // No transliteration needed — print() renders UTF-8 directly.
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
  }
  uint16_t getCodepointWidth(uint32_t cp) override { return glyphXAdvance(cp); }
#else
  int getLineHeight() const override { return 8 * _text_sz; }
  // Classic Adafruit_GFX built-in font pads every measured string by one
  // trailing advance column (see DisplayDriver::textWidthTrailingGap()).
  int textWidthTrailingGap() const override { return 1; }
#endif

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
  
#ifdef ENABLE_SCREENSHOT
  const uint8_t* getBuffer() override { return display.getBuffer(); }
  uint16_t getBufferSize() override { return (uint16_t)((width() * height()) / 8); }
  uint8_t getDisplayType() override { return 0; }
#endif
};
