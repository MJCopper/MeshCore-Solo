#pragma once
#include <math.h>
#include "../GeoUtils.h"
#include "NavView.h"

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// ── Nearby Nodes ──────────────────────────────────────────────────────────────
// One list / detail / action-menu interaction path over two sources:
//   SRC_STORED — contacts known to the mesh (distance / bearing / last-heard)
//   SRC_SCAN   — live NODE_DISCOVER_REQ results (RSSI / SNR / status)
// Filter (type) and sort are independent axes and combine freely. The action
// menu (Hold Enter) is identical everywhere; only the per-row column and the
// detail fields differ between sources.
class NearbyScreen : public UIScreen {
  UITask* _task;

  int _visible = 4;   // updated each render; used by handleInput for scroll clamping

  // ── filter (type axis) ──────────────────────────────────────────────────────
  enum Filter : uint8_t { F_ALL, F_FAV, F_COMP, F_RPT, F_ROOM, F_SNSR, F_COUNT };
  static const char* FILTER_LABELS[F_COUNT];

  // ── sort axis ───────────────────────────────────────────────────────────────
  enum Sort : uint8_t { SORT_DIST, SORT_TIME };

  // ── source ──────────────────────────────────────────────────────────────────
  enum Source : uint8_t { SRC_STORED, SRC_SCAN };

  // ── action-menu actions (matched by id, not by row index) ────────────────────
  enum Action : uint8_t { ACT_NAV, ACT_PING, ACT_WAYPOINT, ACT_SORT, ACT_SCAN };

  // ── unified list entry ───────────────────────────────────────────────────────
  struct Entry {
    char     name[32];
    uint8_t  type;
    uint8_t  pub_key[PUB_KEY_SIZE];
    bool     has_key;
    // stored-source fields
    int32_t  lat_e6, lon_e6;
    float    dist_km;
    uint32_t lastmod;
    int      contact_idx;
    // scan-source fields
    int8_t   rssi, snr_x4, remote_snr_x4;
    bool     is_known;
  };

  static const int MAX_NEARBY = 32;
  Entry   _entries[MAX_NEARBY];
  int     _count;
  int     _sel;
  int     _scroll;
  bool    _detail;
  bool    _nav = false;     // full-screen navigate-to-node view (over detail)
  int32_t _own_lat, _own_lon;
  bool    _own_gps;

  Source  _source;
  uint8_t _filter;
  uint8_t _sort;

  unsigned long _detail_refresh_ms;
  unsigned long _list_refresh_ms = 0;
  static const unsigned long DETAIL_REFRESH_MS    = 10000UL;
  static const unsigned long TIME_LIST_REFRESH_MS = 3000UL;

  // ── live-scan state ──────────────────────────────────────────────────────────
  bool          _scanning;
  unsigned long _scan_started_ms;
  static const unsigned long SCAN_DURATION_MS = 8000UL;

  // ── popups ────────────────────────────────────────────────────────────────────
  PopupMenu _menu;            // unified action menu (Hold Enter), list + detail
  PopupMenu _ping_menu;       // ping (special: read-only result rows)

  Action  _menu_actions[8];   // parallel to _menu rows — stable action ids
  int     _menu_action_count;
  char    _sort_label[16];    // dynamic label for the Sort row

  // ── ping state ───────────────────────────────────────────────────────────────
  char _ping_time_str[24];
  char _ping_snr_out_str[24];
  char _ping_snr_back_str[24];
  bool _pinging;
  unsigned long _ping_started_ms;
  static const unsigned long PING_TIMEOUT_MS = 3000UL;

  // ── helpers ──────────────────────────────────────────────────────────────────
  static void pubKeyToBase64(const uint8_t* key, char* out, int out_len) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < PUB_KEY_SIZE && j + 5 < out_len; i += 3) {
      uint32_t b = ((uint32_t)key[i] << 16)
                 | (i+1 < PUB_KEY_SIZE ? (uint32_t)key[i+1] << 8 : 0)
                 | (i+2 < PUB_KEY_SIZE ? (uint32_t)key[i+2]      : 0);
      out[j++] = T[(b >> 18) & 63];
      out[j++] = T[(b >> 12) & 63];
      if (i+1 < PUB_KEY_SIZE) out[j++] = T[(b >> 6) & 63];
      if (i+2 < PUB_KEY_SIZE) out[j++] = T[b & 63];
    }
    out[j] = '\0';
  }

  bool useImperial() const { return _task && _task->useImperial(); }

  static void fmtAge(char* buf, int n, uint32_t lastmod) {
    uint32_t now = rtc_clock.getCurrentTime();
    if (now < lastmod || lastmod == 0) { snprintf(buf, n, "unknown"); return; }
    uint32_t age = now - lastmod;
    if      (age < 60)     snprintf(buf, n, "%us ago",  age);
    else if (age < 3600)   snprintf(buf, n, "%um ago",  age / 60);
    else if (age < 86400)  snprintf(buf, n, "%uh ago",  age / 3600);
    else                   snprintf(buf, n, ">1d ago");
  }

  static const char* typeName(uint8_t t) {
    switch (t) {
      case ADV_TYPE_CHAT:     return "Companion";
      case ADV_TYPE_REPEATER: return "Repeater";
      case ADV_TYPE_ROOM:     return "Room";
      case ADV_TYPE_SENSOR:   return "Sensor";
      default:                return "Unknown";
    }
  }

  static const char* typeShort(uint8_t t) {
    switch (t) {
      case ADV_TYPE_REPEATER: return "Rpt";
      case ADV_TYPE_SENSOR:   return "Snsr";
      case ADV_TYPE_ROOM:     return "Room";
      case ADV_TYPE_CHAT:     return "Comp";
      default:                return "?";
    }
  }

  // The selected entry, or nullptr when the list is empty.
  const Entry* selected() const {
    return (_count > 0 && _sel < _count) ? &_entries[_sel] : nullptr;
  }

  bool typeMatchesFilter(uint8_t type, uint8_t flags, bool have_flags) const {
    switch (_filter) {
      case F_FAV:  return have_flags && (flags & 0x01);
      case F_COMP: return type == ADV_TYPE_CHAT;
      case F_RPT:  return type == ADV_TYPE_REPEATER;
      case F_ROOM: return type == ADV_TYPE_ROOM;
      case F_SNSR: return type == ADV_TYPE_SENSOR;
      case F_ALL:
      default:     return true;
    }
  }

  // ── data refresh ──────────────────────────────────────────────────────────────
  void refreshStored() {
    _count = 0;
    _own_lat = _own_lon = 0;
    _own_gps = _task->currentLocation(_own_lat, _own_lon);

    int nc = the_mesh.getNumContacts();
    for (int i = 0; i < nc && _count < MAX_NEARBY; i++) {
      ContactInfo ci;
      if (!the_mesh.getContactByIdx(i, ci)) continue;
      if (!typeMatchesFilter(ci.type, ci.flags, true)) continue;

      Entry& e = _entries[_count++];
      strncpy(e.name, ci.name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      memcpy(e.pub_key, ci.id.pub_key, PUB_KEY_SIZE);
      e.has_key = true;
      e.lat_e6  = ci.gps_lat;
      e.lon_e6  = ci.gps_lon;
      bool remote_gps = (ci.gps_lat != 0 || ci.gps_lon != 0);
      e.dist_km = (_own_gps && remote_gps)
                    ? geo::haversineKm(_own_lat, _own_lon, ci.gps_lat, ci.gps_lon)
                    : -1.0f;
      e.type        = ci.type;
      e.contact_idx = i;
      e.lastmod     = ci.lastmod;
      e.is_known    = true;
    }

    sortStored();
    clampSelection();
  }

  void sortStored() {
    uint32_t now_ts = rtc_clock.getCurrentTime();
    for (int i = 0; i < _count - 1; i++) {
      int best = i;
      for (int j = i + 1; j < _count; j++) {
        if (_sort == SORT_TIME) {
          // lastmod=0 or lastmod>now (RTC not synced) → "unknown" → sort to bottom.
          uint32_t tj = (_entries[j].lastmod > 0 && now_ts >= _entries[j].lastmod) ? _entries[j].lastmod : 0;
          uint32_t tb = (_entries[best].lastmod > 0 && now_ts >= _entries[best].lastmod) ? _entries[best].lastmod : 0;
          if (tj > 0 && (tb == 0 || tj > tb)) best = j;  // descending — most recent first
        } else {
          float dj = _entries[j].dist_km, db = _entries[best].dist_km;
          if (dj >= 0.0f && (db < 0.0f || dj < db)) best = j;  // ascending — closest first
        }
      }
      if (best != i) { Entry tmp = _entries[i]; _entries[i] = _entries[best]; _entries[best] = tmp; }
    }
  }

  void refreshScan() {
    static DiscoverResult dr[DISCOVER_RESULTS_MAX];  // scratch — refresh isn't reentrant
    int n = the_mesh.getDiscoverResults(dr, DISCOVER_RESULTS_MAX);
    _count = 0;
    for (int i = 0; i < n && _count < MAX_NEARBY; i++) {
      if (!typeMatchesFilter(dr[i].type, 0, false)) continue;
      Entry& e = _entries[_count++];
      strncpy(e.name, dr[i].name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      e.type          = dr[i].type;
      memcpy(e.pub_key, dr[i].pub_key, PUB_KEY_SIZE);
      e.has_key       = true;
      e.rssi          = dr[i].rssi;
      e.snr_x4        = dr[i].snr_x4;
      e.remote_snr_x4 = dr[i].remote_snr_x4;
      e.is_known      = dr[i].is_known;
      e.lat_e6 = e.lon_e6 = 0;
      e.dist_km = -1.0f;
      e.lastmod = 0;
      e.contact_idx = -1;
    }
    // strongest first
    for (int i = 0; i < _count - 1; i++) {
      int best = i;
      for (int j = i + 1; j < _count; j++)
        if (_entries[j].rssi > _entries[best].rssi) best = j;
      if (best != i) { Entry tmp = _entries[i]; _entries[i] = _entries[best]; _entries[best] = tmp; }
    }
    clampSelection();
  }

  void refresh() { if (_source == SRC_SCAN) refreshScan(); else refreshStored(); }

  void clampSelection() {
    if (_count == 0)            { _sel = _scroll = 0; }
    else if (_sel >= _count)    { _sel = _count - 1; if (_scroll > _sel) _scroll = _sel; }
  }

  // ── live scan ────────────────────────────────────────────────────────────────
  void enterScan() {
    _source        = SRC_SCAN;
    _detail        = false;
    _nav           = false;
    _scanning      = true;
    _scan_started_ms = millis();
    _sel = _scroll = 0;
    the_mesh.sendNodeDiscoverReq();
    refreshScan();
  }

  void leaveScan() {
    _source = SRC_STORED;
    _detail = false;
    _nav    = false;
    _sel = _scroll = 0;
    refreshStored();
  }

  // Save the currently-selected entry as a waypoint (its name as label).
  void saveSelectedWaypoint() {
    const Entry* e = selected();
    if (!e) return;
    if (e->lat_e6 == 0 && e->lon_e6 == 0) { _task->showAlert("No node GPS", 1000); return; }
    _task->addWaypoint(e->lat_e6, e->lon_e6, e->name);   // WaypointStore truncates the label
  }

  // ── ping ──────────────────────────────────────────────────────────────────────
  void resetPingLines() {
    _ping_time_str[0] = '\0';
    _ping_snr_out_str[0] = '\0';
    _ping_snr_back_str[0] = '\0';
  }

  int pingRowCount() const {
    return 1 + (_ping_time_str[0] ? 1 : 0)
             + (_ping_snr_out_str[0] ? 1 : 0)
             + (_ping_snr_back_str[0] ? 1 : 0);
  }

  void rebuildPingMenu() {
    int keep = _ping_menu.selectedIndex();  // preserve selection across a rebuild
    _ping_menu.begin("Ping", 4);
    _ping_menu.addItem("Send");
    if (_ping_time_str[0])      _ping_menu.addItem(_ping_time_str);
    if (_ping_snr_out_str[0])   _ping_menu.addItem(_ping_snr_out_str);
    if (_ping_snr_back_str[0])  _ping_menu.addItem(_ping_snr_back_str);
    _ping_menu.setSelected(keep);
  }

  void closePingMenu(bool clear_task = true) {
    _ping_menu.active = false;
    _pinging = false;
    if (clear_task && _task) _task->clearPing();
    resetPingLines();
  }

  void startPingForKey(const uint8_t* pub_key) {
    resetPingLines();
    snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: ...");
    if (_task && _task->startPing(pub_key)) {
      _pinging = true;
      _ping_started_ms = millis();
    } else {
      snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: send fail");
    }
    if (_ping_menu.active) rebuildPingMenu();   // surface "RTT: ..." immediately
  }

  void updatePingMenuState() {
    if (!_ping_menu.active || !_task || (!_pinging && !_task->isPingActive())) return;

    int16_t snr_out = 0, snr_back = 0;
    uint32_t rtt = 0;
    _task->getPingResult(snr_out, snr_back, rtt);

    if (_pinging && (snr_out != 0 || snr_back != 0 || rtt != 0)) {
      if (rtt > 0 && rtt < 10000) {
        snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: %lums", rtt);
      } else {
        snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: timeout");
      }
      if (snr_out != 0)
        snprintf(_ping_snr_out_str, sizeof(_ping_snr_out_str), "SNR out: %.1f", snr_out / 4.0f);
      if (snr_back != 0)
        snprintf(_ping_snr_back_str, sizeof(_ping_snr_back_str), "SNR back: %.1f", snr_back / 4.0f);
      _pinging = false;
    } else if (_pinging && millis() - _ping_started_ms >= PING_TIMEOUT_MS) {
      snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: timeout");
      _ping_snr_out_str[0] = '\0';
      _ping_snr_back_str[0] = '\0';
      _pinging = false;
      if (_task) _task->clearPing();
    }
    if (pingRowCount() != _ping_menu.count()) rebuildPingMenu();
  }

  // Ping popup input. Result rows are read-only, so UP/DOWN are swallowed to keep
  // the highlight on "Send".
  void handlePingMenuInput(char c) {
    if (c == KEY_UP || c == KEY_DOWN) return;
    auto res = _ping_menu.handleInput(c);
    if (res == PopupMenu::SELECTED) {
      const Entry* e = selected();
      if (!_pinging && e && e->has_key) startPingForKey(e->pub_key);
      _ping_menu.active = true;   // stay open so Ping can be repeated
    } else if (res == PopupMenu::CANCELLED) {
      closePingMenu();
    }
  }

  // ── action menu (Hold Enter) — same everywhere ──────────────────────────────
  void buildSortLabel() {
    snprintf(_sort_label, sizeof(_sort_label),
             "Sort: %s", _sort == SORT_TIME ? "Recent" : "Dist");
  }

  void openActionMenu() {
    const Entry* e = selected();
    bool stored  = (_source == SRC_STORED);
    bool has_gps = e && stored && (e->lat_e6 != 0 || e->lon_e6 != 0);
    bool has_key = e && e->has_key;

    buildSortLabel();
    _menu_action_count = 0;
    _menu.begin("Options", 5);
    auto add = [&](const char* label, Action a) {
      _menu.addItem(label);
      _menu_actions[_menu_action_count++] = a;
    };

    if (has_gps) add("Navigate",      ACT_NAV);
    if (has_key) add("Ping",          ACT_PING);
    if (has_gps) add("Save waypoint", ACT_WAYPOINT);
    if (stored) add(_sort_label, ACT_SORT);   // sort is meaningless for live-scan rows
    add(stored ? "Discover scan" : "Rescan", ACT_SCAN);
  }

  void runAction(Action a) {
    switch (a) {
      case ACT_NAV: {
        const Entry* e = selected();
        if (e && (e->lat_e6 != 0 || e->lon_e6 != 0)) _nav = true;
        else _task->showAlert("No node GPS", 1000);
        break;
      }
      case ACT_PING: {
        const Entry* e = selected();
        rebuildPingMenu();
        _ping_menu.active = true;
        if (e && e->has_key) startPingForKey(e->pub_key);
        break;
      }
      case ACT_WAYPOINT: saveSelectedWaypoint(); break;
      case ACT_SORT:     break;  // adjusted in-place via LEFT/RIGHT, not ENTER
      case ACT_SCAN:     enterScan();            break;
    }
  }

  // ── detail rendering ──────────────────────────────────────────────────────────
  void renderStoredDetail(DisplayDriver& display) {
    const Entry& e = _entries[_sel];
    const int hdr  = display.headerH();
    display.drawInvertedHeader(e.name);

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "Lat: %.5f", e.lat_e6 / 1e6);
    display.setCursor(2, hdr); display.print(buf);
    snprintf(buf, sizeof(buf), "Lon: %.5f", e.lon_e6 / 1e6);
    display.setCursor(2, hdr + step); display.print(buf);

    if (e.dist_km >= 0.0f) {
      char dist[12];
      geo::fmtDist(dist, sizeof(dist), e.dist_km, useImperial());
      int az = geo::bearingDeg(_own_lat, _own_lon, e.lat_e6, e.lon_e6);
      snprintf(buf, sizeof(buf), "Dist: %s %s", dist, geo::bearingCardinal(az));
    } else {
      snprintf(buf, sizeof(buf), "Dist: no GPS");
    }
    display.setCursor(2, hdr + step * 2); display.print(buf);
    snprintf(buf, sizeof(buf), "Type: %s", typeName(e.type));
    display.setCursor(2, hdr + step * 3); display.print(buf);
    char age[16];
    fmtAge(age, sizeof(age), e.lastmod);
    snprintf(buf, sizeof(buf), "Seen: %s", age);
    display.drawTextEllipsized(2, hdr + step * 4, display.width() - 4, buf);
  }

  void renderScanDetail(DisplayDriver& display) {
    const Entry& e = _entries[_sel];
    const int hdr = display.headerH();

    char label[32];
    if (e.name[0]) { strncpy(label, e.name, 31); label[31] = '\0'; }
    else           { snprintf(label, sizeof(label), "[%s]", typeName(e.type)); }
    display.drawInvertedHeader(label);

    char b64[48];
    pubKeyToBase64(e.pub_key, b64, sizeof(b64));
    int max_chars = (display.width() - 4) / display.getCharWidth();
    char b64_line[48];
    if (max_chars < 4) {
      b64_line[0] = '\0';
    } else if ((int)strlen(b64) > max_chars) {
      strncpy(b64_line, b64, max_chars - 3);
      b64_line[max_chars - 3] = '\0';
      strcat(b64_line, "...");
    } else {
      strncpy(b64_line, b64, sizeof(b64_line) - 1);
      b64_line[sizeof(b64_line) - 1] = '\0';
    }
    if (b64_line[0]) { display.setCursor(2, hdr); display.print(b64_line); }

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)e.rssi);
    display.setCursor(2, hdr + step);     display.print(buf);
    snprintf(buf, sizeof(buf), "SNR:  %.1f dB", e.snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 2); display.print(buf);
    snprintf(buf, sizeof(buf), "Rem:  %.1f dB", e.remote_snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 3); display.print(buf);
    display.setCursor(2, hdr + step * 4);
    display.print(e.is_known ? "Status: known" : "Status: new");
  }

  // Draw whichever popup is active over the current view. Returns true if one was.
  bool renderActivePopup(DisplayDriver& display) {
    updatePingMenuState();
    if (_ping_menu.active)   { _ping_menu.render(display);   return true; }
    if (_menu.active)        { _menu.render(display);        return true; }
    return false;
  }

public:
  NearbyScreen(UITask* task)
    : _task(task), _count(0), _sel(0), _scroll(0), _detail(false),
      _own_lat(0), _own_lon(0), _own_gps(false),
      _source(SRC_STORED), _filter(F_ALL), _sort(SORT_DIST),
      _detail_refresh_ms(0), _scanning(false), _scan_started_ms(0),
      _menu_action_count(0), _pinging(false), _ping_started_ms(0) {
    resetPingLines();
    _sort_label[0] = '\0';
  }

  void enter() {
    _sel = _scroll = 0;
    _detail = false;
    _nav = false;
    _source = SRC_STORED;
    // _filter / _sort persist across enter() — set once in the constructor
    _scanning = false;
    _menu.active = false;
    _ping_menu.active = false;
    _pinging = false;
    resetPingLines();
    _task->clearPing();
    refreshStored();
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);

    // periodic refresh in stored-detail — preserve selected contact by idx
    if (_detail && _source == SRC_STORED &&
        millis() - _detail_refresh_ms >= DETAIL_REFRESH_MS) {
      int saved = (_sel < _count) ? _entries[_sel].contact_idx : -1;
      refreshStored();
      bool found = false;
      if (saved >= 0)
        for (int i = 0; i < _count; i++)
          if (_entries[i].contact_idx == saved) { _sel = i; found = true; break; }
      if (!found) { _detail = false; _nav = false; }  // contact gone — drop both views
      _detail_refresh_ms = millis();
    }

    // ── navigate-to-node view ────────────────────────────────────────────────
    if (_nav && _sel < _count) {
      const Entry& e = _entries[_sel];
      int cog; bool cogv = _task->currentCourse(cog);
      navview::draw(display, _own_gps, _own_lat, _own_lon,
                    e.lat_e6, e.lon_e6, e.name, cogv, cog, useImperial());
      return 1000;
    }

    // ── detail view ──────────────────────────────────────────────────────────
    if (_detail && _sel < _count) {
      if (_source == SRC_SCAN) renderScanDetail(display);
      else                     renderStoredDetail(display);
      renderActivePopup(display);
      return _ping_menu.active ? 50 : 2000;
    }

    // ── list view ────────────────────────────────────────────────────────────
    if (_source == SRC_SCAN) {
      refreshScan();
      if (_scanning && millis() - _scan_started_ms >= SCAN_DURATION_MS) _scanning = false;
    } else if (_sort == SORT_TIME && millis() - _list_refresh_ms >= TIME_LIST_REFRESH_MS) {
      // re-sort periodically so newly-heard contacts bubble up
      refreshStored();
      _list_refresh_ms = millis();
    }

    int item_h   = display.lineStep();
    int start_y  = display.listStart();
    int dist_col = display.width() - display.getCharWidth() * 7;
    _visible     = display.listVisible(item_h);

    display.setColor(DisplayDriver::LIGHT);
    char title[28];
    const char* flt = (_filter != F_ALL) ? FILTER_LABELS[_filter] : nullptr;
    if (_source == SRC_SCAN) {
      const char* base = _scanning ? "SCANNING" : "SCAN";
      if (flt) {
        if (!_scanning && _count == 0) snprintf(title, sizeof(title), "SCAN %s: none", flt);
        else                           snprintf(title, sizeof(title), "%s %s (%d)", base, flt, _count);
      } else if (_scanning)            snprintf(title, sizeof(title), "SCANNING (%d)", _count);
      else if (_count == 0)            snprintf(title, sizeof(title), "SCAN: none");
      else                             snprintf(title, sizeof(title), "SCAN (%d)", _count);
    } else {
      snprintf(title, sizeof(title), "NEARBY %s %s",
               FILTER_LABELS[_filter], _sort == SORT_TIME ? "recent" : "dist");
    }
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    if (_count == 0) {
      char empty[24];
      if (_source == SRC_SCAN) {
        const char* msg;
        if (_scanning)   msg = "Waiting for replies...";
        else if (flt)  { snprintf(empty, sizeof(empty), "No %s nodes", flt); msg = empty; }
        else             msg = "No nodes found";
        display.drawTextCentered(display.width() / 2, display.height() / 2, msg);
      } else {
        const char* hint;
        if (flt) { snprintf(empty, sizeof(empty), "No %s contacts", flt); hint = "[<>] change filter"; }
        else     { snprintf(empty, sizeof(empty), "No contacts found");   hint = "[Enter]=Discover";   }
        display.drawTextCentered(display.width() / 2, display.height() / 2 - display.lineStep() / 2, empty);
        display.drawTextCentered(display.width() / 2, display.height() / 2 + display.lineStep() / 2, hint);
      }
    } else {
      int reserve = scrollIndicatorReserve(display, _count, _visible);
      for (int i = 0; i < _visible && (_scroll + i) < _count; i++) {
        int idx = _scroll + i;
        bool sel = (idx == _sel);
        int y = start_y + i * item_h;
        const Entry& e = _entries[idx];

        display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h - 1, sel);

        char filt[32];
        display.translateUTF8ToBlocks(filt, e.name, sizeof(filt));
        if (_source == SRC_SCAN && !e.name[0]) {  // unknown node → "[Type]"
          snprintf(filt, sizeof(filt), "[%s]", typeName(e.type));
        }
        display.drawTextEllipsized(2, y, dist_col - 4, filt);

        display.setColor(sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
        char right[10];
        if (_source == SRC_SCAN) {
          snprintf(right, sizeof(right), "%d", (int)e.rssi);
        } else if (_sort == SORT_TIME) {
          uint32_t now = rtc_clock.getCurrentTime();
          if (e.lastmod == 0 || now < e.lastmod)  snprintf(right, sizeof(right), "?");
          else {
            uint32_t age = now - e.lastmod;
            if      (age < 60)   snprintf(right, sizeof(right), "%us", age);
            else if (age < 3600) snprintf(right, sizeof(right), "%um", age / 60);
            else                 snprintf(right, sizeof(right), "%uh", age / 3600);
          }
        } else {
          if (e.dist_km >= 0.0f) geo::fmtDist(right, sizeof(right), e.dist_km, useImperial());
          else                   strncpy(right, "?GPS", sizeof(right));
        }
        display.setCursor(display.width() - display.getTextWidth(right) - 2 - reserve, y);
        display.print(right);
      }

      drawScrollIndicator(display, start_y, _visible * item_h, _count, _visible, _scroll);
    }

    if (renderActivePopup(display)) return 50;
    if (_source == SRC_SCAN) return _scanning ? 200 : 2000;
    return _count == 0 ? 3000 : 2000;
  }

  bool handleInput(char c) override {
    // ── navigate-to-node view — any nav key returns to detail ─────────────────
    if (_nav) {
      if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_PREV ||
          c == KEY_RIGHT  || c == KEY_NEXT) _nav = false;
      return true;
    }

    // ── popups (same handling in list and detail) ─────────────────────────────
    if (_ping_menu.active)   { handlePingMenuInput(c); return true; }
    if (_menu.active) {
      // LEFT/RIGHT on the Sort row toggles the value in-place and rebuilds the
      // label; the popup stays open so the user can keep tapping. Other rows
      // swallow L/R. ENTER on Sort just closes (value changes via L/R only).
      if (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_PREV || c == KEY_NEXT) {
        int i = _menu.selectedIndex();
        if (i >= 0 && i < _menu_action_count && _menu_actions[i] == ACT_SORT) {
          _sort = (_sort == SORT_DIST) ? SORT_TIME : SORT_DIST;
          buildSortLabel();
          refresh();
        }
        return true;
      }
      auto res = _menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int i = _menu.selectedIndex();
        if (i >= 0 && i < _menu_action_count && _menu_actions[i] != ACT_SORT)
          runAction(_menu_actions[i]);
      }
      return true;
    }

    // ── detail view ───────────────────────────────────────────────────────────
    if (_detail) {
      if (c == KEY_CANCEL)            { _detail = false; closePingMenu(); return true; }
      if (c == KEY_CONTEXT_MENU)      { openActionMenu(); return true; }
      return true;
    }

    // ── list view ───────────────────────────────────────────────────────────
    if (c == KEY_CANCEL) {
      if (_source == SRC_SCAN) leaveScan();
      else                     _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) { openActionMenu(); return true; }
    if (c == KEY_UP && _sel > 0) {
      _sel--;
      if (_sel < _scroll) _scroll = _sel;
      return true;
    }
    if (c == KEY_DOWN && _sel < _count - 1) {
      _sel++;
      if (_sel >= _scroll + _visible) _scroll = _sel - _visible + 1;
      return true;
    }
    if (c == KEY_ENTER) {
      if (_count == 0) { if (_source == SRC_STORED) enterScan(); return true; }
      _detail = true;
      _detail_refresh_ms = millis();
      return true;
    }
    if (c == KEY_LEFT)  { _filter = (_filter + F_COUNT - 1) % F_COUNT; refresh(); return true; }
    if (c == KEY_RIGHT) { _filter = (_filter + 1) % F_COUNT;          refresh(); return true; }
    return false;
  }
};

const char* NearbyScreen::FILTER_LABELS[F_COUNT] = { "All", "Fav", "Comp", "Rpt", "Room", "Snsr" };
