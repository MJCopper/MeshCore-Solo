#pragma once
// Tools › Compass. A standalone heading view: shows the device's course over
// ground (derived from GPS movement — there is no magnetometer) as a rotating
// arrow plus a numeric degrees + cardinal readout. When standing still the
// heading is undefined, so it shows a hint to move.
//
// Reuses UITask's COG ring (currentCourse) — works whether or not a trail is
// being recorded.

#include "../GeoUtils.h"
#include <math.h>

class CompassScreen : public UIScreen {
  UITask* _task;

  static bool gpsValid() {
#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    return loc && loc->isValid();
#else
    return false;
#endif
  }

  static void drawLine(DisplayDriver& d, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      d.fillRect(x0, y0, 1, 1);
      if (x0 == x1 && y0 == y1) break;
      int e2 = err * 2;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }

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

    // Compass ring ticks: N/E/S/W as short marks; N drawn as a filled wedge top.
    display.drawRect(cx - r, cy - r, 2 * r + 1, 2 * r + 1);   // bounding box stands in for a ring
    display.drawTextCentered(cx, cy - r - 1, "N");

    if (!have) {
      display.drawTextCentered(cx, cy - display.getLineHeight() / 2, "move to");
      display.drawTextCentered(cx, cy + display.getLineHeight() / 2, "set heading");
      return 1000;
    }

    // Arrow from centre toward the course (0° = up = north, clockwise).
    float rad = cog * (float)M_PI / 180.0f;
    int ex = cx + (int)(sinf(rad) * (r - 2));
    int ey = cy - (int)(cosf(rad) * (r - 2));
    drawLine(display, cx, cy, ex, ey);
    // Small arrowhead: two short lines back from the tip at ±150°.
    for (int da = -25; da <= 25; da += 50) {
      float a = rad + (float)M_PI + da * (float)M_PI / 180.0f;
      int hx = ex + (int)(sinf(a) * 4);
      int hy = ey - (int)(cosf(a) * 4);
      drawLine(display, ex, ey, hx, hy);
    }

    // Numeric readout below the ring.
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
