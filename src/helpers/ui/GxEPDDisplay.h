#pragma once

#include <SPI.h>
#include <Wire.h>

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include "DisplayDriver.h"

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 0
#endif

// Panel native dimensions before rotation. Derived from the model class when
// EINK_DISPLAY_MODEL is set; override with EINK_PANEL_W/H for other panels.
#if defined(EINK_DISPLAY_MODEL)
  #ifndef EINK_PANEL_W
    #define EINK_PANEL_W EINK_DISPLAY_MODEL::WIDTH
  #endif
  #ifndef EINK_PANEL_H
    #define EINK_PANEL_H EINK_DISPLAY_MODEL::HEIGHT
  #endif
#else
  #ifndef EINK_PANEL_W
    #define EINK_PANEL_W 200
    #define EINK_PANEL_H 200
  #endif
#endif

// Odd rotations (1, 3) swap width and height.
#define EINK_DISP_W ((DISPLAY_ROTATION & 1) ? EINK_PANEL_H : EINK_PANEL_W)
#define EINK_DISP_H ((DISPLAY_ROTATION & 1) ? EINK_PANEL_W : EINK_PANEL_H)

class GxEPDDisplay : public DisplayDriver {
#if defined(EINK_DISPLAY_MODEL)
  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
#else
  GxEPD2_BW<GxEPD2_150_BN, 200> display;
#endif
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;
  int _text_sz = 1;

public:
#if defined(EINK_DISPLAY_MODEL)
  GxEPDDisplay() : DisplayDriver(EINK_DISP_W, EINK_DISP_H),
    display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
#else
  GxEPDDisplay() : DisplayDriver(EINK_DISP_W, EINK_DISP_H),
    display(GxEPD2_150_BN(DISP_CS, DISP_DC, DISP_RST, DISP_BUSY)) {}
#endif

  // Line height and approx. char width for each font size:
  //   1 = FreeSans9pt      (lineH=16, charW≈9)
  //   2 = FreeSansBold12pt (lineH=20, charW≈12)
  //   3 = FreeSans18pt     (lineH=28, charW≈17)
  int getCharWidth()  const override { return _text_sz == 3 ? 17 : _text_sz == 2 ? 12 : 9; }
  int getLineHeight() const override { return _text_sz == 3 ? 28 : _text_sz == 2 ? 20 : 16; }

  bool begin();

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
  void setDisplayRotation(uint8_t rot) override;
  void endFrame() override;
};
