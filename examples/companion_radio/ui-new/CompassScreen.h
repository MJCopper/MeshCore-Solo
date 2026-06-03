#pragma once
// Tools › Compass. A standalone heading view: shows the device's course over
// ground (derived from GPS movement — there is no magnetometer) as a rotating
// arrow plus a numeric degrees + cardinal readout. When standing still the
// heading is undefined, so it shows a hint to move.
//
// Reuses UITask's COG ring (currentCourse) — works whether or not a trail is
// being recorded.

#include "../GeoUtils.h"
#include "GfxUtils.h"
#include <math.h>

class CompassScreen : public UIScreen {
  UITask* _task;

  bool gpsValid() const { int32_t lat, lon; return _task->currentLocation(lat, lon); }

public:
  CompassScreen(UITask* task) : _task(task) {}
  void enter() {}

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0, "COMPASS");
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    const int hdr = display.headerH();
    const int cx  = display.width() / 2;
    // Reserve a size-2 row at the bottom for the numeric readout.
    display.setTextSize(2);
    const int bigH = display.getLineHeight();
    display.setTextSize(1);
    const int top = hdr + 2;
    const int bottom = display.height() - bigH - 2;
    const int cy  = (top + bottom) / 2;
    int r = ((bottom - top) / 2);
    int rx = (display.width() / 2) - 4;
    if (rx < r) r = rx;
    if (r < 6) r = 6;

    if (!gpsValid()) {
      display.drawTextCentered(cx, cy - display.getLineHeight() / 2, "No GPS fix");
      return 1000;
    }

    int cog;
    bool have = _task->currentCourse(cog);

    gfx::drawCircle(display, cx, cy, r);   // compass ring

    // Fixed forward index at the top: the direction you are travelling is
    // always "up". The compass card (North) rotates underneath it.
    display.fillRect(cx - 1, cy - r - 2, 3, 1);
    display.fillRect(cx,     cy - r - 1, 1, 2);

    if (!have) {
      display.drawTextCentered(cx, cy - display.getLineHeight() / 2, "move to");
      display.drawTextCentered(cx, cy + display.getLineHeight() / 2, "set heading");
      return 1000;
    }

    // Heads-up compass: with "up" = course, an absolute bearing B sits at
    // screen angle (B - cog). Draw the rotating card — a North needle plus the
    // other three cardinals as letters around the ring.
    auto cardPos = [&](int bearing, int rad_px, int& px, int& py) {
      float a = (bearing - cog) * (float)M_PI / 180.0f;
      px = cx + (int)(sinf(a) * rad_px);
      py = cy - (int)(cosf(a) * rad_px);
    };

    // North needle from centre.
    int nx, ny; cardPos(0, r - 2, nx, ny);
    gfx::drawLine(display, cx, cy, nx, ny);
    float nrad = (0 - cog) * (float)M_PI / 180.0f;
    for (int da = -25; da <= 25; da += 50) {
      float a = nrad + (float)M_PI + da * (float)M_PI / 180.0f;
      gfx::drawLine(display, nx, ny, nx + (int)(sinf(a) * 4), ny - (int)(cosf(a) * 4));
    }

    // Cardinal letters around the rim (N at the needle tip).
    const int cw = display.getCharWidth(), ch = display.getLineHeight();
    struct { int b; const char* s; } card[4] = { {0,"N"},{90,"E"},{180,"S"},{270,"W"} };
    for (int i = 0; i < 4; i++) {
      int lx, ly; cardPos(card[i].b, r - 2, lx, ly);
      display.setCursor(lx - cw / 2, ly - ch / 2);
      display.print(card[i].s);
    }

    // Numeric readout below the ring — the absolute course you're travelling.
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %s", cog, geo::bearingCardinal(cog));
    display.setTextSize(2);
    display.drawTextCentered(cx, bottom + 1, buf);
    display.setTextSize(1);
    return 1000;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { _task->gotoToolsScreen(); return true; }
    return true;
  }
};
