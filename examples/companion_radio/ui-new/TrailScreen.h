#pragma once
// GPS trail viewer. Tools › Trail.
// Config view (stopped): shows the Min-dist setting and lets Enter start tracking.
// Active views (running): Summary counters, Map polyline, per-point List.
// Included by UITask.cpp after Trail store + ToolsScreen.

#include "../Trail.h"
#include <math.h>

class TrailScreen : public UIScreen {
  UITask*     _task;
  TrailStore* _store;

  // V_CONFIG is the resting screen — shown whenever the trail is stopped.
  // Once tracking starts, the user cycles between V_SUMMARY/V_MAP/V_LIST with
  // LEFT/RIGHT. Stop returns to V_CONFIG.
  enum View { V_CONFIG = 0, V_SUMMARY = 1, V_MAP = 2, V_LIST = 3 };
  static const uint8_t ACTIVE_VIEW_COUNT = 3;  // Summary, Map, List

  uint8_t _view           = V_CONFIG;
  int     _summary_scroll = 0;  // top visible item index in Summary view
  int     _list_scroll    = 0;  // top visible row in List view (newest = row 0)
  bool    _cfg_dirty      = false;  // unsaved Config edits; flushed on start or back

  // Selected row in the Config view. UP/DOWN moves between rows;
  // LEFT/RIGHT cycles the value of the focused row.
  enum CfgRow { CFG_MIN_DIST = 0, CFG_UNITS = 1, CFG_ROW_COUNT };
  uint8_t _cfg_sel = CFG_MIN_DIST;

  static const int SUMMARY_ITEM_COUNT = 5;

public:
  TrailScreen(UITask* task, TrailStore* store) : _task(task), _store(store) {}

  void enter() {
    _view = _store->isActive() ? V_SUMMARY : V_CONFIG;
    _summary_scroll = 0;
    _list_scroll    = 0;
    _cfg_dirty      = false;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // Snap _view to whatever the active-state allows. Stopped → always V_CONFIG;
    // active and stuck on V_CONFIG → flip to Summary.
    if (!_store->isActive() && _view != V_CONFIG)             _view = V_CONFIG;
    if ( _store->isActive() && _view == V_CONFIG)             _view = V_SUMMARY;

    const char* title = (_view == V_MAP)   ? "TRAIL MAP"
                      : (_view == V_LIST)  ? "TRAIL LIST"
                      :                       "TRAIL";
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    if      (_view == V_CONFIG)  renderConfig(display);
    else if (_view == V_MAP)     renderMap(display);
    else if (_view == V_LIST)    renderList(display);
    else                          renderSummary(display);

    // Bottom hint
    display.setColor(DisplayDriver::LIGHT);
    int hint_y = display.height() - display.lineStep();
    display.setCursor(2, hint_y);
    if (_view == V_CONFIG) {
      display.print(_store->empty() ? "<>dist [Ent] start" : "<>dist [Ent] resume");
    } else {
      char hint[28];
      snprintf(hint, sizeof(hint), "<>%d/%d  [Ent] stop",
                (int)(_view - V_SUMMARY + 1), (int)ACTIVE_VIEW_COUNT);
      display.print(hint);
    }

    return _store->isActive() ? 1000 : 5000;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      if (_cfg_dirty) { the_mesh.savePrefs(); _cfg_dirty = false; }
      _task->gotoToolsScreen();
      return true;
    }

    if (_view == V_CONFIG) {
      NodePrefs* p = _task->getNodePrefs();
      if (c == KEY_UP   && _cfg_sel > 0)                  { _cfg_sel--; return true; }
      if (c == KEY_DOWN && _cfg_sel < CFG_ROW_COUNT - 1)  { _cfg_sel++; return true; }
      if ((c == KEY_LEFT  || c == KEY_PREV) && p) { cycleCfg(p, -1); return true; }
      if ((c == KEY_RIGHT || c == KEY_NEXT) && p) { cycleCfg(p, +1); return true; }
      if (c == KEY_ENTER) {
        if (_cfg_dirty) { the_mesh.savePrefs(); _cfg_dirty = false; }
        _store->setActive(true);
        _view = V_SUMMARY;
        _task->showAlert(gpsHasFix() ? "Tracking started" : "Waiting for GPS fix", 1000);
        return true;
      }
      return false;
    }

    // Active views: Summary / Map / List
    if (c == KEY_LEFT || c == KEY_PREV) {
      int v = (int)_view - V_SUMMARY;
      v = (v + ACTIVE_VIEW_COUNT - 1) % ACTIVE_VIEW_COUNT;
      _view = (uint8_t)(V_SUMMARY + v);
      return true;
    }
    if (c == KEY_RIGHT || c == KEY_NEXT) {
      int v = (int)_view - V_SUMMARY;
      v = (v + 1) % ACTIVE_VIEW_COUNT;
      _view = (uint8_t)(V_SUMMARY + v);
      return true;
    }
    if (_view == V_SUMMARY && c == KEY_UP   && _summary_scroll > 0) { _summary_scroll--; return true; }
    if (_view == V_SUMMARY && c == KEY_DOWN)                        { _summary_scroll++; return true; }
    if (_view == V_LIST    && c == KEY_UP   && _list_scroll > 0)    { _list_scroll--;    return true; }
    if (_view == V_LIST    && c == KEY_DOWN)                        { _list_scroll++;    return true; }
    if (c == KEY_ENTER) {
      _store->setActive(false);
      _view = V_CONFIG;
      _task->showAlert("Tracking stopped", 800);
      return true;
    }
    return false;
  }

  // True when the global SensorManager has a usable GPS fix.
  static bool gpsHasFix() {
#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    return loc && loc->isValid();
#else
    return false;
#endif
  }

private:
  // Cycle the focused Config row's value. Dirty flag commits on Start / Back.
  void cycleCfg(NodePrefs* p, int dir) {
    if (_cfg_sel == CFG_MIN_DIST) {
      uint8_t idx = p->trail_min_delta_idx;
      if (idx >= TrailStore::MIN_DELTA_COUNT) idx = 0;
      idx = (dir > 0) ? (idx + 1) % TrailStore::MIN_DELTA_COUNT
                      : (idx + TrailStore::MIN_DELTA_COUNT - 1) % TrailStore::MIN_DELTA_COUNT;
      p->trail_min_delta_idx = idx;
    } else if (_cfg_sel == CFG_UNITS) {
      uint8_t idx = p->trail_units_idx;
      if (idx >= TrailStore::UNITS_COUNT) idx = 0;
      idx = (dir > 0) ? (idx + 1) % TrailStore::UNITS_COUNT
                      : (idx + TrailStore::UNITS_COUNT - 1) % TrailStore::UNITS_COUNT;
      p->trail_units_idx = idx;
    }
    _cfg_dirty = true;
  }

  // Render the average speed (or pace) in the user-selected units.
  void formatAvgPaceOrSpeed(char* buf, size_t n, NodePrefs* p) const {
    uint8_t u = p ? p->trail_units_idx : 0;
    if (u >= TrailStore::UNITS_COUNT) u = 0;
    uint16_t kmh = _store->avgSpeedKmh();
    if (u == TrailStore::UNITS_KMH) {
      snprintf(buf, n, "Avg: %u km/h", (unsigned)kmh);
    } else if (u == TrailStore::UNITS_MPH) {
      // 1 km = 0.621371 mi
      unsigned mph = (unsigned)((float)kmh * 0.621371f + 0.5f);
      snprintf(buf, n, "Avg: %u mph", mph);
    } else {
      // Pace = elapsed / distance. Compute directly from raw totals for precision.
      uint32_t d_m  = _store->totalDistanceMeters();
      uint32_t es_s = _store->elapsedSeconds();
      const char* unit = (u == TrailStore::UNITS_PACE_KM) ? "/km" : "/mi";
      if (d_m == 0 || es_s == 0) {
        snprintf(buf, n, "Pace: --:-- %s", unit);
      } else {
        float dist_unit = (u == TrailStore::UNITS_PACE_KM) ? (d_m / 1000.0f)
                                                            : (d_m / 1609.344f);
        if (dist_unit <= 0) { snprintf(buf, n, "Pace: --:-- %s", unit); return; }
        float pace_min = (es_s / 60.0f) / dist_unit;
        unsigned pm = (unsigned)pace_min;
        unsigned ps = (unsigned)((pace_min - pm) * 60.0f);
        snprintf(buf, n, "Pace: %u:%02u %s", pm, ps, unit);
      }
    }
  }

  void renderConfig(DisplayDriver& display) {
    NodePrefs* p = _task->getNodePrefs();
    const int y0   = display.listStart();
    const int step = display.lineStep();
    const int cw   = display.getCharWidth();

    char buf[24];

    // Row 0 — Min dist
    display.setCursor(0, y0);
    display.print(_cfg_sel == CFG_MIN_DIST ? ">" : " ");
    snprintf(buf, sizeof(buf), "Min dist: %s",
              TrailStore::minDeltaLabel(p ? p->trail_min_delta_idx : 0));
    display.setCursor(cw + 2, y0);
    display.print(buf);

    // Row 1 — Units
    display.setCursor(0, y0 + step);
    display.print(_cfg_sel == CFG_UNITS ? ">" : " ");
    snprintf(buf, sizeof(buf), "Units:    %s",
              TrailStore::unitLabel(p ? p->trail_units_idx : 0));
    display.setCursor(cw + 2, y0 + step);
    display.print(buf);

    // Existing trail summary (if any) so the user sees what Enter will resume.
    if (!_store->empty()) {
      snprintf(buf, sizeof(buf), "Stored: %d pts", _store->count());
      display.setCursor(2, y0 + step * 3);
      display.print(buf);

      uint32_t d = _store->totalDistanceMeters();
      if (d < 1000) snprintf(buf, sizeof(buf), "Dist: %lu m", (unsigned long)d);
      else          snprintf(buf, sizeof(buf), "Dist: %lu.%02lu km",
                              (unsigned long)(d / 1000), (unsigned long)((d % 1000) / 10));
      display.setCursor(2, y0 + step * 4);
      display.print(buf);
    } else {
      display.setCursor(2, y0 + step * 3);
      display.print("No trail yet.");
    }
  }

  // Format the i-th summary line into buf. Indices match SUMMARY_ITEM_COUNT.
  void summaryItem(int i, char* buf, size_t n) const {
    switch (i) {
      case 0: {
        const char* st;
        if (!_store->isActive())     st = "stopped";
        else if (_store->empty())    st = "waiting fix";
        else                          st = "tracking";
        snprintf(buf, n, "Status: %s", st);
        break;
      }
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
        uint32_t es = _store->elapsedSeconds();
        // Below 1 h show m:ss so the seconds counter updates visibly each refresh.
        if (es < 3600) snprintf(buf, n, "Time: %lu:%02lu",
                                  (unsigned long)(es / 60), (unsigned long)(es % 60));
        else           snprintf(buf, n, "Time: %lu:%02lu",
                                  (unsigned long)(es / 3600), (unsigned long)((es % 3600) / 60));
        break;
      }
      case 4:
        formatAvgPaceOrSpeed(buf, n, _task->getNodePrefs());
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

  // Per-point list, newest first. Each row: HH:MM (local) + delta from the
  // previous point in the same segment, or "start" for segment-start rows.
  void renderList(DisplayDriver& display) {
    const int top    = display.listStart();
    const int step   = display.lineStep();
    const int hint_h = step;
    const int avail  = display.height() - top - hint_h - 2;

    if (_store->empty()) {
      display.drawTextCentered(display.width() / 2, top + avail / 2, "No trail yet");
      return;
    }

    int visible = avail / step;
    if (visible < 1) visible = 1;
    const int total = _store->count();
    if (visible > total) visible = total;

    int max_scroll = total - visible;
    if (_list_scroll > max_scroll) _list_scroll = max_scroll;
    if (_list_scroll < 0)          _list_scroll = 0;

    NodePrefs* p = _task->getNodePrefs();
    int32_t tz_off = (int32_t)(p ? p->tz_offset_hours : 0) * 3600;

    for (int i = 0; i < visible; i++) {
      int idx = total - 1 - (_list_scroll + i);  // newest first
      if (idx < 0) break;
      const TrailPoint& pt = _store->at(idx);

      time_t lt = (time_t)((int32_t)pt.ts + tz_off);
      struct tm* ti = gmtime(&lt);

      char dist[12];
      if (idx == 0 || (pt.flags & TRAIL_FLAG_SEG_START)) {
        snprintf(dist, sizeof(dist), "start");
      } else {
        const TrailPoint& prev = _store->at(idx - 1);
        float d = TrailStore::haversineMeters(prev.lat_1e6, prev.lon_1e6,
                                                pt.lat_1e6,    pt.lon_1e6);
        if (d < 1000.0f) snprintf(dist, sizeof(dist), "+%d m", (int)d);
        else             snprintf(dist, sizeof(dist), "+%.1f km", d / 1000.0f);
      }

      char row[28];
      snprintf(row, sizeof(row), "%02d:%02d  %s",
                ti->tm_hour, ti->tm_min, dist);
      display.setCursor(2, top + i * step);
      display.print(row);
    }

    int cw = display.getCharWidth();
    if (_list_scroll > 0) {
      display.setCursor(display.width() - cw, top);
      display.print("^");
    }
    if (_list_scroll + visible < total) {
      display.setCursor(display.width() - cw, top + (visible - 1) * step);
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
    // its predecessor is not joined to it. At every break we mark the segment
    // end (filled dot) and the resume point (open dot) so the user can see
    // where tracking paused.
    int x0, y0;
    project(_store->at(0), x0, y0);
    display.fillRect(x0, y0, 1, 1);
    for (int i = 1; i < _store->count(); i++) {
      int x1, y1;
      project(_store->at(i), x1, y1);
      if (_store->at(i).flags & TRAIL_FLAG_SEG_START) {
        drawFilledDot(display, x0, y0);  // end of the previous segment
        drawOpenDot(display, x1, y1);    // resume point
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

    // North indicator in the top-right of the map area — the projection is
    // strictly north-up (y inverted from latitude), so the arrow is fixed.
    drawNorthArrow(display, area_x + area_w - 4, area_y + display.getLineHeight() + 1);
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

  // 3×3 filled square — end of a segment before a break.
  static void drawFilledDot(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 1, cy - 1, 3, 3);
  }

  // 3×3 outline square — start of a segment after a break (resume point).
  static void drawOpenDot(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 1, cy - 1, 3, 1);  // top
    d.fillRect(cx - 1, cy + 1, 3, 1);  // bottom
    d.fillRect(cx - 1, cy,     1, 1);  // left
    d.fillRect(cx + 1, cy,     1, 1);  // right
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

  // North arrow + "N" label. Tip at (cx, cy), arrow body extends down for 6 px;
  // the "N" sits one line height above the tip.
  static void drawNorthArrow(DisplayDriver& d, int cx, int cy) {
    int lh = d.getLineHeight();
    int cw = d.getCharWidth();
    d.setCursor(cx - cw / 2, cy - lh);
    d.print("N");
    // arrowhead
    d.fillRect(cx,     cy,     1, 1);
    d.fillRect(cx - 1, cy + 1, 3, 1);
    d.fillRect(cx - 2, cy + 2, 5, 1);
    // shaft
    d.fillRect(cx,     cy + 3, 1, 4);
  }
};
