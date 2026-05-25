#pragma once
// GPS trail viewer. Tools › Trail.
// Summary view: counters, distance, time, speed.
// Map view: pixel-by-pixel auto-fit of the polyline with start (+) and current (×) markers.
// Phase 3 adds a per-point list view.
// Included by UITask.cpp after Trail store + ToolsScreen.

#include "../Trail.h"
#include <math.h>

class TrailScreen : public UIScreen {
  UITask*     _task;
  TrailStore* _store;

  enum View { V_SUMMARY = 0, V_MAP = 1, V_COUNT };
  uint8_t _view = V_SUMMARY;

public:
  TrailScreen(UITask* task, TrailStore* store) : _task(task), _store(store) {}

  void enter() { /* nothing to reset; live trail stays */ }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0,
                              _view == V_MAP ? "TRAIL MAP" : "TRAIL");
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    if (_view == V_MAP) renderMap(display);
    else                renderSummary(display);

    // Bottom hint — current view indicator + control reminders.
    display.setColor(DisplayDriver::LIGHT);
    int hint_y = display.height() - display.lineStep();
    char hint[28];
    snprintf(hint, sizeof(hint), "<>%d/%d  [Ent] %s",
             (int)_view + 1, (int)V_COUNT,
             _store->isActive() ? "stop" : "start");
    display.setCursor(2, hint_y);
    display.print(hint);

    return _store->isActive() ? 2000 : 5000;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_LEFT  || c == KEY_PREV) { _view = (_view + V_COUNT - 1) % V_COUNT; return true; }
    if (c == KEY_RIGHT || c == KEY_NEXT) { _view = (_view + 1)           % V_COUNT; return true; }
    if (c == KEY_ENTER) {
      _store->setActive(!_store->isActive());
      _task->showAlert(_store->isActive() ? "Tracking started" : "Tracking stopped", 800);
      return true;
    }
    return false;
  }

private:
  void renderSummary(DisplayDriver& display) {
    const int y0   = display.listStart();
    const int step = display.lineStep();

    char buf[28];

    snprintf(buf, sizeof(buf), "Status: %s",
             _store->isActive() ? "tracking" : "stopped");
    display.setCursor(2, y0);
    display.print(buf);

    snprintf(buf, sizeof(buf), "Points: %d / %d", _store->count(), TrailStore::CAPACITY);
    display.setCursor(2, y0 + step);
    display.print(buf);

    uint32_t dist = _store->totalDistanceMeters();
    if (dist < 1000) snprintf(buf, sizeof(buf), "Dist: %lu m", (unsigned long)dist);
    else             snprintf(buf, sizeof(buf), "Dist: %lu.%02lu km",
                              (unsigned long)(dist / 1000),
                              (unsigned long)((dist % 1000) / 10));
    display.setCursor(2, y0 + step * 2);
    display.print(buf);

    uint32_t es = _store->elapsedSeconds();
    snprintf(buf, sizeof(buf), "Time: %lu:%02lu",
             (unsigned long)(es / 3600),
             (unsigned long)((es % 3600) / 60));
    display.setCursor(2, y0 + step * 3);
    display.print(buf);

    snprintf(buf, sizeof(buf), "Speed: %u km/h", (unsigned)_store->currentSpeedKmh());
    display.setCursor(2, y0 + step * 4);
    display.print(buf);
  }

  // Pixel-by-pixel polyline. Bounds are auto-fitted to the available area and
  // longitude is scaled by cos(avg_lat) so high-latitude trails don't stretch.
  void renderMap(DisplayDriver& display) {
    const int top    = display.listStart();
    const int bottom = display.height() - display.lineStep() - 1;

    if (_store->empty()) {
      display.drawTextCentered(display.width() / 2, (top + bottom) / 2, "No trail yet");
      return;
    }

    const int area_x = 2;
    const int area_y = top + 1;
    const int area_w = display.width() - 4;
    const int area_h = bottom - top - 2;
    if (area_w < 6 || area_h < 6) return;  // no room to draw anything sensible

    int32_t min_lat, min_lon, max_lat, max_lon;
    _store->boundingBox(min_lat, min_lon, max_lat, max_lon);

    if (_store->count() == 1 || (min_lat == max_lat && min_lon == max_lon)) {
      // Degenerate — single point or all samples colocated. Mark centre.
      drawCurrentMarker(display, area_x + area_w / 2, area_y + area_h / 2);
      return;
    }

    // cos(lat) correction prevents east/west stretching at higher latitudes.
    float avg_lat_rad = ((min_lat + max_lat) / 2.0e6f) * (float)M_PI / 180.0f;
    float lon_scale_geo = cosf(avg_lat_rad);
    if (lon_scale_geo < 0.05f) lon_scale_geo = 0.05f;  // guard near poles

    float lat_span = (float)(max_lat - min_lat);
    float lon_span = (float)(max_lon - min_lon) * lon_scale_geo;

    // Pick the limiting axis so the polyline fills the area without distorting.
    float scale_lat = (float)area_h / (lat_span > 0 ? lat_span : 1.0f);
    float scale_lon = (float)area_w / (lon_span > 0 ? lon_span : 1.0f);
    float scale     = (scale_lat < scale_lon) ? scale_lat : scale_lon;

    // After scaling, the used area is smaller in the non-limiting dim; centre it.
    int used_w = (int)(lon_span * scale);
    int used_h = (int)(lat_span * scale);
    int off_x  = area_x + (area_w - used_w) / 2;
    int off_y  = area_y + (area_h - used_h) / 2;

    auto project = [&](const TrailPoint& p, int& px, int& py) {
      float dx = (float)(p.lon_1e6 - min_lon) * lon_scale_geo * scale;
      float dy = (float)(max_lat - p.lat_1e6) * scale;  // north = up
      px = off_x + (int)dx;
      py = off_y + (int)dy;
    };

    int x0, y0;
    project(_store->at(0), x0, y0);
    for (int i = 1; i < _store->count(); i++) {
      int x1, y1;
      project(_store->at(i), x1, y1);
      drawLine(display, x0, y0, x1, y1);
      x0 = x1;
      y0 = y1;
    }

    int sx, sy, ex, ey;
    project(_store->first(), sx, sy);
    project(_store->last(),  ex, ey);
    drawStartMarker(display, sx, sy);
    drawCurrentMarker(display, ex, ey);
  }

  // Bresenham line via 1×1 fillRect. Acceptable for occasional view renders.
  static void drawLine(DisplayDriver& d, int x0, int y0, int x1, int y1) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
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

  // 5×5 plus-sign marker for the start point.
  static void drawStartMarker(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 2, cy,     5, 1);
    d.fillRect(cx,     cy - 2, 1, 5);
  }

  // 5×5 cross marker for the current/last point.
  static void drawCurrentMarker(DisplayDriver& d, int cx, int cy) {
    for (int i = -2; i <= 2; i++) {
      d.fillRect(cx + i, cy + i, 1, 1);
      d.fillRect(cx + i, cy - i, 1, 1);
    }
  }
};
