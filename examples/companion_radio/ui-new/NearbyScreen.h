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

  static const int FILTER_COUNT = 5;
  static const char*    FILTER_LABELS[FILTER_COUNT];
  static const uint8_t  FILTER_TYPES[FILTER_COUNT];

  struct Entry {
    char    name[32];
    int32_t lat_e6, lon_e6;
    float   dist_km;
    uint8_t type;
    int     contact_idx;
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

  // scan state
  bool          _scanning;
  unsigned long _scan_started_ms;
  static const unsigned long SCAN_DURATION_MS = 4000UL;

  // context menu (list view)
  PopupMenu _ctx_menu;

  // ping state (detail view)
  enum PingState { PING_IDLE, PING_WAIT, PING_OK, PING_TIMEOUT };
  PingState     _ping_state;
  uint32_t      _ping_expected_ack;
  unsigned long _ping_start_ms;
  unsigned long _ping_rtt_ms;
  bool          _ping_ack_seen;  // true once ACK entry confirmed in table
  static const unsigned long PING_TIMEOUT_MS = 30000UL;

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

  void startScan() {
    the_mesh.advert();
    _scanning = true;
    _scan_started_ms = millis();
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
      if (_filter > 0 && ci.type != FILTER_TYPES[_filter]) continue;

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
    }

    // sort by distance ascending; nodes without GPS (-1) go to the end
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

public:
  NearbyScreen(UITask* task)
    : _task(task), _filter(0), _scanning(false), _ping_state(PING_IDLE) {}

  void enter() {
    _sel = _scroll = 0;
    _detail = false;
    _filter = 0;
    _scanning = false;
    _ctx_menu.active = false;
    _ping_state    = PING_IDLE;
    _ping_ack_seen = false;
    refresh();
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);

    // poll scan timer — only refresh list when not in detail view
    if (_scanning && !_detail && millis() - _scan_started_ms >= SCAN_DURATION_MS) {
      _scanning = false;
      refresh();
    }

    // poll ping ack
    if (_ping_state == PING_WAIT) {
      bool pending = the_mesh.isAckPending(_ping_expected_ack);
      if (pending) {
        _ping_ack_seen = true;
      } else if (_ping_ack_seen) {
        _ping_rtt_ms = millis() - _ping_start_ms;
        _ping_state  = PING_OK;
      } else if (millis() - _ping_start_ms >= PING_TIMEOUT_MS) {
        _ping_state = PING_TIMEOUT;
      }
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
      display.setCursor(2, 13); display.print(buf);

      snprintf(buf, sizeof(buf), "Lon: %.5f", e.lon_e6 / 1e6);
      display.setCursor(2, 22); display.print(buf);

      if (e.dist_km >= 0.0f) {
        char dist[12];
        fmtDist(dist, sizeof(dist), e.dist_km);
        snprintf(buf, sizeof(buf), "Dist:%s %dd", dist, bearingDeg(_own_lat, _own_lon, e.lat_e6, e.lon_e6));
        display.setCursor(2, 31); display.print(buf);
      } else {
        display.setCursor(2, 31); display.print("Dist: no own GPS");
      }

      display.setCursor(2, 40);
      switch (_ping_state) {
        case PING_IDLE:    display.print("[OK]=Ping");                              break;
        case PING_WAIT:    display.print("Pinging...");                             break;
        case PING_OK:      snprintf(buf,sizeof(buf),"RTT:%lums",_ping_rtt_ms);
                           display.print(buf);                                      break;
        case PING_TIMEOUT: display.print("Ping timeout");                           break;
      }

      snprintf(buf, sizeof(buf), "Type:%s", typeName(e.type));
      display.setCursor(2, 49); display.print(buf);

      return (_ping_state == PING_WAIT) ? 100 : 2000;
    }

    // ── list view ────────────────────────────────────────────────────────────
    display.setColor(DisplayDriver::LIGHT);
    if (_scanning) {
      display.drawTextCentered(display.width() / 2, 0, "SCANNING...");
    } else {
      char title[22];
      snprintf(title, sizeof(title), "NEARBY[%s]", FILTER_LABELS[_filter]);
      display.drawTextCentered(display.width() / 2, 0, title);
    }
    display.fillRect(0, 10, display.width(), 1);

    if (_count == 0) {
      display.drawTextCentered(display.width() / 2, 28, _scanning ? "Waiting..." : "No contacts found");
      display.drawTextCentered(display.width() / 2, 40, "[M]=Scan");
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

    // context menu overlay — drawn on top regardless of list state
    if (_ctx_menu.active) {
      _ctx_menu.render(display);
      return 50;
    }

    return _scanning ? 100 : (_count == 0 ? 3000 : 2000);
  }

  bool handleInput(char c) override {
    // ── detail view input ────────────────────────────────────────────────────
    if (_detail) {
      if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
        _detail        = false;
        _ping_state    = PING_IDLE;
        _ping_ack_seen = false;
        return true;
      }
      if (c == KEY_ENTER && _ping_state == PING_IDLE) {
        ContactInfo ci;
        if (the_mesh.getContactByIdx(_entries[_sel].contact_idx, ci)) {
          uint32_t expected_ack, est_timeout;
          int res = the_mesh.sendMessage(ci, rtc_clock.getCurrentTime(), 0, "", expected_ack, est_timeout);
          if (res != MSG_SEND_FAILED && expected_ack != 0) {
            the_mesh.registerExpectedAck(expected_ack);
            _ping_expected_ack = expected_ack;
            _ping_start_ms     = millis();
            _ping_ack_seen     = false;
            _ping_state        = PING_WAIT;
          }
        }
        return true;
      }
      return true;
    }

    // ── list view — context menu ─────────────────────────────────────────────
    if (_ctx_menu.active) {
      auto res = _ctx_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        if (_ctx_menu.selectedIndex() == 0) {
          startScan();
        } else {
          _task->gotoToolsScreen();
        }
      }
      return true;
    }

    // ── list view — normal input ─────────────────────────────────────────────
    if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
    if (c == KEY_CONTEXT_MENU) {
      _ctx_menu.begin("Options", 2);
      _ctx_menu.addItem("Scan for nodes");
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
    if (c == KEY_ENTER && _count > 0) {
      _detail        = true;
      _ping_state    = PING_IDLE;
      _ping_ack_seen = false;
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

const char*   NearbyScreen::FILTER_LABELS[5] = { "ALL", "Comp", "Rpt", "Room", "Snsr" };
const uint8_t NearbyScreen::FILTER_TYPES[5]  = { 0, ADV_TYPE_CHAT, ADV_TYPE_REPEATER, ADV_TYPE_ROOM, ADV_TYPE_SENSOR };
