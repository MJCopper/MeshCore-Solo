#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SH110X_NO_SPLASH
#include <Adafruit_SH110X.h>

#ifndef PIN_OLED_RESET
#define PIN_OLED_RESET -1
#endif

#ifndef DISPLAY_ADDRESS
#define DISPLAY_ADDRESS 0x3C
#endif

class SH1106Display : public DisplayDriver
{
  Adafruit_SH1106G display;
  bool _isOn;
  uint8_t _color;
  uint8_t _contrast;
  uint8_t _precharge;
  bool _use_lemon;
  int  _text_sz;

  bool i2c_probe(TwoWire &wire, uint8_t addr);
  int16_t drawLemonChar(int16_t x, int16_t y, uint32_t cp);
  uint8_t lemonXAdvance(uint32_t cp);

public:
  SH1106Display() : DisplayDriver(128, 64), display(128, 64, &Wire, PIN_OLED_RESET) {
    _isOn = false; _contrast = 255; _precharge = 0x1F; _use_lemon = false; _text_sz = 1;
  }
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char *str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t *bits, int w, int h) override;
  uint16_t getTextWidth(const char *str) override;
  uint16_t getCodepointWidth(uint32_t cp) override {
    if (_use_lemon) return lemonXAdvance(cp);
    return 6 * _text_sz;  // built-in 5x7 font: 6 px advance per glyph
  }
  int getCharWidth() const override { return (_use_lemon ? 5 : 6) * _text_sz; }
  int getLineHeight() const override { return (_use_lemon ? 9 : 8) * _text_sz; }
  void setLemonFont(bool enabled) override { _use_lemon = enabled; _vw_dirty = true; }
  bool isLemonFont() const override { return _use_lemon; }
  void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) override;
  void setBrightness(uint8_t level) override;
  void endFrame() override;
};
