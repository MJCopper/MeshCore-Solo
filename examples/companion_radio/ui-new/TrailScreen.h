#pragma once
// GPS trail viewer. Tools › Trail.
// Three live views — Summary, Map, List — cyclable with LEFT/RIGHT.
// All settings and actions live in the Hold-Enter popup so a short Enter never
// accidentally stops tracking.
// Included by UITask.cpp after Trail store + ToolsScreen.

#include "../Trail.h"
#include <math.h>

class TrailScreen : public UIScreen {
  UITask*     _task;
  TrailStore* _store;

  enum View { V_SUMMARY = 0, V_MAP = 1, V_LIST = 2, V_COUNT };
  uint8_t _view           = V_SUMMARY;
  int     _summary_scroll = 0;
  int     _list_scroll    = 0;
  bool    _cfg_dirty      = false;

  // Action popup (Hold Enter / KEY_CONTEXT_MENU). Carries both settings rows
  // (Min dist, Units — cycled with LEFT/RIGHT while focused) and actions
  // (Start/Stop tracking, Reset). PopupMenu stores label pointers verbatim, so
  // the labels live in member buffers that get refreshed in openActionMenu()
  // and after every LEFT/RIGHT cycle.
  enum ActionId { ACT_MIN_DIST, ACT_UNITS, ACT_TOGGLE, ACT_SAVE, ACT_LOAD, ACT_RESET, ACT_EXPORT };
  PopupMenu _action_menu;
  uint8_t   _act_map[8];
  uint8_t   _act_count = 0;
  char      _act_min_dist_label[24];
  char      _act_units_label[24];
  char      _act_toggle_label[20];

  static const int SUMMARY_ITEM_COUNT = 5;

public:
  TrailScreen(UITask* task, TrailStore* store) : _task(task), _store(store) {}

  void enter() {
    _view = V_SUMMARY;
    _summary_scroll = 0;
    _list_scroll    = 0;
    _cfg_dirty      = false;
    _action_menu.active = false;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // Title carries the view counter so the bottom hint row can be reclaimed
    // for content.
    const char* base = (_view == V_MAP)  ? "TRAIL MAP"
                     : (_view == V_LIST) ? "TRAIL LIST"
                     :                      "TRAIL";
    char title[20];
    snprintf(title, sizeof(title), "%s %d/%d", base, (int)_view + 1, (int)V_COUNT);
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    if      (_view == V_MAP)  renderMap(display);
    else if (_view == V_LIST) renderList(display);
    else                       renderSummary(display);

    if (_action_menu.active) _action_menu.render(display);
    return _store->isActive() ? 1000 : 5000;
  }

  bool handleInput(char c) override {
    // Action popup overrides everything else.
    if (_action_menu.active) {
      // LEFT/RIGHT on settings rows cycles the value in-place and refreshes
      // the popup label — the popup stays open so the user can keep tapping.
      if (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_PREV || c == KEY_NEXT) {
        int idx = _action_menu.selectedIndex();
        if (idx >= 0 && idx < _act_count) {
          NodePrefs* p = _task->getNodePrefs();
          int dir = (c == KEY_RIGHT || c == KEY_NEXT) ? 1 : -1;
          if (_act_map[idx] == ACT_MIN_DIST && p) { cycleMinDelta(p, dir); _cfg_dirty = true; refreshActionLabels(); return true; }
          if (_act_map[idx] == ACT_UNITS    && p) { cycleUnits(p, dir);    _cfg_dirty = true; refreshActionLabels(); return true; }
        }
        return true;  // swallow on action rows
      }
      auto res = _action_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int sel = _action_menu.selectedIndex();
        if (sel >= 0 && sel < _act_count) {
          ActionId act = (ActionId)_act_map[sel];
          if (act == ACT_TOGGLE)        { handleToggle(); }
          else if (act == ACT_SAVE)     { handleSave(); }
          else if (act == ACT_LOAD)     { handleLoad(); }
          else if (act == ACT_RESET)    { handleReset(); }
          else if (act == ACT_EXPORT)   { handleExport(); }
          else /* MIN_DIST / UNITS */   { reopenAt(sel); return true; }  // re-open keeping focus
        }
        if (_cfg_dirty) { the_mesh.savePrefs(); _cfg_dirty = false; }
      } else if (res == PopupMenu::CANCELLED) {
        if (_cfg_dirty) { the_mesh.savePrefs(); _cfg_dirty = false; }
      }
      return true;
    }

    if (c == KEY_CANCEL) {
      if (_cfg_dirty) { the_mesh.savePrefs(); _cfg_dirty = false; }
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) { openActionMenu(); return true; }

    if (c == KEY_LEFT  || c == KEY_PREV) { _view = (uint8_t)((_view + V_COUNT - 1) % V_COUNT); return true; }
    if (c == KEY_RIGHT || c == KEY_NEXT) { _view = (uint8_t)((_view + 1) % V_COUNT);          return true; }
    if (_view == V_SUMMARY && c == KEY_UP   && _summary_scroll > 0) { _summary_scroll--; return true; }
    if (_view == V_SUMMARY && c == KEY_DOWN)                        { _summary_scroll++; return true; }
    if (_view == V_LIST    && c == KEY_UP   && _list_scroll > 0)    { _list_scroll--;    return true; }
    if (_view == V_LIST    && c == KEY_DOWN)                        { _list_scroll++;    return true; }
    if (c == KEY_ENTER) {
      // Short Enter intentionally does nothing destructive — both start and
      // stop go through the Hold-Enter popup so a stray tap can't change the
      // tracking state.
      _task->showAlert("Hold Enter for menu", 1000);
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
  void handleToggle() {
    bool was = _store->isActive();
    _store->setActive(!was);
    _task->showAlert(was ? "Tracking stopped"
                         : (gpsHasFix() ? "Tracking started" : "Waiting for GPS fix"),
                      was ? 800 : 1000);
  }
  void handleReset() {
    if (_store->isActive()) _store->setActive(false);
    _store->clear();
    _task->showAlert("Trail reset", 800);
  }
  void handleSave() {
    DataStore* ds = the_mesh.getDataStore();
    if (!ds) { _task->showAlert("FS unavailable", 800); return; }
    File f = ds->openWrite(TRAIL_FILE);
    if (!f) { _task->showAlert("Save failed", 800); return; }
    bool ok = _store->writeTo(f);
    f.close();
    _task->showAlert(ok ? "Trail saved" : "Save failed", 800);
  }
  void handleLoad() {
    DataStore* ds = the_mesh.getDataStore();
    if (!ds) { _task->showAlert("FS unavailable", 800); return; }
    File f = ds->openRead(TRAIL_FILE);
    if (!f) { _task->showAlert("No saved trail", 800); return; }
    bool ok = _store->readFrom(f);
    f.close();
    _task->showAlert(ok ? "Trail loaded" : "Load failed", 800);
  }
  void handleExport() {
    if (!Serial) { _task->showAlert("Connect USB first", 1200); return; }
    // The companion app multiplexes BLE + USB on the same frame protocol.
    // When BLE is connected, USB-receive is ignored and the dump can't
    // collide. Without a BLE link the app might be on USB itself, in
    // which case the raw GPX would land mid-frame and confuse it — warn
    // the user but proceed (they may have no app attached at all).
    size_t n = _store->exportGpx(Serial);
    char alert[28];
    if (_task->hasConnection()) {
      snprintf(alert, sizeof(alert), "GPX %u B (USB)", (unsigned)n);
    } else {
      snprintf(alert, sizeof(alert), "GPX %u B - disc. app", (unsigned)n);
    }
    _task->showAlert(alert, 1500);
  }

  static constexpr const char* TRAIL_FILE = "/trail";

  // Refresh the three settings/action labels in-place. Pointer identity is
  // preserved, so PopupMenu picks up the new text on the next render.
  void refreshActionLabels() {
    NodePrefs* p = _task->getNodePrefs();
    snprintf(_act_min_dist_label, sizeof(_act_min_dist_label),
              "Min dist: %s", TrailStore::minDeltaLabel(p ? p->trail_min_delta_idx : 0));
    snprintf(_act_units_label, sizeof(_act_units_label),
              "Units: %s",    TrailStore::unitLabel(p    ? p->trail_units_idx     : 0));
    snprintf(_act_toggle_label, sizeof(_act_toggle_label),
              "%s tracking",  _store->isActive() ? "Stop" : "Start");
  }

  void openActionMenu() {
    refreshActionLabels();
    _act_count = 0;
    _action_menu.begin("Trail", 4);
    _act_map[_act_count++] = ACT_MIN_DIST; _action_menu.addItem(_act_min_dist_label);
    _act_map[_act_count++] = ACT_UNITS;    _action_menu.addItem(_act_units_label);
    _act_map[_act_count++] = ACT_TOGGLE;   _action_menu.addItem(_act_toggle_label);
    if (!_store->empty()) {
      _act_map[_act_count++] = ACT_SAVE;   _action_menu.addItem("Save trail");
    }
    if (savedTrailExists()) {
      _act_map[_act_count++] = ACT_LOAD;   _action_menu.addItem("Load trail");
    }
    if (!_store->empty()) {
      _act_map[_act_count++] = ACT_EXPORT; _action_menu.addItem("Export GPX");
      _act_map[_act_count++] = ACT_RESET;  _action_menu.addItem("Reset trail");
    }
  }

  static bool savedTrailExists() {
    DataStore* ds = the_mesh.getDataStore();
    if (!ds) return false;
    File f = ds->openRead(TRAIL_FILE);
    bool ok = (bool)f;
    if (f) f.close();
    return ok;
  }

  // After Enter on a settings row, the popup auto-closes per PopupMenu's
  // semantics. Re-open it with focus restored to that row so the user can
  // continue cycling.
  void reopenAt(int sel) {
    openActionMenu();
    if (sel >= 0 && sel < _act_count) _action_menu._sel = sel;
  }

  void cycleMinDelta(NodePrefs* p, int dir) {
    uint8_t idx = p->trail_min_delta_idx;
    if (idx >= TrailStore::MIN_DELTA_COUNT) idx = 0;
    idx = (dir > 0) ? (idx + 1) % TrailStore::MIN_DELTA_COUNT
                    : (idx + TrailStore::MIN_DELTA_COUNT - 1) % TrailStore::MIN_DELTA_COUNT;
    p->trail_min_delta_idx = idx;
  }
  void cycleUnits(NodePrefs* p, int dir) {
    uint8_t idx = p->trail_units_idx;
    if (idx >= TrailStore::UNITS_COUNT) idx = 0;
    idx = (dir > 0) ? (idx + 1) % TrailStore::UNITS_COUNT
                    : (idx + TrailStore::UNITS_COUNT - 1) % TrailStore::UNITS_COUNT;
    p->trail_units_idx = idx;
  }

  // Render the average speed (or pace) in the user-selected units.
  void formatAvgPaceOrSpeed(char* buf, size_t n, NodePrefs* p) const {
    uint8_t u = p ? p->trail_units_idx : 0;
    if (u >= TrailStore::UNITS_COUNT) u = 0;
    uint16_t kmh = _store->avgSpeedKmh();
    if (u == TrailStore::UNITS_KMH) {
      snprintf(buf, n, "Avg: %u km/h", (unsigned)kmh);
    } else if (u == TrailStore::UNITS_MPH) {
      unsigned mph = (unsigned)((float)kmh * 0.621371f + 0.5f);
      snprintf(buf, n, "Avg: %u mph", mph);
    } else {
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
    const int avail  = display.height() - y0 - 2;
    int visible      = avail / step;
    if (visible < 1) visible = 1;
    if (visible > SUMMARY_ITEM_COUNT) visible = SUMMARY_ITEM_COUNT;

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

  void renderList(DisplayDriver& display) {
    const int top    = display.listStart();
    const int step   = display.lineStep();
    const int avail  = display.height() - top - 2;

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
      int idx = total - 1 - (_list_scroll + i);
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

  void renderMap(DisplayDriver& display) {
    const int top    = display.listStart();
    const int bottom = display.height() - 2;

    if (_store->empty()) {
      display.drawTextCentered(display.width() / 2, (top + bottom) / 2, "No trail yet");
      return;
    }

    const int area_x = 2;
    const int area_y = top + 1;
    const int area_w = display.width() - 4;
    const int area_h = bottom - top - 2;
    if (area_w < 6 || area_h < 6) return;

    int32_t min_lat, min_lon, max_lat, max_lon;
    _store->boundingBox(min_lat, min_lon, max_lat, max_lon);

    if (_store->count() == 1 || (min_lat == max_lat && min_lon == max_lon)) {
      drawCurrentMarker(display, area_x + area_w / 2, area_y + area_h / 2);
      return;
    }

    float avg_lat_rad = ((min_lat + max_lat) / 2.0e6f) * (float)M_PI / 180.0f;
    float lon_scale_geo = cosf(avg_lat_rad);
    if (lon_scale_geo < 0.05f) lon_scale_geo = 0.05f;

    float lat_span = (float)(max_lat - min_lat);
    float lon_span = (float)(max_lon - min_lon) * lon_scale_geo;

    float scale_lat = (float)area_h / (lat_span > 0 ? lat_span : 1.0f);
    float scale_lon = (float)area_w / (lon_span > 0 ? lon_span : 1.0f);
    float scale     = (scale_lat < scale_lon) ? scale_lat : scale_lon;

    int used_w = (int)(lon_span * scale);
    int used_h = (int)(lat_span * scale);
    int off_x  = area_x + (area_w - used_w) / 2;
    int off_y  = area_y + (area_h - used_h) / 2;

    auto project = [&](const TrailPoint& p, int& px, int& py) {
      float dx = (float)(p.lon_1e6 - min_lon) * lon_scale_geo * scale;
      float dy = (float)(max_lat - p.lat_1e6) * scale;
      px = off_x + (int)dx;
      py = off_y + (int)dy;
    };

    int x0, y0;
    project(_store->at(0), x0, y0);
    display.fillRect(x0, y0, 1, 1);
    for (int i = 1; i < _store->count(); i++) {
      int x1, y1;
      project(_store->at(i), x1, y1);
      if (_store->at(i).flags & TRAIL_FLAG_SEG_START) {
        drawFilledDot(display, x0, y0);
        drawOpenDot(display, x1, y1);
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

    drawNorthArrow(display, area_x + area_w - 4, area_y + display.getLineHeight() + 1);
  }

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

  static void drawFilledDot(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 1, cy - 1, 3, 3);
  }
  static void drawOpenDot(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 1, cy - 1, 3, 1);
    d.fillRect(cx - 1, cy + 1, 3, 1);
    d.fillRect(cx - 1, cy,     1, 1);
    d.fillRect(cx + 1, cy,     1, 1);
  }
  static void drawStartMarker(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx - 2, cy,     5, 1);
    d.fillRect(cx,     cy - 2, 1, 5);
  }
  static void drawCurrentMarker(DisplayDriver& d, int cx, int cy) {
    for (int i = -2; i <= 2; i++) {
      d.fillRect(cx + i, cy + i, 1, 1);
      d.fillRect(cx + i, cy - i, 1, 1);
    }
  }
  static void drawNorthArrow(DisplayDriver& d, int cx, int cy) {
    int lh = d.getLineHeight();
    int cw = d.getCharWidth();
    d.setCursor(cx - cw / 2, cy - lh);
    d.print("N");
    d.fillRect(cx,     cy,     1, 1);
    d.fillRect(cx - 1, cy + 1, 3, 1);
    d.fillRect(cx - 2, cy + 2, 5, 1);
    d.fillRect(cx,     cy + 3, 1, 4);
  }
};
