#pragma once
#include <math.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

class NearbyScreen : public UIScreen {
  UITask* _task;

  static const int VISIBLE   = 4;
  static const int ITEM_H    = 12;
  static const int START_Y   = 12;
  static const int DIST_COL  = 86;

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
  int32_t _own_lat, _own_lon;
  bool    _own_gps;
  uint8_t _filter;

  unsigned long _detail_refresh_ms;
  static const unsigned long DETAIL_REFRESH_MS = 10000UL;

  PopupMenu _ctx_menu;

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

  // ── helpers ──────────────────────────────────────────────────────────────────
  static float haversineKm(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    static const float DEG2RAD = (float)M_PI / 180.0f;
    float la1 = lat1 * (1e-6f * DEG2RAD);
    float la2 = lat2 * (1e-6f * DEG2RAD);
    float dla = (lat2 - lat1) * (1e-6f * DEG2RAD);
    float dlo = (lon2 - lon1) * (1e-6f * DEG2RAD);
    float a   = sinf(dla/2)*sinf(dla/2) + cosf(la1)*cosf(la2)*sinf(dlo/2)*sinf(dlo/2);
    return 6371.0f * 2.0f * asinf(sqrtf(a));
  }

  static int bearingDeg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    static const float DEG2RAD = (float)M_PI / 180.0f;
    float la1 = lat1 * (1e-6f * DEG2RAD);
    float la2 = lat2 * (1e-6f * DEG2RAD);
    float dlo = (lon2 - lon1) * (1e-6f * DEG2RAD);
    float b   = atan2f(sinf(dlo)*cosf(la2),
                       cosf(la1)*sinf(la2) - sinf(la1)*cosf(la2)*cosf(dlo)) * (180.0f / (float)M_PI);
    if (b < 0.0f) b += 360.0f;
    return (int)(b + 0.5f) % 360;
  }

  static const char* bearingCardinal(int deg) {
    static const char* dirs[] = { "N","NE","E","SE","S","SW","W","NW" };
    return dirs[((deg + 22) % 360) / 45];
  }

  static void fmtAge(char* buf, int n, uint32_t lastmod) {
    uint32_t now = rtc_clock.getCurrentTime();
    if (now < lastmod || lastmod == 0) { snprintf(buf, n, "unknown"); return; }
    uint32_t age = now - lastmod;
    if      (age < 60)     snprintf(buf, n, "%us ago",  age);
    else if (age < 3600)   snprintf(buf, n, "%um ago",  age / 60);
    else if (age < 86400)  snprintf(buf, n, "%uh ago",  age / 3600);
    else                   snprintf(buf, n, ">1d ago");
  }

  static void fmtDist(char* buf, int n, float km) {
    if      (km < 1.0f)   snprintf(buf, n, "%dm",   (int)(km * 1000 + 0.5f));
    else if (km < 100.0f) snprintf(buf, n, "%.1fkm", km);
    else                  snprintf(buf, n, "%dkm",  (int)(km + 0.5f));
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
    _own_gps = false;
    _own_lat = _own_lon = 0;

#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc && loc->isValid()) {
      _own_lat = loc->getLatitude();
      _own_lon = loc->getLongitude();
      _own_gps = true;
    }
#endif

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

  int renderDiscover(DisplayDriver& display) {
    static const int D_BOX_H   = 19;
    static const int D_ITEM_H  = 21;
    static const int D_VISIBLE = 2;
    static const int D_START_Y = 11;

    if (_ddetail) {
      // ── full-screen detail for selected node ──────────────────────────────
      const DiscoverResult& r = _dresults[_dsel];
      const char* fullType = (r.type == ADV_TYPE_REPEATER) ? "Repeater" :
                             (r.type == ADV_TYPE_SENSOR)   ? "Sensor"   :
                             (r.type == ADV_TYPE_ROOM)     ? "Room"     : "Node";

      // title bar: node name inverted
      char label[32];
      if (r.name[0]) { strncpy(label, r.name, 31); label[31] = '\0'; }
      else           { snprintf(label, sizeof(label), "[%s]", fullType); }
      char filtered[32];
      display.translateUTF8ToBlocks(filtered, label, sizeof(filtered));
      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(0, 0, display.width(), 10);
      display.setColor(DisplayDriver::DARK);
      display.drawTextEllipsized(2, 1, display.width() - 4, filtered);
      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(0, 10, display.width(), 1);

      char buf[32];
      snprintf(buf, sizeof(buf), "Type: %s", fullType);
      display.setCursor(2, 12); display.print(buf);

      snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)r.rssi);
      display.setCursor(2, 21); display.print(buf);

      snprintf(buf, sizeof(buf), "SNR:  %d dB", (int)(r.snr_x4 / 4));
      display.setCursor(2, 30); display.print(buf);

      snprintf(buf, sizeof(buf), "Rem:  %d dB", (int)(r.remote_snr_x4 / 4));
      display.setCursor(2, 39); display.print(buf);

      display.setCursor(2, 48);
      display.print(r.is_known ? "Status: known" : "Status: new");

      return 5000;
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
    display.fillRect(0, 10, display.width(), 1);

    if (_dresult_count == 0) {
      display.drawTextCentered(display.width() / 2, 32,
        _discovering ? "Waiting for replies..." : "No nodes found");
    } else {
      if (_dsel >= _dresult_count) _dsel = _dresult_count - 1;
      for (int i = 0; i < D_VISIBLE && (_dscroll + i) < _dresult_count; i++) {
        int idx = _dscroll + i;
        bool sel = (idx == _dsel);
        const DiscoverResult& r = _dresults[idx];
        int y = D_START_Y + i * D_ITEM_H;

        const char* typeStr = (r.type == ADV_TYPE_REPEATER) ? "Rpt"  :
                              (r.type == ADV_TYPE_SENSOR)   ? "Snsr" :
                              (r.type == ADV_TYPE_ROOM)     ? "Room" : "?";

        display.setColor(DisplayDriver::LIGHT);
        if (sel) {
          display.fillRect(0, y, display.width(), D_BOX_H);  // fully inverted when selected
          display.setColor(DisplayDriver::DARK);
        } else {
          display.drawRect(0, y, display.width(), D_BOX_H);
          display.fillRect(1, y + 1, display.width() - 2, 8);
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
        snprintf(sig, sizeof(sig), "RSSI:%d SNR:%d", (int)r.rssi, (int)(r.snr_x4 / 4));
        display.drawTextEllipsized(3, y + 10, display.width() - 6, sig);
      }

      display.setColor(DisplayDriver::LIGHT);
      if (_dscroll > 0)
        { display.setCursor(display.width() - 6, D_START_Y + 1); display.print("^"); }
      if (_dscroll + D_VISIBLE < _dresult_count)
        { display.setCursor(display.width() - 6, D_START_Y + D_VISIBLE * D_ITEM_H - 10); display.print("v"); }
    }

    return _discovering ? 200 : 2000;
  }

  bool handleInputDiscover(char c) {
    if (_ddetail) {
      if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { _ddetail = false; return true; }
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
      if (_dsel >= _dscroll + 2) _dscroll = _dsel - 1;
      return true;
    }
    return true;
  }

public:
  NearbyScreen(UITask* task)
    : _task(task), _filter(0), _discover_mode(false), _discovering(false),
      _ddetail(false), _dsel(0) {}

  void enter() {
    _sel = _scroll = 0;
    _detail = false;
    _filter = 0;
    _discover_mode = false;
    _ddetail = false;
    _dsel    = 0;
    _ctx_menu.active = false;
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
      if (!found) _detail = false;
      _detail_refresh_ms = millis();
    }

    // ── detail view ──────────────────────────────────────────────────────────
    if (_detail && _sel < _count) {
      const Entry& e = _entries[_sel];

      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(0, 0, display.width(), 10);
      display.setColor(DisplayDriver::DARK);
      char filtered[32];
      display.translateUTF8ToBlocks(filtered, e.name, sizeof(filtered));
      display.drawTextEllipsized(2, 1, display.width() - 4, filtered);
      display.setColor(DisplayDriver::LIGHT);

      char buf[32];
      snprintf(buf, sizeof(buf), "Lat: %.5f", e.lat_e6 / 1e6);
      display.setCursor(2, 11); display.print(buf);

      snprintf(buf, sizeof(buf), "Lon: %.5f", e.lon_e6 / 1e6);
      display.setCursor(2, 20); display.print(buf);

      if (e.dist_km >= 0.0f) {
        char dist[12];
        fmtDist(dist, sizeof(dist), e.dist_km);
        int az = bearingDeg(_own_lat, _own_lon, e.lat_e6, e.lon_e6);
        snprintf(buf, sizeof(buf), "Dist: %s", dist);
        display.setCursor(2, 29); display.print(buf);
        snprintf(buf, sizeof(buf), "Az: %dd (%s)", az, bearingCardinal(az));
        display.setCursor(2, 38); display.print(buf);
      } else {
        display.setCursor(2, 29); display.print("Dist: no own GPS");
        display.setCursor(2, 38); display.print("Az: unknown");
      }

      snprintf(buf, sizeof(buf), "Type: %s", typeName(e.type));
      display.setCursor(2, 47); display.print(buf);

      char age[16];
      fmtAge(age, sizeof(age), e.lastmod);
      snprintf(buf, sizeof(buf), "Seen: %s", age);
      display.setCursor(2, 56); display.print(buf);

      return 2000;
    }

    // ── list view ────────────────────────────────────────────────────────────
    display.setColor(DisplayDriver::LIGHT);
    char title[22];
    snprintf(title, sizeof(title), "NEARBY[%s]", FILTER_LABELS[_filter]);
    display.drawTextCentered(display.width() / 2, 0, title);
    display.fillRect(0, 10, display.width(), 1);

    if (_count == 0) {
      display.drawTextCentered(display.width() / 2, 28, "No contacts found");
      display.drawTextCentered(display.width() / 2, 40, "[Enter]=Discover");
    } else {
      for (int i = 0; i < VISIBLE && (_scroll + i) < _count; i++) {
        int idx = _scroll + i;
        bool sel = (idx == _sel);
        int y = START_Y + i * ITEM_H;
        const Entry& e = _entries[idx];

        if (sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(0, y - 1, display.width(), ITEM_H - 1);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
        }

        char filt[32];
        display.translateUTF8ToBlocks(filt, e.name, sizeof(filt));
        display.drawTextEllipsized(2, y, DIST_COL - 4, filt);

        display.setColor(sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
        char dist[10];
        if (e.dist_km >= 0.0f) fmtDist(dist, sizeof(dist), e.dist_km);
        else                   strncpy(dist, "?GPS", sizeof(dist));
        display.setCursor(DIST_COL, y);
        display.print(dist);
      }

      display.setColor(DisplayDriver::LIGHT);
      if (_scroll > 0)
        { display.setCursor(display.width() - 6, START_Y); display.print("^"); }
      if (_scroll + VISIBLE < _count)
        { display.setCursor(display.width() - 6, START_Y + (VISIBLE - 1) * ITEM_H); display.print("v"); }
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

    // ── detail view ─────────────────────────────────────────────────────────
    if (_detail) {
      if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { _detail = false; return true; }
      return true;
    }

    // ── context menu ─────────────────────────────────────────────────────────
    if (_ctx_menu.active) {
      auto res = _ctx_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        if (_ctx_menu.selectedIndex() == 0)
          enterDiscoverMode();
        else
          _task->gotoToolsScreen();
      }
      return true;
    }

    // ── list view ────────────────────────────────────────────────────────────
    if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
    if (c == KEY_CONTEXT_MENU) {
      _ctx_menu.begin("Options", 2);
      _ctx_menu.addItem("Discover nearby");
      _ctx_menu.addItem("Back");
      return true;
    }
    if (c == KEY_UP && _sel > 0) {
      _sel--;
      if (_sel < _scroll) _scroll = _sel;
      return true;
    }
    if (c == KEY_DOWN && _sel < _count - 1) {
      _sel++;
      if (_sel >= _scroll + VISIBLE) _scroll = _sel - VISIBLE + 1;
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
