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
  int     _summary_scroll = 0;  // top visible item index in Summary view

  static const int SUMMARY_ITEM_COUNT = 5;

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
    if (_view == V_SUMMARY && c == KEY_UP   && _summary_scroll > 0) { _summary_scroll--; return true; }
    if (_view == V_SUMMARY && c == KEY_DOWN)                        { _summary_scroll++; return true; }  // clamped on render
    if (c == KEY_ENTER) {
      _store->setActive(!_store->isActive());
      _task->showAlert(_store->isActive() ? "Tracking started" : "Tracking stopped", 800);
      return true;
    }
    return false;
  }

private:
  // Format the i-th summary line into buf. Indices match SUMMARY_ITEM_COUNT.
  void summaryItem(int i, char* buf, size_t n) const {
    switch (i) {
      case 0:
        snprintf(buf, n, "Status: %s", _store->isActive() ? "tracking" : "stopped");
        break;
      case 1:
        snprintf(buf, n, "Points: %d / %d", _store->count(), TrailStore::CAPACITY);
        break;
      case 2: {
        uint32_t d = _store->totalDistanceMeters();
        if (d < 1000) snprintf(buf, n, "Dist: %lu m", (unsigned long)d);
        else          snprintf(buf, n, "Dist: %lu.%02lu km",
                                (unsigned long)(d / 1000), (unsigned long)((d % 1000) / 10));
        break;
      }
      case 3: {
        uint32_t es = _store->elapsedSeconds((uint32_t)rtc_clock.getCurrentTime());
        // Below 1 h show m:ss so the seconds counter updates visibly each refresh.
        if (es < 3600) snprintf(buf, n, "Time: %lu:%02lu",
                                  (unsigned long)(es / 60), (unsigned long)(es % 60));
        else           snprintf(buf, n, "Time: %lu:%02lu",
                                  (unsigned long)(es / 3600), (unsigned long)((es % 3600) / 60));
        break;
      }
      case 4:
        snprintf(buf, n, "Avg speed: %u km/h",
                  (unsigned)_store->avgSpeedKmh((uint32_t)rtc_clock.getCurrentTime()));
        break;
      default:
        buf[0] = '\0';
    }
  }

  void renderSummary(DisplayDriver& display) {
    const int y0     = display.listStart();
    const int step   = display.lineStep();
    const int hint_h = step;
    const int avail  = display.height() - y0 - hint_h - 2;
    int visible      = avail / step;
    if (visible < 1) visible = 1;
    if (visible > SUMMARY_ITEM_COUNT) visible = SUMMARY_ITEM_COUNT;

    // Clamp scroll once we know visible count.
    int max_scroll = SUMMARY_ITEM_COUNT - visible;
    if (_summary_scroll > max_scroll) _summary_scroll = max_scroll;
    if (_summary_scroll < 0)          _summary_scroll = 0;

    for (int i = 0; i < visible; i++) {
      int idx = _summary_scroll + i;
      if (idx >= SUMMARY_ITEM_COUNT) break;
      char buf[28];
      summaryItem(idx, buf, sizeof(buf));
      display.setCursor(2, y0 + i * step);
      display.print(buf);
    }

    // Scroll arrows on the right edge when there's more above/below.
    int cw = display.getCharWidth();
    if (_summary_scroll > 0) {
      display.setCursor(display.width() - cw, y0);
      display.print("^");
    }
    if (_summary_scroll + visible < SUMMARY_ITEM_COUNT) {
      display.setCursor(display.width() - cw, y0 + (visible - 1) * step);
      display.print("v");
    }
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

    // Draw connected segments. A point flagged SEG_START starts a new segment;
    // its predecessor is not joined to it. Single-point segments still get a
    // visible pixel.
    int x0, y0;
    project(_store->at(0), x0, y0);
    display.fillRect(x0, y0, 1, 1);
    for (int i = 1; i < _store->count(); i++) {
      int x1, y1;
      project(_store->at(i), x1, y1);
      if (_store->at(i).flags & TRAIL_FLAG_SEG_START) {
        display.fillRect(x1, y1, 1, 1);  // mark the segment's first point
      } else {
        drawLine(display, x0, y0, x1, y1);
      }
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
