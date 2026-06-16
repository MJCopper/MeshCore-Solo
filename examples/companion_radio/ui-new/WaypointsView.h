#pragma once
// Waypoint management UI for Tools › Trail: list / navigate / add-by-coords,
// plus mark-here, rename, delete and send. Extracted from TrailScreen as a
// self-contained component that TrailScreen owns and delegates to while active.
// It needs the TrailStore only for the synthetic "Trail start" backtrack row.
//
// Included by TrailScreen.h (which UITask.cpp pulls in after UITask is fully
// defined, so the inline _task->… calls below see the complete type).

#include "../Waypoint.h"
#include "../GeoUtils.h"
#include "../Trail.h"
#include "KeyboardWidget.h"
#include "PopupMenu.h"
#include "NavView.h"

class WaypointsView {
  UITask*     _task;
  TrailStore* _store;

  // Sub-modes layered over the trail views. OFF = the component is dormant.
  enum Mode { OFF, LIST, NAV, ADD };
  uint8_t _mode   = OFF;
  int     _sel    = 0;
  int     _scroll = 0;

  PopupMenu      _ctx;               // Rename / Delete / Send on a selected waypoint
  bool      _kb_active = false;
  int       _kb_rename_idx = -1;     // -1 = marking new, ≥0 = renaming that index
  int32_t   _mark_lat = 0, _mark_lon = 0;   // pending new-mark position
  uint32_t  _mark_ts  = 0;

  // ADD form — type a waypoint by coordinates. The keyboard has no comma or
  // minus, so lat/lon are entered as magnitude + a hemisphere toggle (LEFT/RIGHT).
  char      _add_lat[12] = "", _add_lon[12] = "";   // magnitude strings
  char      _add_label[WAYPOINT_LABEL_LEN] = "";
  bool      _add_lat_neg = false;    // true = S
  bool      _add_lon_neg = false;    // true = W
  int       _add_sel  = 0;           // 0=Lat 1=Lon 2=Label 3=Save
  int       _kb_field = -1;          // which ADD field the keyboard edits (-1 = label/rename)

  bool useImperial() const { return _task && _task->useImperial(); }
  bool ownPos(int32_t& lat, int32_t& lon) const { return _task->currentLocation(lat, lon); }

  // The nav list shows a synthetic "Trail start" row (index 0) whenever a trail
  // exists, followed by the saved waypoints. These map the combined row index
  // (_sel) onto that layout.
  bool hasStart() const { return !_store->empty(); }
  int  wpListCount() const { return (hasStart() ? 1 : 0) + _task->waypoints().count(); }
  bool selIsStart() const { return hasStart() && _sel == 0; }
  int  wpIndex() const { return _sel - (hasStart() ? 1 : 0); }  // index into WaypointStore

  // Add a waypoint by coordinates (no GPS fix needed). Opens the ADD form.
  void openAddForm() {
    if (_task->waypoints().full()) { _task->showAlert("Waypoints full", 1000); return; }
    _add_lat[0] = _add_lon[0] = _add_label[0] = '\0';
    _add_lat_neg = _add_lon_neg = false;
    _add_sel = 0;
    _mode = ADD;
  }

  // Store the just-typed value into the focused ADD field.
  void applyAddField(const char* buf) {
    char* dst = (_kb_field == 0) ? _add_lat
              : (_kb_field == 1) ? _add_lon : _add_label;
    int   cap = (_kb_field == 2) ? WAYPOINT_LABEL_LEN : (int)sizeof(_add_lat);
    strncpy(dst, buf, cap - 1);
    dst[cap - 1] = '\0';
  }

  // Parse a numeric field: true only when the whole string is a number (so a
  // stray letter from the full keyboard can't silently read as 0).
  static bool parseNum(const char* s, double& out) {
    if (!s || !s[0]) return false;
    char* end = nullptr;
    out = strtod(s, &end);
    if (end == s) return false;          // nothing numeric parsed
    while (*end == ' ') end++;
    return *end == '\0';                  // no trailing non-numeric chars
  }

  // Validate the ADD form and save the waypoint.
  void commitAddForm() {
    double la, lo;
    if (!parseNum(_add_lat, la) || !parseNum(_add_lon, lo)) {
      _task->showAlert("Lat/Lon invalid", 1200); return;
    }
    if (_add_lat_neg) la = -la;
    if (_add_lon_neg) lo = -lo;
    if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0) {
      _task->showAlert("Out of range", 1200); return;
    }
    if (_task->waypoints().full()) { _task->showAlert("Waypoints full", 1000); return; }
    int32_t lat = (int32_t)(la * 1e6 + (la < 0 ? -0.5 : 0.5));
    int32_t lon = (int32_t)(lo * 1e6 + (lo < 0 ? -0.5 : 0.5));
    char label[WAYPOINT_LABEL_LEN];
    if (_add_label[0]) { strncpy(label, _add_label, sizeof(label) - 1); label[sizeof(label) - 1] = '\0'; }
    else               snprintf(label, sizeof(label), "WP%d", _task->waypoints().count() + 1);
    if (_task->addWaypoint(lat, lon, label)) { _mode = OFF; }
  }

  // Open the shared keyboard for a literal field. Waypoint labels and the
  // lat/lon magnitudes are never placeholder-expanded, so {loc}/{time} are
  // cleared (same intent as the bot's literal trigger field).
  void openKb(const char* initial, int max) {
    KeyboardWidget& kb = _task->keyboard();
    kb.begin(initial, max);
    kb.clearPlaceholders();
    _kb_active = true;
  }

  // Commit a mark-here / rename label from the keyboard.
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
    _task->addWaypoint(_mark_lat, _mark_lon, _mark_ts, buf);
  }

  void renderAddForm(DisplayDriver& display) {
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("ADD WAYPOINT");
    const int top  = display.listStart();
    const int step = display.lineStep();
    for (int i = 0; i < 4; i++) {
      int y = top + i * step;
      bool sel = (i == _add_sel);
      display.drawSelectionRow(0, y - 1, display.width(), step - 1, sel);
      char row[28];
      if      (i == 0) snprintf(row, sizeof(row), "Lat: %c %s", _add_lat_neg ? 'S' : 'N', _add_lat[0] ? _add_lat : "--");
      else if (i == 1) snprintf(row, sizeof(row), "Lon: %c %s", _add_lon_neg ? 'W' : 'E', _add_lon[0] ? _add_lon : "--");
      else if (i == 2) snprintf(row, sizeof(row), "Label: %s",  _add_label[0] ? _add_label : "(auto)");
      else             snprintf(row, sizeof(row), "[Save]");
      display.setCursor(2, y); display.print(row);
      display.setColor(DisplayDriver::LIGHT);
    }
  }

  void renderWpList(DisplayDriver& display) {
    display.setColor(DisplayDriver::LIGHT);
    char title[24];
    snprintf(title, sizeof(title), "WAYPOINTS %d/%d",
             _task->waypoints().count(), WaypointStore::CAPACITY);
    display.drawCenteredHeader(title);

    int n     = wpListCount();
    int total = n + 1;                    // final row = "+ Add by coords"
    const int step = display.lineStep();

    int32_t mylat, mylon; bool have = ownPos(mylat, mylon);

    drawList(display, total, _sel, _scroll, [&](int row, int y, bool sel, int reserve) {
      display.drawSelectionRow(0, y - 1, display.width() - reserve, step - 1, sel);

      if (row == n) {                     // the synthetic "Add" row
        display.setCursor(2, y); display.print("+ Add by coords");
        display.setColor(DisplayDriver::LIGHT);
        return;
      }

      int32_t tlat, tlon; const char* label;
      if (!rowTarget(row, tlat, tlon, label)) return;
      char dist[12] = ""; int bw = 0;
      if (have) {
        geo::fmtDist(dist, sizeof(dist), geo::haversineKm(mylat, mylon, tlat, tlon), useImperial());
        bw = display.getTextWidth(dist) + 2;
      }
      char nm[24];
      display.translateUTF8ToBlocks(nm, label, sizeof(nm));
      display.drawTextEllipsized(2, y, display.width() - 2 - bw - reserve, nm);
      if (dist[0]) { display.setCursor(display.width() - bw + 1 - reserve, y); display.print(dist); }
      display.setColor(DisplayDriver::LIGHT);
    });
  }

  void renderWpNav(DisplayDriver& display) {
    int32_t tlat, tlon; const char* label;
    if (!rowTarget(_sel, tlat, tlon, label)) { _mode = LIST; return; }
    int32_t mylat, mylon; bool have = ownPos(mylat, mylon);
    int cog; bool cogv = _task->currentCourse(cog);
    navview::draw(display, have, mylat, mylon, tlat, tlon, label, cogv, cog, useImperial());
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

public:
  WaypointsView(UITask* task, TrailStore* store) : _task(task), _store(store) {}

  void reset() { _mode = OFF; _ctx.active = false; _kb_active = false; _kb_field = -1; }

  // True while the component owns the screen (a sub-mode or the keyboard is up).
  bool active() const { return _mode != OFF || _kb_active; }

  // Entry points called from TrailScreen's action menu.
  void openList() { _mode = LIST; _sel = 0; _scroll = 0; }
  void markHere() {
    int32_t lat, lon;
    if (!ownPos(lat, lon))         { _task->showAlert("No GPS fix", 1000); return; }
    if (_task->waypoints().full()) { _task->showAlert("Waypoints full", 1000); return; }
    _mark_lat = lat; _mark_lon = lon; _mark_ts = (uint32_t)rtc_clock.getCurrentTime();
    _kb_rename_idx = -1;
    openKb("", WAYPOINT_LABEL_LEN - 1);
  }

  // Only called while active().
  int render(DisplayDriver& display) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    if (_kb_active) return _task->keyboard().render(display);  // keyboard owns the screen
    if (_mode == ADD) { renderAddForm(display); return 1000; }
    if (_mode == NAV) { renderWpNav(display);   return 1000; }
    renderWpList(display);                          // LIST
    if (_ctx.active) _ctx.render(display);
    return 1000;
  }

  // Returns true if the input was consumed (always, while active()).
  bool handleInput(char c) {
    // Label keyboard (mark new / rename / ADD field) takes all input first.
    if (_kb_active) {
      KeyboardWidget& kb = _task->keyboard();
      auto r = kb.handleInput(c);
      if (r == KeyboardWidget::DONE) {
        if (_kb_field >= 0) applyAddField(kb.buf);    // ADD field edit
        else                commitLabel(kb.buf);      // mark / rename label
        _kb_active = false; _kb_field = -1;
      } else if (r == KeyboardWidget::CANCELLED) {
        _kb_active = false; _kb_field = -1;
      }
      return true;
    }

    // Rename/Delete/Send popup — operates on the saved-waypoint index (the
    // synthetic Trail-start row, if any, is never editable).
    if (_ctx.active) {
      auto res = _ctx.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int sel = _ctx.selectedIndex();
        int wi  = wpIndex();
        if (sel == 0) {                                  // Rename
          if (wi >= 0 && wi < _task->waypoints().count()) {
            _kb_rename_idx = wi;
            openKb(_task->waypoints().at(wi).label, WAYPOINT_LABEL_LEN - 1);
          }
        } else if (sel == 1) {                            // Delete
          if (wi >= 0 && wi < _task->waypoints().count()) {
            _task->waypoints().remove(wi);
            _task->saveWaypoints();
            if (_sel >= wpListCount()) _sel = wpListCount() - 1;
            if (_sel < 0) _sel = 0;
          }
        } else {                                          // Send (share in a message)
          if (wi >= 0 && wi < _task->waypoints().count()) {
            const Waypoint& w = _task->waypoints().at(wi);
            double lat = w.lat_1e6 / 1000000.0, lon = w.lon_1e6 / 1000000.0;
            char text[80];
            if (w.label[0]) snprintf(text, sizeof(text), WAYPOINT_MSG_TAG "%.5f,%.5f %s", lat, lon, w.label);
            else            snprintf(text, sizeof(text), WAYPOINT_MSG_TAG "%.5f,%.5f", lat, lon);
            _task->shareToMessage(text);   // hands off to the Messages screen
          }
        }
      }
      return true;
    }

    // ADD form: type lat/lon (magnitude) + hemisphere toggle + label.
    if (_mode == ADD) {
      if (c == KEY_CANCEL) { _mode = OFF; return true; }
      if (c == KEY_UP   && _add_sel > 0) { _add_sel--; return true; }
      if (c == KEY_DOWN && _add_sel < 3) { _add_sel++; return true; }
      if (c == KEY_LEFT || c == KEY_RIGHT) {
        if      (_add_sel == 0) _add_lat_neg = !_add_lat_neg;   // N <-> S
        else if (_add_sel == 1) _add_lon_neg = !_add_lon_neg;   // E <-> W
        return true;
      }
      if (c == KEY_ENTER) {
        if      (_add_sel == 0) { _kb_field = 0; openKb(_add_lat, 11); }
        else if (_add_sel == 1) { _kb_field = 1; openKb(_add_lon, 11); }
        else if (_add_sel == 2) { _kb_field = 2; openKb(_add_label, WAYPOINT_LABEL_LEN - 1); }
        else                    { commitAddForm(); }
        return true;
      }
      return true;
    }

    // Navigation view — any nav key returns to the list.
    if (_mode == NAV) {
      if (c == KEY_CANCEL || keyIsPrev(c) || keyIsNext(c)) { _mode = LIST; }
      return true;
    }

    // List (row 0 is the synthetic "Trail start" when a trail exists; the final
    // row is "+ Add by coords"). Cancel returns control to the trail views.
    int n = wpListCount();
    int total = n + 1;                       // + the "Add by coords" row
    if (c == KEY_CANCEL) { _mode = OFF; return true; }
    // drawList() reclamps _scroll from _sel every render.
    if (c == KEY_UP   && _sel > 0)         { _sel--; return true; }
    if (c == KEY_DOWN && _sel < total - 1) { _sel++; return true; }
    if (c == KEY_ENTER) {
      if (_sel == n) openAddForm();          // last row → open the add form
      else           _mode = NAV;            // a waypoint / Trail-start row
      return true;
    }
    // Rename/Delete/Send apply to saved waypoints only — not Trail-start or Add.
    if (c == KEY_CONTEXT_MENU && !selIsStart() && _sel != n &&
        wpIndex() >= 0 && wpIndex() < _task->waypoints().count()) {
      _ctx.begin("Waypoint", 3);
      _ctx.addItem("Rename");
      _ctx.addItem("Delete");
      _ctx.addItem("Send");
      return true;
    }
    return true;
  }
};
