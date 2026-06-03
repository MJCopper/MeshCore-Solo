#pragma once
#include <math.h>
#include "../GeoUtils.h"
#include "NavView.h"

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

class NearbyScreen : public UIScreen {
  UITask* _task;

  int _visible   = 4;   // updated each render; used by handleInput for scroll clamping

  static const int FILTER_COUNT = 6;
  static const char*    FILTER_LABELS[FILTER_COUNT];
  static const uint8_t  FILTER_TYPES[FILTER_COUNT];

  // ── nearby list state ────────────────────────────────────────────────────────
  struct Entry {
    char     name[32];
    int32_t  lat_e6, lon_e6;
    float    dist_km;
    uint8_t  type;
    int      contact_idx;
    uint32_t lastmod;
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
  uint8_t _filter;

  unsigned long _detail_refresh_ms;
  static const unsigned long DETAIL_REFRESH_MS = 10000UL;

  PopupMenu _ctx_menu;
  PopupMenu _opts;            // detail-view Options: Navigate / Ping
  PopupMenu _ping_menu;
  char _ping_time_str[24];    // For storing ping RTT result
  char _ping_snr_out_str[24]; // For storing ping SNR out result
  char _ping_snr_back_str[24];// For storing ping SNR back result

  // ── ping state ─────────────────────────────────────────────────────────────
  bool _pinging;
  unsigned long _ping_started_ms;
  static const unsigned long PING_TIMEOUT_MS = 3000UL;

  // ── discover sub-screen state ────────────────────────────────────────────────
  bool          _discover_mode;
  bool          _discovering;
  unsigned long _discover_started_ms;
  static const unsigned long DISCOVER_DURATION_MS = 8000UL;

  DiscoverResult _dresults[DISCOVER_RESULTS_MAX];
  int            _dresult_count;
  int            _dscroll;
  int            _dsel;
  bool           _ddetail;

  int _d_visible = 2;  // updated each render; used by handleInputDiscover

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

  // Geographic helpers live in GeoUtils.h (geo::) — shared with Waypoints /
  // trail course-over-ground. Thin forwarders keep the existing call sites.
  static float haversineKm(int32_t a, int32_t b, int32_t c, int32_t d) { return geo::haversineKm(a, b, c, d); }
  static int   bearingDeg(int32_t a, int32_t b, int32_t c, int32_t d)   { return geo::bearingDeg(a, b, c, d); }
  static const char* bearingCardinal(int deg) { return geo::bearingCardinal(deg); }
  static void  fmtDist(char* buf, int n, float km) { geo::fmtDist(buf, n, km); }

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

  void refresh() {
    _count = 0;
    _own_lat = _own_lon = 0;
    _own_gps = _task->currentLocation(_own_lat, _own_lon);

    int nc = the_mesh.getNumContacts();
    for (int i = 0; i < nc && _count < MAX_NEARBY; i++) {
      ContactInfo ci;
      if (!the_mesh.getContactByIdx(i, ci)) continue;
      if (_filter == 0 && !(ci.flags & 1)) continue;
      if (_filter >= 2 && ci.type != FILTER_TYPES[_filter]) continue;

      Entry& e = _entries[_count++];
      strncpy(e.name, ci.name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      e.lat_e6  = ci.gps_lat;
      e.lon_e6  = ci.gps_lon;
      bool remote_gps = (ci.gps_lat != 0 || ci.gps_lon != 0);
      e.dist_km = (_own_gps && remote_gps)
                    ? haversineKm(_own_lat, _own_lon, ci.gps_lat, ci.gps_lon)
                    : -1.0f;
      e.type        = ci.type;
      e.contact_idx = i;
      e.lastmod     = ci.lastmod;
    }

    // sort by distance ascending; nodes without GPS go to the end
    for (int i = 0; i < _count - 1; i++) {
      int best = i;
      for (int j = i + 1; j < _count; j++) {
        float dj = _entries[j].dist_km, db = _entries[best].dist_km;
        if (dj >= 0.0f && (db < 0.0f || dj < db)) best = j;
      }
      if (best != i) { Entry tmp = _entries[i]; _entries[i] = _entries[best]; _entries[best] = tmp; }
    }

    if (_count == 0) {
      _sel = _scroll = 0;
    } else if (_sel >= _count) {
      _sel = _count - 1;
      if (_scroll > _sel) _scroll = _sel;
    }
  }

  // ── discover sub-screen ──────────────────────────────────────────────────────
  void enterDiscoverMode() {
    _discover_mode = true;
    _discovering   = true;
    _ddetail       = false;
    _discover_started_ms = millis();
    _dresult_count = 0;
    _dscroll = 0;
    _dsel    = 0;
    the_mesh.sendNodeDiscoverReq();
  }

  void resetPingLines() {
    _ping_time_str[0] = '\0';
    _ping_snr_out_str[0] = '\0';
    _ping_snr_back_str[0] = '\0';
  }

  // Number of rows the ping menu should currently show: "Send" plus only the
  // result lines that are populated (avoids blank scrollable rows).
  int pingRowCount() const {
    return 1 + (_ping_time_str[0] ? 1 : 0)
             + (_ping_snr_out_str[0] ? 1 : 0)
             + (_ping_snr_back_str[0] ? 1 : 0);
  }

  void rebuildPingMenu() {
    _ping_menu.begin("Ping", 4);
    _ping_menu.addItem("Send");
    if (_ping_time_str[0])     _ping_menu.addItem(_ping_time_str);
    if (_ping_snr_out_str[0])  _ping_menu.addItem(_ping_snr_out_str);
    if (_ping_snr_back_str[0])  _ping_menu.addItem(_ping_snr_back_str);
  }

  void openPingMenu() { rebuildPingMenu(); }

  bool renderPingMenuIfActive(DisplayDriver& display) {
    if (!_ping_menu.active) return false;
    _ping_menu.render(display);
    return true;
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
      if (snr_out != 0) {
        snprintf(_ping_snr_out_str, sizeof(_ping_snr_out_str), "SNR out: %.1f", snr_out / 4.0f);
      }
      if (snr_back != 0) {
        snprintf(_ping_snr_back_str, sizeof(_ping_snr_back_str), "SNR back: %.1f", snr_back / 4.0f);
      }
      _pinging = false;
    } else if (_pinging && millis() - _ping_started_ms >= PING_TIMEOUT_MS) {
      snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: timeout");
      _ping_snr_out_str[0] = '\0';
      _ping_snr_back_str[0] = '\0';
      _pinging = false;
      if (_task) _task->clearPing();
    }
    // Keep the menu in sync with the populated result lines (grows as the
    // reply arrives; never shows blank rows).
    if (pingRowCount() != _ping_menu._count) rebuildPingMenu();
  }

  bool handlePingMenuInput(char c, const uint8_t* pub_key, bool allow_enter_to_open = false) {
    if (!_ping_menu.active) {
      if (c == KEY_CONTEXT_MENU || (allow_enter_to_open && c == KEY_ENTER)) {
        openPingMenu();
        return true;
      }
      return false;
    }

    // Rows 1-3 are read-only result fields (RTT, SNR out, SNR back) — swallow
    // UP/DOWN so the highlight stays on the "Ping" action and the user can't
    // ENTER on a result row by accident.
    if (c == KEY_UP || c == KEY_DOWN) return true;

    auto res = _ping_menu.handleInput(c);
    if (res == PopupMenu::SELECTED) {
      if (!_pinging && pub_key) {
        startPingForKey(pub_key);
      }
      // Keep the popup open so Ping stays available after completion.
      _ping_menu.active = true;
    } else if (res == PopupMenu::CANCELLED) {
      closePingMenu();
    }
    return true;
  }

  bool selectedStoredPubKey(uint8_t* out_pub_key) const {
    ContactInfo ci;
    if (_task && _sel < _count && _entries[_sel].contact_idx >= 0 &&
        the_mesh.getContactByIdx(_entries[_sel].contact_idx, ci)) {
      memcpy(out_pub_key, ci.id.pub_key, PUB_KEY_SIZE);
      return true;
    }
    return false;
  }

  const uint8_t* selectedDiscoverPubKey() const {
    return (_dsel < _dresult_count) ? _dresults[_dsel].pub_key : nullptr;
  }

  bool renderDiscoverDetail(DisplayDriver& display) {
    const DiscoverResult& r = _dresults[_dsel];
    const int hdr = display.headerH();
    const char* fullType = (r.type == ADV_TYPE_REPEATER) ? "Repeater" :
                           (r.type == ADV_TYPE_SENSOR)   ? "Sensor"   :
                           (r.type == ADV_TYPE_ROOM)     ? "Room"     : "Node";

    char label[32];
    if (r.name[0]) { strncpy(label, r.name, 31); label[31] = '\0'; }
    else           { snprintf(label, sizeof(label), "[%s]", fullType); }
    char filtered[32];
    display.translateUTF8ToBlocks(filtered, label, sizeof(filtered));
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, 0, display.width(), hdr - 1);
    display.setColor(DisplayDriver::DARK);
    display.drawTextEllipsized(2, 1, display.width() - 4, filtered);
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, hdr - 1, display.width(), 1);

    char b64[48];
    pubKeyToBase64(r.pub_key, b64, sizeof(b64));
    {
      int max_chars = (display.width() - 4) / display.getCharWidth();
      int b64_len   = strlen(b64);
      char b64_line[48];
      // Need at least 4 chars (one char + "..." ellipsis) to display anything
      // meaningful; on a very narrow display skip the pubkey line entirely
      // rather than risk a negative strncpy length.
      if (max_chars < 4) {
        b64_line[0] = '\0';
      } else if (b64_len > max_chars) {
        strncpy(b64_line, b64, max_chars - 3);
        b64_line[max_chars - 3] = '\0';
        strcat(b64_line, "...");
      } else {
        strncpy(b64_line, b64, sizeof(b64_line) - 1);
        b64_line[sizeof(b64_line) - 1] = '\0';
      }
      if (b64_line[0]) {
        display.setCursor(2, hdr);
        display.print(b64_line);
      }
    }

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)r.rssi);
    display.setCursor(2, hdr + step);     display.print(buf);
    snprintf(buf, sizeof(buf), "SNR:  %.1f dB", r.snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 2); display.print(buf);
    snprintf(buf, sizeof(buf), "Rem:  %.1f dB", r.remote_snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 3); display.print(buf);
    display.setCursor(2, hdr + step * 4);
    display.print(r.is_known ? "Status: known" : "Status: new");

    updatePingMenuState();
    if (renderPingMenuIfActive(display)) return true;
    return true;
  }

  bool renderStoredDetail(DisplayDriver& display) {
    const Entry& e = _entries[_sel];
    const int hdr  = display.headerH();

    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, 0, display.width(), hdr - 1);
    display.setColor(DisplayDriver::DARK);
    char filtered[32];
    display.translateUTF8ToBlocks(filtered, e.name, sizeof(filtered));
    display.drawTextEllipsized(2, 1, display.width() - 4, filtered);
    display.setColor(DisplayDriver::LIGHT);

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "Lat: %.5f", e.lat_e6 / 1e6);
    display.setCursor(2, hdr); display.print(buf);
    snprintf(buf, sizeof(buf), "Lon: %.5f", e.lon_e6 / 1e6);
    display.setCursor(2, hdr + step); display.print(buf);

    if (e.dist_km >= 0.0f) {
      char dist[12];
      fmtDist(dist, sizeof(dist), e.dist_km);
      int az = bearingDeg(_own_lat, _own_lon, e.lat_e6, e.lon_e6);
      snprintf(buf, sizeof(buf), "Dist: %s %s", dist, bearingCardinal(az));
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

    updatePingMenuState();
    if (_opts.active) { _opts.render(display); return true; }
    if (renderPingMenuIfActive(display)) return true;
    return true;
  }

  int renderDiscover(DisplayDriver& display) {
    int lh      = display.getLineHeight();
    int hdr     = display.headerH();
    int d_box_h = 2 * lh + 3;   // two text rows + padding
    int d_item_h = d_box_h + 2;
    int d_start_y = hdr;
    _d_visible  = display.listVisible(d_item_h);
    if (_d_visible < 1) _d_visible = 1;

    if (_ddetail) {
      renderDiscoverDetail(display);
      return _ping_menu.active ? 50 : 5000;
    }

    // ── list view ─────────────────────────────────────────────────────────────
    _dresult_count = the_mesh.getDiscoverResults(_dresults, DISCOVER_RESULTS_MAX);

    if (_discovering && millis() - _discover_started_ms >= DISCOVER_DURATION_MS)
      _discovering = false;

    display.setColor(DisplayDriver::LIGHT);
    char title[28];
    if (_discovering)
      snprintf(title, sizeof(title), "SCANNING... (%d)", _dresult_count);
    else if (_dresult_count == 0)
      snprintf(title, sizeof(title), "DISCOVER: none");
    else
      snprintf(title, sizeof(title), "DISCOVER (%d found)", _dresult_count);
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, hdr - 1, display.width(), 1);

    if (_dresult_count == 0) {
      display.drawTextCentered(display.width() / 2, display.height() / 2,
        _discovering ? "Waiting for replies..." : "No nodes found");
    } else {
      if (_dsel >= _dresult_count) _dsel = _dresult_count - 1;
      if (_dscroll > _dresult_count - _d_visible)
        _dscroll = _dresult_count > _d_visible ? _dresult_count - _d_visible : 0;
      if (_dscroll < 0) _dscroll = 0;

      for (int i = 0; i < _d_visible && (_dscroll + i) < _dresult_count; i++) {
        int idx = _dscroll + i;
        bool sel = (idx == _dsel);
        const DiscoverResult& r = _dresults[idx];
        int y = d_start_y + i * d_item_h;

        const char* typeStr = (r.type == ADV_TYPE_REPEATER) ? "Rpt"  :
                              (r.type == ADV_TYPE_SENSOR)   ? "Snsr" :
                              (r.type == ADV_TYPE_ROOM)     ? "Room" : "?";

        display.setColor(DisplayDriver::LIGHT);
        if (sel) {
          display.fillRect(0, y, display.width(), d_box_h);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.drawRect(0, y, display.width(), d_box_h);
          display.fillRect(1, y + 1, display.width() - 2, lh);
          display.setColor(DisplayDriver::DARK);
        }

        // header: name left, type right
        char label[32];
        if (r.name[0]) { strncpy(label, r.name, 31); label[31] = '\0'; }
        else {
          const char* ft = (r.type == ADV_TYPE_REPEATER) ? "Repeater" :
                           (r.type == ADV_TYPE_SENSOR)   ? "Sensor"   :
                           (r.type == ADV_TYPE_ROOM)     ? "Room"     : "Node";
          snprintf(label, sizeof(label), "[%s]", ft);
        }
        char filtered[32];
        display.translateUTF8ToBlocks(filtered, label, sizeof(filtered));
        int tw = display.getTextWidth(typeStr);
        display.drawTextEllipsized(3, y + 1, display.width() - 6 - tw, filtered);
        display.setCursor(display.width() - 3 - tw, y + 1);
        display.print(typeStr);

        // body: RSSI + SNR
        display.setColor(sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
        char sig[24];
        snprintf(sig, sizeof(sig), "RSSI:%d SNR:%.1f", (int)r.rssi, r.snr_x4 / 4.0f);
        display.drawTextEllipsized(3, y + lh + 2, display.width() - 6, sig);
      }

      display.setColor(DisplayDriver::LIGHT);
      int cw = display.getCharWidth();
      if (_dscroll > 0)
        { display.setCursor(display.width() - cw, d_start_y); display.print("^"); }
      if (_dscroll + _d_visible < _dresult_count)
        { display.setCursor(display.width() - cw, d_start_y + (_d_visible - 1) * d_item_h); display.print("v"); }
    }

    updatePingMenuState();

    // Show ping popup menu if active
    if (_ping_menu.active) {
      _ping_menu.render(display);
      return 50;
    }

    return _discovering ? 200 : 2000;
  }

  bool handleInputDiscover(char c) {
    if (_ddetail) {
      if (c == KEY_CANCEL) { 
        _ddetail = false; 
        closePingMenu();
        return true; 
      }

      if (handlePingMenuInput(c, selectedDiscoverPubKey())) return true;
      return true;
    }
    if (c == KEY_CANCEL) {
      _discover_mode = false;
      refresh();
      return true;
    }
    if (c == KEY_ENTER && _dresult_count > 0) {
      _ddetail = true;
      return true;
    }
    if (c == KEY_CONTEXT_MENU) {
      enterDiscoverMode();  // re-scan
      return true;
    }
    if (c == KEY_UP && _dsel > 0) {
      _dsel--;
      if (_dsel < _dscroll) _dscroll = _dsel;
      return true;
    }
    if (c == KEY_DOWN && _dsel < _dresult_count - 1) {
      _dsel++;
      if (_dsel >= _dscroll + _d_visible) _dscroll = _dsel - _d_visible + 1;
      return true;
    }
    return true;
  }

public:
  NearbyScreen(UITask* task)
    : _task(task), _count(0), _sel(0), _scroll(0), _detail(false),
      _own_lat(0), _own_lon(0), _own_gps(false), _filter(0),
      _detail_refresh_ms(0),
      _discover_mode(false), _discovering(false), _discover_started_ms(0),
      _dresult_count(0), _dscroll(0), _dsel(0), _ddetail(false),
      _pinging(false), _ping_started_ms(0) {
    _ping_time_str[0] = '\0';
    _ping_snr_out_str[0] = '\0';
    _ping_snr_back_str[0] = '\0';
  }

  void enter() {
    _sel = _scroll = 0;
    _detail = false;
    _nav = false;
    _filter = 0;
    _discover_mode = false;
    _ddetail = false;
    _dsel    = 0;
    _ctx_menu.active = false;
    _opts.active = false;
    _ping_menu.active = false;
    _pinging = false;
    _ping_time_str[0] = '\0';
    _ping_snr_out_str[0] = '\0';
    _ping_snr_back_str[0] = '\0';
    _task->clearPing();
    refresh();
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);

    // ── discover sub-screen ──────────────────────────────────────────────────
    if (_discover_mode) return renderDiscover(display);

    // periodic refresh in detail view — preserve selected contact by idx
    if (_detail && millis() - _detail_refresh_ms >= DETAIL_REFRESH_MS) {
      int saved_contact_idx = (_sel < _count) ? _entries[_sel].contact_idx : -1;
      refresh();
      bool found = false;
      if (saved_contact_idx >= 0) {
        for (int i = 0; i < _count; i++) {
          if (_entries[i].contact_idx == saved_contact_idx) { _sel = i; found = true; break; }
        }
      }
      if (!found) { _detail = false; _nav = false; }  // contact gone — drop both views
      _detail_refresh_ms = millis();
    }

    // ── navigate-to-node view ────────────────────────────────────────────────
    if (_nav && _sel < _count) {
      const Entry& e = _entries[_sel];
      int cog; bool cogv = _task->currentCourse(cog);
      navview::draw(display, _own_gps, _own_lat, _own_lon,
                    e.lat_e6, e.lon_e6, e.name, cogv, cog);
      return 1000;
    }

    // ── detail view ──────────────────────────────────────────────────────────
    if (_detail && _sel < _count) {
      renderStoredDetail(display);
      return _ping_menu.active ? 50 : 2000;
    }

    // ── list view ────────────────────────────────────────────────────────────
    int item_h   = display.lineStep();
    int start_y  = display.listStart();
    int dist_col = display.width() - display.getCharWidth() * 7;
    _visible     = display.listVisible(item_h);

    display.setColor(DisplayDriver::LIGHT);
    char title[22];
    snprintf(title, sizeof(title), "NEARBY[%s]", FILTER_LABELS[_filter]);
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    if (_count == 0) {
      display.drawTextCentered(display.width() / 2, display.height() / 2 - display.lineStep() / 2, "No contacts found");
      display.drawTextCentered(display.width() / 2, display.height() / 2 + display.lineStep() / 2, "[Enter]=Discover");
    } else {
      for (int i = 0; i < _visible && (_scroll + i) < _count; i++) {
        int idx = _scroll + i;
        bool sel = (idx == _sel);
        int y = start_y + i * item_h;
        const Entry& e = _entries[idx];

        display.drawSelectionRow(0, y - 1, display.width(), item_h - 1, sel);

        char filt[32];
        display.translateUTF8ToBlocks(filt, e.name, sizeof(filt));
        display.drawTextEllipsized(2, y, dist_col - 4, filt);

        display.setColor(sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
        char dist[10];
        if (e.dist_km >= 0.0f) fmtDist(dist, sizeof(dist), e.dist_km);
        else                   strncpy(dist, "?GPS", sizeof(dist));
        display.setCursor(dist_col, y);
        display.print(dist);
      }

      display.setColor(DisplayDriver::LIGHT);
      int cw = display.getCharWidth();
      if (_scroll > 0)
        { display.setCursor(display.width() - cw, start_y); display.print("^"); }
      if (_scroll + _visible < _count)
        { display.setCursor(display.width() - cw, start_y + (_visible - 1) * item_h); display.print("v"); }
    }

    if (_ctx_menu.active) {
      _ctx_menu.render(display);
      return 50;
    }

    return _count == 0 ? 3000 : 2000;
  }

  bool handleInput(char c) override {
    // ── discover sub-screen ──────────────────────────────────────────────────
    if (_discover_mode) return handleInputDiscover(c);

    // ── navigate-to-node view — any nav key returns to detail ─────────────────
    if (_nav) {
      if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_PREV ||
          c == KEY_RIGHT  || c == KEY_NEXT) { _nav = false; }
      return true;
    }

    // ── detail view ─────────────────────────────────────────────────────────
    if (_detail) {
      // Options popup (Hold Enter) — Navigate / Ping.
      if (_opts.active) {
        auto res = _opts.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          if (_opts.selectedIndex() == 0) {            // Navigate
            if (_sel < _count) {
              const Entry& e = _entries[_sel];
              if (e.lat_e6 != 0 || e.lon_e6 != 0) _nav = true;
              else _task->showAlert("No node GPS", 1000);
            }
          } else {                                      // Ping
            uint8_t pk[PUB_KEY_SIZE];
            openPingMenu();
            if (selectedStoredPubKey(pk)) startPingForKey(pk);
          }
        }
        return true;
      }

      // Ping popup (opened from Options) consumes input while active.
      if (_ping_menu.active) {
        uint8_t pk[PUB_KEY_SIZE]; selectedStoredPubKey(pk);
        handlePingMenuInput(c, pk);
        return true;
      }

      if (c == KEY_CANCEL) { _detail = false; closePingMenu(); return true; }
      if (c == KEY_CONTEXT_MENU) {
        _opts.begin("Options", 2);
        _opts.addItem("Navigate");
        _opts.addItem("Ping");
        return true;
      }
      return true;
    }

    // ── context menu ─────────────────────────────────────────────────────────
    if (_ctx_menu.active) {
      auto res = _ctx_menu.handleInput(c);
      if (res == PopupMenu::SELECTED)
        enterDiscoverMode();
      return true;
    }

    // ── list view ────────────────────────────────────────────────────────────
    if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
    if (c == KEY_CONTEXT_MENU) {
      _ctx_menu.begin("Options", 1);
      _ctx_menu.addItem("Discover nearby");
      return true;
    }
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
    if (c == KEY_ENTER && _count == 0) { enterDiscoverMode(); return true; }
    if (c == KEY_ENTER && _count > 0) {
      _detail = true;
      _detail_refresh_ms = millis();
      return true;
    }
    if (c == KEY_LEFT) {
      _filter = (_filter + FILTER_COUNT - 1) % FILTER_COUNT;
      refresh();
      return true;
    }
    if (c == KEY_RIGHT) {
      _filter = (_filter + 1) % FILTER_COUNT;
      refresh();
      return true;
    }
    return false;
  }
};

const char*   NearbyScreen::FILTER_LABELS[6] = { "Fav", "ALL", "Comp", "Rpt", "Room", "Snsr" };
const uint8_t NearbyScreen::FILTER_TYPES[6]  = { 0, 0, ADV_TYPE_CHAT, ADV_TYPE_REPEATER, ADV_TYPE_ROOM, ADV_TYPE_SENSOR };
