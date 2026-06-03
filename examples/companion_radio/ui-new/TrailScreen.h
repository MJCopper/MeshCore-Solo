#pragma once
// GPS trail viewer. Tools › Trail.
// Three live views — Summary, Map, List — cyclable with LEFT/RIGHT.
// All settings and actions live in the Hold-Enter popup so a short Enter never
// accidentally stops tracking.
// Included by UITask.cpp after Trail store + ToolsScreen.

#include "../Trail.h"
#include "../GeoUtils.h"
#include "NavView.h"
#include <math.h>

class TrailScreen : public UIScreen {
  UITask*     _task;
  TrailStore* _store;

  enum View { V_SUMMARY = 0, V_MAP = 1, V_LIST = 2, V_COUNT };
  uint8_t _view           = V_SUMMARY;
  int     _summary_scroll = 0;
  int     _list_scroll    = 0;
  bool    _cfg_dirty      = false;
  bool    _map_grid       = true;   // show scale grid in map view (Enter to toggle)

  // Waypoint sub-screens layered over the trail views. WP_OFF = normal trail.
  enum WpMode { WP_OFF, WP_LIST, WP_NAV };
  uint8_t   _wp_mode  = WP_OFF;
  int       _wp_sel   = 0;
  int       _wp_scroll = 0;
  PopupMenu _wp_ctx;                 // Rename / Delete on a selected waypoint
  KeyboardWidget _wp_kb;             // label entry (mark new / rename)
  bool      _kb_active = false;
  int       _kb_rename_idx = -1;     // -1 = marking new, ≥0 = renaming that index
  int32_t   _mark_lat = 0, _mark_lon = 0;   // pending new-mark position
  uint32_t  _mark_ts  = 0;

  // Action popup (Hold Enter / KEY_CONTEXT_MENU). Carries both settings rows
  // (Min dist, Units — cycled with LEFT/RIGHT while focused) and actions
  // (Start/Stop tracking, Reset). PopupMenu stores label pointers verbatim, so
  // the labels live in member buffers that get refreshed in openActionMenu()
  // and after every LEFT/RIGHT cycle.
  enum ActionId { ACT_MIN_DIST, ACT_UNITS, ACT_GRID, ACT_TOGGLE, ACT_SAVE, ACT_LOAD, ACT_RESET, ACT_EXPORT, ACT_EXPORT_SAVED, ACT_MARK, ACT_WAYPOINTS, ACT_WP_CLEAR };
  PopupMenu _action_menu;
  uint8_t   _act_map[14];  // 12 used today; pad so adding an action doesn't OOB
  uint8_t   _act_count = 0;
  char      _act_min_dist_label[24];
  char      _act_units_label[24];
  char      _act_grid_label[16];
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
    _wp_mode = WP_OFF;
    _wp_ctx.active = false;
    _kb_active = false;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // Label keyboard (mark / rename) owns the whole screen while open.
    if (_kb_active) return _wp_kb.render(display);

    // Waypoint sub-screens replace the trail views.
    if (_wp_mode == WP_NAV)  { renderWpNav(display);  return 1000; }
    if (_wp_mode == WP_LIST) {
      renderWpList(display);
      if (_wp_ctx.active) _wp_ctx.render(display);
      return 1000;
    }

    // Title carries the view counter so the bottom hint row can be reclaimed
    // for content.
    const char* base = (_view == V_MAP)  ? (_map_grid ? "TRAIL MAP+" : "TRAIL MAP")
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
    // Label keyboard (mark new / rename) takes all input first.
    if (_kb_active) {
      auto r = _wp_kb.handleInput(c);
      if (r == KeyboardWidget::DONE)        { commitLabel(_wp_kb.buf); _kb_active = false; }
      else if (r == KeyboardWidget::CANCELLED) { _kb_active = false; }
      return true;
    }

    // Waypoint Rename/Delete popup. Operates on the saved-waypoint index
    // (the synthetic Trail-start row, if any, is never editable).
    if (_wp_ctx.active) {
      auto res = _wp_ctx.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int sel = _wp_ctx.selectedIndex();
        int wi  = wpIndex();
        if (sel == 0) {                                  // Rename
          if (wi >= 0 && wi < _task->waypoints().count()) {
            _kb_rename_idx = wi;
            _wp_kb.begin(_task->waypoints().at(wi).label, WAYPOINT_LABEL_LEN - 1);
            _kb_active = true;
          }
        } else {                                          // Delete
          if (wi >= 0 && wi < _task->waypoints().count()) {
            _task->waypoints().remove(wi);
            _task->saveWaypoints();
            if (_wp_sel >= wpListCount()) _wp_sel = wpListCount() - 1;
            if (_wp_sel < 0) _wp_sel = 0;
          }
        }
      }
      return true;
    }

    // Waypoint navigation view — any nav key returns to the list.
    if (_wp_mode == WP_NAV) {
      if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_PREV ||
          c == KEY_RIGHT  || c == KEY_NEXT) { _wp_mode = WP_LIST; }
      return true;
    }

    // Waypoint list (row 0 is the synthetic "Trail start" when a trail exists).
    if (_wp_mode == WP_LIST) {
      int n = wpListCount();
      if (c == KEY_CANCEL) { _wp_mode = WP_OFF; return true; }
      if (c == KEY_UP   && _wp_sel > 0)     { _wp_sel--; if (_wp_sel < _wp_scroll) _wp_scroll = _wp_sel; return true; }
      if (c == KEY_DOWN && _wp_sel < n - 1) { _wp_sel++; return true; }
      if (c == KEY_ENTER && n > 0)          { _wp_mode = WP_NAV; return true; }
      // Rename/Delete apply to saved waypoints only — not the Trail-start row.
      if (c == KEY_CONTEXT_MENU && !selIsStart() && wpIndex() < _task->waypoints().count()) {
        _wp_ctx.begin("Waypoint", 2);
        _wp_ctx.addItem("Rename");
        _wp_ctx.addItem("Delete");
        return true;
      }
      return true;
    }

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
          if (_act_map[idx] == ACT_GRID)          { _map_grid = !_map_grid;                    refreshActionLabels(); return true; }
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
          else if (act == ACT_GRID)         { _map_grid = !_map_grid; }
          else if (act == ACT_EXPORT)       { handleExport(); }
          else if (act == ACT_EXPORT_SAVED) { handleExportSaved(); }
          else if (act == ACT_MARK)         { handleMarkHere(); }
          else if (act == ACT_WAYPOINTS)    { _wp_mode = WP_LIST; _wp_sel = 0; _wp_scroll = 0; }
          else if (act == ACT_WP_CLEAR)     { _task->waypoints().clear(); _task->saveWaypoints();
                                              _task->showAlert("Waypoints cleared", 800); }
          else /* MIN_DIST / UNITS */       { reopenAt(sel); return true; }  // re-open keeping focus
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
    size_t n = _store->exportGpx(Serial);
    showExportAlert(n);
  }

  void handleExportSaved() {
    if (!Serial) { _task->showAlert("Connect USB first", 1200); return; }
    DataStore* ds = the_mesh.getDataStore();
    if (!ds) { _task->showAlert("FS unavailable", 800); return; }
    File f = ds->openRead(TRAIL_FILE);
    if (!f) { _task->showAlert("No saved trail", 800); return; }
    size_t n = TrailStore::exportGpxFromFile(f, Serial);
    f.close();
    if (n == 0) { _task->showAlert("Bad saved file", 1000); return; }
    showExportAlert(n);
  }

  void showExportAlert(size_t n) {
    // The companion app multiplexes BLE + USB on the same frame protocol.
    // When BLE is connected, USB-receive is ignored and the dump can't
    // collide. Without a BLE link the app might be on USB itself, so warn
    // the user — they may need to reconnect their app afterwards.
    char alert[28];
    if (_task->hasConnection()) snprintf(alert, sizeof(alert), "GPX %u B (USB)",       (unsigned)n);
    else                         snprintf(alert, sizeof(alert), "GPX %u B - disc. app", (unsigned)n);
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
    snprintf(_act_grid_label,   sizeof(_act_grid_label),
              "Grid: %s",      _map_grid ? "ON" : "OFF");
    snprintf(_act_toggle_label, sizeof(_act_toggle_label),
              "%s tracking",  _store->isActive() ? "Stop" : "Start");
  }

  void openActionMenu() {
    refreshActionLabels();
    _act_count = 0;
    _action_menu.begin("Trail", 4);
    _act_map[_act_count++] = ACT_MIN_DIST; _action_menu.addItem(_act_min_dist_label);
    _act_map[_act_count++] = ACT_UNITS;    _action_menu.addItem(_act_units_label);
    _act_map[_act_count++] = ACT_GRID;     _action_menu.addItem(_act_grid_label);
    _act_map[_act_count++] = ACT_TOGGLE;   _action_menu.addItem(_act_toggle_label);
    // Waypoints: mark the current spot, and browse/navigate saved ones.
    _act_map[_act_count++] = ACT_MARK;     _action_menu.addItem("Mark here");
    // "Waypoints" opens the nav list — useful when there are saved waypoints
    // OR a trail to backtrack to (the list always includes a "Trail start" row).
    if (_task->waypoints().count() > 0 || !_store->empty()) {
      _act_map[_act_count++] = ACT_WAYPOINTS; _action_menu.addItem("Waypoints");
    }
    if (_task->waypoints().count() > 0) {
      _act_map[_act_count++] = ACT_WP_CLEAR;  _action_menu.addItem("Clear waypoints");
    }
    if (!_store->empty()) {
      _act_map[_act_count++] = ACT_SAVE;   _action_menu.addItem("Save trail");
    }
    bool saved = savedTrailExists();
    if (saved) {
      _act_map[_act_count++] = ACT_LOAD;          _action_menu.addItem("Load trail");
    }
    if (!_store->empty()) {
      _act_map[_act_count++] = ACT_EXPORT;        _action_menu.addItem("Export (live)");
    }
    if (saved) {
      _act_map[_act_count++] = ACT_EXPORT_SAVED;  _action_menu.addItem("Export (saved)");
    }
    if (!_store->empty()) {
      _act_map[_act_count++] = ACT_RESET;         _action_menu.addItem("Reset trail");
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

  // ── Waypoints ──────────────────────────────────────────────────────────────
  // The nav list shows a synthetic "Trail start" row (index 0) whenever a trail
  // exists, followed by the saved waypoints. These helpers map the combined
  // row index (_wp_sel) onto that layout.
  bool hasStart() const { return !_store->empty(); }
  int  wpListCount() const { return (hasStart() ? 1 : 0) + _task->waypoints().count(); }
  bool selIsStart() const { return hasStart() && _wp_sel == 0; }
  int  wpIndex() const { return _wp_sel - (hasStart() ? 1 : 0); }  // index into WaypointStore

  static bool ownPos(int32_t& lat, int32_t& lon) {
#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc && loc->isValid()) {
      lat = (int32_t)loc->getLatitude();
      lon = (int32_t)loc->getLongitude();
      return true;
    }
#endif
    return false;
  }

  void handleMarkHere() {
    int32_t lat, lon;
    if (!ownPos(lat, lon))            { _task->showAlert("No GPS fix", 1000); return; }
    if (_task->waypoints().full())    { _task->showAlert("Waypoints full", 1000); return; }
    _mark_lat = lat; _mark_lon = lon; _mark_ts = (uint32_t)rtc_clock.getCurrentTime();
    _kb_rename_idx = -1;
    _wp_kb.begin("", WAYPOINT_LABEL_LEN - 1);
    _kb_active = true;
  }

  void commitLabel(const char* buf) {
    if (_kb_rename_idx >= 0) {
      _task->waypoints().rename(_kb_rename_idx, buf);
      _task->saveWaypoints();
      _kb_rename_idx = -1;
      return;
    }
    char auto_lbl[WAYPOINT_LABEL_LEN];
    if (!buf || !buf[0]) {
      snprintf(auto_lbl, sizeof(auto_lbl), "WP%d", _task->waypoints().count() + 1);
      buf = auto_lbl;
    }
    if (_task->waypoints().add(_mark_lat, _mark_lon, _mark_ts, buf)) {
      _task->saveWaypoints();
      _task->showAlert("Waypoint saved", 800);
    } else {
      _task->showAlert("Waypoints full", 1000);
    }
  }

  void renderWpList(DisplayDriver& display) {
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0, "WAYPOINTS");
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    int n = wpListCount();
    if (n == 0) {
      display.drawTextCentered(display.width() / 2, display.height() / 2, "No waypoints");
      return;
    }
    const int top  = display.listStart();
    const int step = display.lineStep();
    const int cw   = display.getCharWidth();
    int vis = display.listVisible();
    if (vis < 1) vis = 1;
    if (_wp_sel < _wp_scroll)            _wp_scroll = _wp_sel;
    if (_wp_sel >= _wp_scroll + vis)     _wp_scroll = _wp_sel - vis + 1;

    int32_t mylat, mylon; bool have = ownPos(mylat, mylon);

    for (int i = 0; i < vis && (_wp_scroll + i) < n; i++) {
      int row = _wp_scroll + i;
      int32_t tlat, tlon; const char* label;
      if (!rowTarget(row, tlat, tlon, label)) continue;
      int y = top + i * step;
      bool sel = (row == _wp_sel);
      display.drawSelectionRow(0, y - 1, display.width(), step - 1, sel);
      display.setCursor(0, y); display.print(sel ? ">" : " ");

      char dist[12] = ""; int bw = 0;
      if (have) {
        geo::fmtDist(dist, sizeof(dist), geo::haversineKm(mylat, mylon, tlat, tlon));
        bw = display.getTextWidth(dist) + 2;
      }
      char nm[24];
      display.translateUTF8ToBlocks(nm, label, sizeof(nm));
      display.drawTextEllipsized(cw + 2, y, display.width() - cw - 2 - bw, nm);
      if (dist[0]) { display.setCursor(display.width() - bw + 1, y); display.print(dist); }
      display.setColor(DisplayDriver::LIGHT);
    }
  }

  void renderWpNav(DisplayDriver& display) {
    int32_t tlat, tlon; const char* label;
    if (!rowTarget(_wp_sel, tlat, tlon, label)) { _wp_mode = WP_LIST; return; }
    int32_t mylat, mylon; bool have = ownPos(mylat, mylon);
    int cog; bool cogv = _task->currentCourse(cog);
    navview::draw(display, have, mylat, mylon, tlat, tlon, label, cogv, cog);
  }

  // Resolve a combined-list row to a nav target. Row 0 is the trail start when
  // a trail exists; the rest are saved waypoints.
  bool rowTarget(int row, int32_t& lat, int32_t& lon, const char*& label) {
    if (hasStart() && row == 0) {
      const TrailPoint& s = _store->first();
      lat = s.lat_1e6; lon = s.lon_1e6; label = "Trail start";
      return true;
    }
    int wi = row - (hasStart() ? 1 : 0);
    WaypointStore& wp = _task->waypoints();
    if (wi >= 0 && wi < wp.count()) {
      const Waypoint& w = wp.at(wi);
      lat = w.lat_1e6; lon = w.lon_1e6;
      label = w.label[0] ? w.label : "(unnamed)";
      return true;
    }
    return false;
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

    WaypointStore& wp = _task->waypoints();
    const int nwp     = wp.count();
    const bool have_trail = !_store->empty();

    if (!have_trail && nwp == 0) {
      display.drawTextCentered(display.width() / 2, (top + bottom) / 2, "No trail yet");
      return;
    }

    const int area_x = 2;
    const int area_y = top + 1;
    const int area_w = display.width() - 4;
    const int area_h = bottom - top - 2;
    if (area_w < 6 || area_h < 6) return;

    // Bounding box spans the trail AND every waypoint, so off-track waypoints
    // stay in frame.
    int32_t min_lat = 0, min_lon = 0, max_lat = 0, max_lon = 0;
    bool init = false;
    if (have_trail) { _store->boundingBox(min_lat, min_lon, max_lat, max_lon); init = true; }
    for (int i = 0; i < nwp; i++) {
      const Waypoint& w = wp.at(i);
      if (!init) { min_lat = max_lat = w.lat_1e6; min_lon = max_lon = w.lon_1e6; init = true; }
      else {
        if (w.lat_1e6 < min_lat) min_lat = w.lat_1e6;
        if (w.lat_1e6 > max_lat) max_lat = w.lat_1e6;
        if (w.lon_1e6 < min_lon) min_lon = w.lon_1e6;
        if (w.lon_1e6 > max_lon) max_lon = w.lon_1e6;
      }
    }

    // Degenerate: everything at one coordinate — just centre the markers.
    if (min_lat == max_lat && min_lon == max_lon) {
      int ccx = area_x + area_w / 2, ccy = area_y + area_h / 2;
      if (have_trail) drawCurrentMarker(display, ccx, ccy);
      if (nwp > 0)    drawWaypointMarker(display, ccx, ccy);
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

    auto projectLL = [&](int32_t lat, int32_t lon, int& px, int& py) {
      px = off_x + (int)((float)(lon - min_lon) * lon_scale_geo * scale);
      py = off_y + (int)((float)(max_lat - lat) * scale);
    };
    auto project = [&](const TrailPoint& p, int& px, int& py) {
      projectLL(p.lat_1e6, p.lon_1e6, px, py);
    };

    if (_map_grid)
      renderGrid(display, area_x, area_y, area_w, area_h,
                 min_lat, max_lat, min_lon, lon_scale_geo, scale, off_x, off_y);

    if (have_trail) {
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
    }

    // Waypoints on top, with the label's first character beside the marker.
    for (int i = 0; i < nwp; i++) {
      const Waypoint& w = wp.at(i);
      int wx, wy; projectLL(w.lat_1e6, w.lon_1e6, wx, wy);
      drawWaypointMarker(display, wx, wy);
      if (w.label[0] && wx + 3 + display.getCharWidth() <= area_x + area_w) {
        char s[2] = { w.label[0], 0 };
        display.setCursor(wx + 3, wy - 3);
        display.print(s);
      }
    }

    drawNorthArrow(display, area_x + area_w - 4, area_y + display.getLineHeight() + 1);
  }

  void renderGrid(DisplayDriver& display,
                  int area_x, int area_y, int area_w, int area_h,
                  int32_t min_lat, int32_t max_lat, int32_t min_lon,
                  float lon_scale_geo, float scale, int off_x, int off_y) {
    static constexpr float M_PER_1E6 = 0.11132f;  // metres per 1e-6 deg lat
    float ppm = scale / M_PER_1E6;
    if (ppm <= 0.0f) return;

    float shorter_m = (float)(area_w < area_h ? area_w : area_h) / ppm;
    float target_m  = shorter_m / 3.0f;

    static const uint32_t STEPS[] = {
      1, 2, 5, 10, 20, 50, 100, 200, 500,
      1000, 2000, 5000, 10000, 20000, 50000, 100000
    };
    static const int N_STEPS = (int)(sizeof(STEPS)/sizeof(STEPS[0]));

    // Pick largest step ≤ target_m (gives ~3 intervals across shorter dim).
    int sel = 0;
    for (int si = N_STEPS - 1; si >= 0; si--) {
      if ((float)STEPS[si] <= target_m) { sel = si; break; }
    }
    // Bump up until pixel spacing between intersections is at least MIN_GRID_PX.
    // This keeps OLED grids sparse even when target_m selects a fine step.
    static const float MIN_GRID_PX = 22.0f;
    while (sel < N_STEPS - 1 && (float)STEPS[sel] / M_PER_1E6 * scale < MIN_GRID_PX)
      sel++;
    // Clamp back down: ensure at least 2 intervals (3 visible lines) across
    // the shorter dimension. MIN_GRID_PX must not leave only 1 interval.
    float shorter_px = (float)(area_w < area_h ? area_w : area_h);
    while (sel > 0 && (float)STEPS[sel] / M_PER_1E6 * scale > shorter_px / 2.0f)
      sel--;

    float vis_lat_max = (float)max_lat + (float)(off_y - area_y) / scale;
    float vis_lat_min = (float)max_lat - (float)(area_y + area_h - off_y) / scale;
    float vis_lon_min = (float)min_lon - (float)(off_x - area_x) / (lon_scale_geo * scale);
    float vis_lon_max = (float)min_lon + (float)(area_x + area_w - off_x) / (lon_scale_geo * scale);

    // Final guard: a very elongated trail (lat_span ≪ lon_span or vice-versa)
    // shares a single isotropic scale, so the visible window in the *short*
    // direction can span many grid_m. Bump the step up until the resulting
    // line count fits the static buffers (40×40 = ~1600 intersections, plenty
    // for any sane display).
    static const int MAX_GRID_LINES = 40;
    uint32_t grid_m = STEPS[sel];
    float gs_lat = (float)grid_m / M_PER_1E6;
    float gs_lon = (float)grid_m / (M_PER_1E6 * lon_scale_geo);
    int lat_n = (int)((vis_lat_max - vis_lat_min) / gs_lat) + 2;
    int lon_n = (int)((vis_lon_max - vis_lon_min) / gs_lon) + 2;
    while ((lat_n > MAX_GRID_LINES || lon_n > MAX_GRID_LINES) && sel < N_STEPS - 1) {
      sel++;
      grid_m = STEPS[sel];
      gs_lat = (float)grid_m / M_PER_1E6;
      gs_lon = (float)grid_m / (M_PER_1E6 * lon_scale_geo);
      lat_n  = (int)((vis_lat_max - vis_lat_min) / gs_lat) + 2;
      lon_n  = (int)((vis_lon_max - vis_lon_min) / gs_lon) + 2;
    }
    if (lat_n > MAX_GRID_LINES || lon_n > MAX_GRID_LINES) return;  // huge step still won't fit — give up

    // Anchor to display edges so lines always start at the border and space inward.
    float first_lat = vis_lat_min;
    float first_lon = vis_lon_min;

    int x_max = area_x + area_w - 1;
    int y_max = area_y + area_h - 1;

    auto fillSafe = [&](int x, int y) {
      if (x >= area_x && x <= x_max && y >= area_y && y <= y_max)
        display.fillRect(x, y, 1, 1);
    };

    char lbl[12];
    if (grid_m < 1000) snprintf(lbl, sizeof(lbl), "%um",  (unsigned)grid_m);
    else               snprintf(lbl, sizeof(lbl), "%ukm", (unsigned)(grid_m / 1000));
    int lh  = display.getLineHeight();
    int cw  = display.getCharWidth();

    // Scale label bbox (bottom-left corner)
    int lbl_x1 = area_x;
    int lbl_x2 = area_x + (int)strlen(lbl) * cw + 1;
    int lbl_y1 = area_y + area_h - lh - 1;
    int lbl_y2 = area_y + area_h;

    // North-arrow bbox — mirrors drawNorthArrow(cx = area_x+area_w-4, cy = area_y+lh+1)
    int arr_cx  = area_x + area_w - 4;
    int arr_cy  = area_y + lh + 1;
    int arr_x1  = arr_cx - cw - 1;
    int arr_x2  = x_max;
    int arr_y1  = area_y;
    int arr_y2  = arr_cy + 7;

    for (int i = 0; i < lat_n; i++) {
      float lat_val = first_lat + (float)i * gs_lat;
      int py = off_y + (int)(((float)max_lat - lat_val) * scale);
      if (py < area_y - 1 || py > y_max + 1) continue;
      for (int j = 0; j < lon_n; j++) {
        float lon_val = first_lon + (float)j * gs_lon;
        int px = off_x + (int)((lon_val - (float)min_lon) * lon_scale_geo * scale);
        if (px < area_x - 1 || px > x_max + 1) continue;
        if (px >= lbl_x1 && px <= lbl_x2 && py >= lbl_y1 && py <= lbl_y2) continue;
        if (px >= arr_x1 && px <= arr_x2 && py >= arr_y1 && py <= arr_y2) continue;
        fillSafe(px,     py);
        fillSafe(px - 1, py);
        fillSafe(px + 1, py);
        fillSafe(px,     py - 1);
        fillSafe(px,     py + 1);
      }
    }

    display.setCursor(area_x, area_y + area_h - lh);
    display.print(lbl);
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
  // Hollow diamond — distinct from the trail's dots (○●), start (+) and
  // current (✕) markers.
  static void drawWaypointMarker(DisplayDriver& d, int cx, int cy) {
    d.fillRect(cx,     cy - 2, 1, 1);
    d.fillRect(cx - 1, cy - 1, 1, 1); d.fillRect(cx + 1, cy - 1, 1, 1);
    d.fillRect(cx - 2, cy,     1, 1); d.fillRect(cx + 2, cy,     1, 1);
    d.fillRect(cx - 1, cy + 1, 1, 1); d.fillRect(cx + 1, cy + 1, 1, 1);
    d.fillRect(cx,     cy + 2, 1, 1);
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
