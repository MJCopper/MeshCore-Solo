#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "../MsgExpand.h"
#include "target.h"
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];
  char _plus_ver[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    // extract plus version: v1.15-plus.1.4-SHA -> "1.4"
    _plus_ver[0] = '\0';
    const char *plus = strstr(ver, "plus.");
    if (plus) {
      plus += 5;  // skip "plus."
      const char *end = strchr(plus, '-');
      int plen = end ? end - plus : strlen(plus);
      if (plen >= (int)sizeof(_plus_ver)) plen = sizeof(_plus_ver) - 1;
      memcpy(_plus_ver, plus, plen);
      _plus_ver[plen] = '\0';
    }

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    const int lh = display.getLineHeight();
    const int step = display.lineStep();

    // meshcore logo
    display.setColor(DisplayDriver::LIGHT);
    int logoWidth = 128;
    int logo_y = 3;
    display.drawXbm((display.width() - logoWidth) / 2, logo_y, meshcore_logo, logoWidth, 13);

    // version info at sz2
    int ver_y = logo_y + 13 + 2;
    display.setTextSize(2);
    int lh2 = display.getLineHeight();
    display.drawTextCentered(display.width()/2, ver_y, _version_info);

    // build date at sz1, below sz2 version
    int date_y = ver_y + lh2 + 2;
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, date_y, FIRMWARE_BUILD_DATE);

#ifdef FIRMWARE_PLUS_BUILD
    int plus_y = date_y + step;
    display.fillRect(0, plus_y - 1, display.width(), lh + 2);
    display.setColor(DisplayDriver::DARK);
    char plus_label[24];
    if (_plus_ver[0])
      snprintf(plus_label, sizeof(plus_label), "Plus %s for Wio", _plus_ver);
    else
      snprintf(plus_label, sizeof(plus_label), "Plus for Wio");
    display.drawTextCentered(display.width()/2, plus_y, plus_label);
    display.setColor(DisplayDriver::LIGHT);
#endif

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

static const int QUICK_MSGS_MAX = 10;

// Bit positions for NodePrefs::home_pages_mask.
// Bit=1 means page is shown. 0 in the field means ALL visible (default/unset).
static const uint16_t HP_CLOCK     = 1 << 0;
static const uint16_t HP_RECENT    = 1 << 1;
static const uint16_t HP_RADIO     = 1 << 2;
static const uint16_t HP_BLUETOOTH = 1 << 3;
static const uint16_t HP_ADVERT    = 1 << 4;
static const uint16_t HP_GPS       = 1 << 5;
static const uint16_t HP_SENSORS   = 1 << 6;
static const uint16_t HP_TOOLS     = 1 << 7;
static const uint16_t HP_SHUTDOWN  = 1 << 8;
static const uint16_t HP_ALL       = 0x01FF;

#include "KeyboardWidget.h"
#include "FullscreenMsgView.h"
#include "SensorPlaceholders.h"
#include "SettingsScreen.h"
#include "QuickMsgScreen.h"

// ── Custom screens (separate files to ease upstream merges) ───────────────────
#include "RingtoneEditorScreen.h"
#include "BotScreen.h"
#include "NearbyScreen.h"
#include "DashboardConfigScreen.h"
#include "AutoAdvertScreen.h"
#include "ToolsScreen.h"

// ── HomeScreen ────────────────────────────────────────────────────────────────
class HomeScreen : public UIScreen {
  enum HomePage {
    CLOCK,
    RECENT,
    RADIO,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    SETTINGS,
    TOOLS,
    QUICK_MSG,
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];

  int pageBit(int page) const {
    if (page == CLOCK)     return 0;
    if (page == RECENT)    return 1;
    if (page == RADIO)     return 2;
    if (page == BLUETOOTH) return 3;
    if (page == ADVERT)    return 4;
#if ENV_INCLUDE_GPS == 1
    if (page == GPS)       return 5;
#endif
#if UI_SENSORS_PAGE == 1
    if (page == SENSORS)   return 6;
#endif
    if (page == TOOLS)     return 7;
    if (page == SHUTDOWN)  return 8;
    return -1;  // SETTINGS, QUICK_MSG always visible
  }

  // Maps page_order bit-index (0-10) back to the HomePage enum value for this build.
  // Returns -1 if the page is not compiled in.
  int bitToPage(int bit) const {
    switch (bit) {
      case 0: return CLOCK;
      case 1: return RECENT;
      case 2: return RADIO;
      case 3: return BLUETOOTH;
      case 4: return ADVERT;
      case 5:
#if ENV_INCLUDE_GPS == 1
        return GPS;
#else
        return -1;
#endif
      case 6:
#if UI_SENSORS_PAGE == 1
        return SENSORS;
#else
        return -1;
#endif
      case 7: return TOOLS;
      case 8: return SHUTDOWN;
      case 9: return SETTINGS;
      case 10: return QUICK_MSG;
      default: return -1;
    }
  }

  bool isPageVisible(int page) const {
    int bit = pageBit(page);
    if (bit < 0) return true;
    uint16_t mask = (_node_prefs && _node_prefs->home_pages_mask) ? _node_prefs->home_pages_mask : HP_ALL;
    return (mask >> bit) & 1;
  }

  // Build ordered list of all visible pages, respecting page_order when set.
  // Returns count; out[] receives HomePage enum values.
  int buildVisibleOrder(int* out) const {
    int n = 0;
    bool custom = _node_prefs && _node_prefs->page_order[0] >= 1 && _node_prefs->page_order[0] <= 11;
    if (custom) {
      for (int i = 0; i < 11; i++) {
        uint8_t v = _node_prefs->page_order[i];
        if (v < 1 || v > 11) break;
        int pg = bitToPage(v - 1);
        if (pg >= 0 && pg < (int)Count && isPageVisible(pg)) out[n++] = pg;
      }
    } else {
      for (int pg = 0; pg < (int)Count; pg++)
        if (isPageVisible(pg)) out[n++] = pg;
    }
    return n;
  }

  int navPage(int from, int dir) const {
    int order[11]; int n = buildVisibleOrder(order);
    if (n == 0) return from;
    int cur = 0;
    for (int i = 0; i < n; i++) if (order[i] == from) { cur = i; break; }
    return order[((cur + dir) % n + n) % n];
  }

  int renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3200
#endif
    // LiPo discharge curve: voltage (mV) → raw capacity (%)
    static const struct { uint16_t mv; uint8_t pct; } CURVE[] = {
      {3200,  0}, {3300,  3}, {3400,  8}, {3500, 15},
      {3600, 25}, {3650, 33}, {3700, 45}, {3750, 58},
      {3800, 68}, {3900, 77}, {4000, 86}, {4100, 93}, {4200, 100}
    };
    static const int CURVE_LEN = sizeof(CURVE) / sizeof(CURVE[0]);

    auto curveAt = [&](int mv) -> int {
      if (mv <= (int)CURVE[0].mv) return CURVE[0].pct;
      if (mv >= (int)CURVE[CURVE_LEN-1].mv) return CURVE[CURVE_LEN-1].pct;
      for (int i = 1; i < CURVE_LEN; i++) {
        if (mv <= (int)CURVE[i].mv) {
          int span_mv  = CURVE[i].mv  - CURVE[i-1].mv;
          int span_pct = CURVE[i].pct - CURVE[i-1].pct;
          return CURVE[i-1].pct + (mv - (int)CURVE[i-1].mv) * span_pct / span_mv;
        }
      }
      return 100;
    };

    int low_mv = (_node_prefs && _node_prefs->low_batt_mv > 0)
                   ? (int)_node_prefs->low_batt_mv : BATT_MIN_MILLIVOLTS;
    int raw_pct = curveAt((int)batteryMilliVolts);
    int low_pct = curveAt(low_mv);
    // rescale so low_mv = 0% and 4200mV = 100%
    int pct = (low_pct >= 100) ? 0
              : (raw_pct - low_pct) * 100 / (100 - low_pct);
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    uint8_t mode = (_node_prefs && _node_prefs->batt_display_mode < 3)
                     ? _node_prefs->batt_display_mode : 0;

    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    const int lh      = display.getLineHeight();
    const int cw      = display.getCharWidth();
    const int ind     = cw + 2;    // single-char indicator width
    const int ind_h   = display.isLemonFont() ? lh - 2 : lh;
    const int ind_gap = display.isLandscape() ? 3 : 1;  // gap between indicator boxes

    int battLeftX;
    if (mode == 1) {  // percent
      char buf[6];
      sprintf(buf, "%d%%", pct);
      battLeftX = display.width() - display.getTextWidth(buf) - 1;
      display.setCursor(battLeftX, 0);
      display.print(buf);
    } else if (mode == 2) {  // voltage
      char buf[8];
      sprintf(buf, "%u.%02uV", batteryMilliVolts / 1000, (batteryMilliVolts % 1000) / 10);
      battLeftX = display.width() - display.getTextWidth(buf) - 1;
      display.setCursor(battLeftX, 0);
      display.print(buf);
    } else {  // icon — scales with lh
      const int iconH = lh;
      const int iconW = lh * 2;
      const int bm = display.isLandscape() ? 3 : 2;  // inner margin: 3px on landscape e-ink, 2px on OLED/portrait
      battLeftX = display.width() - iconW - 3;
      display.drawRect(battLeftX, 0, iconW, iconH);
      display.fillRect(battLeftX + iconW, iconH / 4, 2, iconH / 2);
      int fillW = (pct * (iconW - 2 * bm)) / 100;
      display.fillRect(battLeftX + bm, bm, fillW, iconH - 2 * bm);
    }

#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(DisplayDriver::LIGHT);
      int mx = battLeftX - ind - ind_gap;
      display.fillRect(mx, 0, ind, ind_h);
      display.setColor(DisplayDriver::DARK);
      display.setCursor(mx + 1, 0);
      display.print("M");
      display.setColor(DisplayDriver::LIGHT);
      battLeftX = mx;
    }
#endif

    // BT connection indicator (left of muted/battery icons)
    int leftmostX = battLeftX;
    if (_task->isSerialEnabled()) {
      int btX = battLeftX - ind - ind_gap;
      if (_task->hasConnection()) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(btX, 0, ind, ind_h);
        display.setColor(DisplayDriver::DARK);
        display.setCursor(btX + 1, 0);
        display.print("B");
        display.setColor(DisplayDriver::LIGHT);
      } else {
        display.setCursor(btX + 1, 0);
        display.print("b");
      }
      leftmostX = btX - ind_gap;

      // "A" indicator — left of BT; blinks on OLED, always shown on e-ink
      if (_node_prefs && _node_prefs->advert_auto_interval_sec > 0) {
        int aX = leftmostX - ind;
#ifdef EINK_DISPLAY_MODEL
        bool show_a = true;
#else
        bool show_a = (millis() % 4000) < 2000;
#endif
        if (show_a) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(aX, 0, ind, ind_h);
          display.setColor(DisplayDriver::DARK);
          display.setCursor(aX + 1, 0);
          display.print("A");
          display.setColor(DisplayDriver::LIGHT);
        }
        leftmostX = aX - 1;
      }
    }
    return leftmostX;
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;
  
  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0), 
       _shutdown_init(false), sensors_lpp(200) {  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    display.setTextSize(1);
    const int lh      = display.getLineHeight();  // line height at sz1
    const int step    = display.lineStep();        // lh + 2
    const int dots_y  = lh + 4;                   // page-dot row: just below header
    const int content_y = dots_y + 6;             // first content row (6px gap keeps dots visible)

    // node name + battery — hidden on CLOCK page (full screen used for dashboard)
    if (_page != CLOCK) {
      display.setColor(DisplayDriver::LIGHT);
      char filtered_name[sizeof(_node_prefs->node_name)];
      display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
      int rightEdge = renderBatteryIndicator(display, _task->getBattMilliVolts());
      display.setColor(DisplayDriver::LIGHT);
      display.drawTextEllipsized(0, 0, rightEdge - 2, filtered_name);
    }

    // ensure current page is visible (e.g. after settings change)
    if (!isPageVisible(_page)) _page = navPage(_page, +1);

    // curr page indicator — hidden on CLOCK page (full screen used for dashboard)
    if (_page != CLOCK) {
      int order[11]; int n = buildVisibleOrder(order);
      int curr_vis = 0;
      for (int i = 0; i < n; i++) if (order[i] == _page) { curr_vis = i; break; }
      int x = display.width() / 2 - 5 * (n - 1);
      for (int i = 0; i < n; i++) {
        int ds = display.isLandscape() ? 2 : 1;
        if (i == curr_vis) display.fillRect(x-ds, dots_y-ds, 2*ds+1, 2*ds+1);
        else               display.fillRect(x-ds+1, dots_y-ds+1, 2*ds-1, 2*ds-1);
        x += 10;
      }
    }

    if (_page == HomePage::CLOCK) {
      uint32_t unix_ts = _rtc->getCurrentTime();
      if (unix_ts < 1000000000UL) {
        display.setColor(DisplayDriver::LIGHT);
        display.setTextSize(1);
        int mid_y = display.height() / 2 - step;
        display.drawTextCentered(display.width() / 2, mid_y, "! No time sync");
        display.drawTextCentered(display.width() / 2, mid_y + step, "Enable GPS or");
        display.drawTextCentered(display.width() / 2, mid_y + step * 2, "connect app");
      } else {
        int8_t tz = _node_prefs ? _node_prefs->tz_offset_hours : 0;
        unix_ts += (int32_t)tz * 3600;
        time_t t = (time_t)unix_ts;
        struct tm* ti = gmtime(&t);

        char buf[24];
        display.setColor(DisplayDriver::LIGHT);
        display.setTextSize(2);
#ifdef EINK_DISPLAY_MODEL
        bool show_sec = false;
#else
        bool show_sec = !_node_prefs || !_node_prefs->clock_hide_seconds;
#endif
        bool h12 = _node_prefs && _node_prefs->clock_12h;
        if (h12) {
          int h = ti->tm_hour % 12; if (h == 0) h = 12;
          const char* ap = ti->tm_hour < 12 ? "AM" : "PM";
          if (show_sec) sprintf(buf, "%d:%02d:%02d%s", h, ti->tm_min, ti->tm_sec, ap);
          else          sprintf(buf, "%d:%02d %s", h, ti->tm_min, ap);
        } else {
          if (show_sec) sprintf(buf, "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
          else          sprintf(buf, "%02d:%02d", ti->tm_hour, ti->tm_min);
        }
        int lh2 = display.getLineHeight();  // sz2 height: 16 (OLED) or 32 (landscape)
        display.drawTextCentered(display.width() / 2, 0, buf);

        display.setTextSize(1);
        static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        sprintf(buf, "%s %d %s %d", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon], 1900 + ti->tm_year);
        int date_y = lh2 + 2;
        display.drawTextCentered(display.width() / 2, date_y, buf);

        int sep_y  = date_y + lh + 1;
        int dash0  = sep_y + display.sepH() + 2;
        display.fillRect(0, sep_y, display.width(), display.sepH());

        // dashboard data fields
        if (_node_prefs) {
          refresh_sensors();
          const int FIELD_Y[3] = { dash0, dash0 + step, dash0 + step * 2 };
          for (int fi = 0; fi < 3; fi++) {
            uint8_t field = _node_prefs->dashboard_fields[fi];
            if (field == DASH_NONE) continue;

            char label[10], val[20];
            label[0] = '\0';
            val[0] = '\0';

            if (field == DASH_BATT) {
              strcpy(label, "Batt");
              uint16_t mv = _task->getBattMilliVolts();
              if (mv > 0) snprintf(val, sizeof(val), "%u.%02uV", mv/1000, (mv%1000)/10);
              else strcpy(val, "--");
            } else if (field == DASH_GPS) {
              strcpy(label, "GPS");
#if ENV_INCLUDE_GPS == 1
              LocationProvider* loc = sensors.getLocationProvider();
              if (loc && loc->isValid())
                snprintf(val, sizeof(val), "%.3f %.3f",
                  loc->getLatitude()/1000000.0f, loc->getLongitude()/1000000.0f);
              else
                strcpy(val, "no fix");
#else
              strcpy(val, "--");
#endif
            } else if (field == DASH_NODES) {
              strcpy(label, "Nodes");
              snprintf(val, sizeof(val), "%d", the_mesh.getNumContacts());
            } else if (field == DASH_MSGS) {
              strcpy(label, "Msgs");
              int unread = _task->getDMUnreadTotal() + _task->getChannelUnreadCount() + _task->getRoomUnreadCount();
              snprintf(val, sizeof(val), "%d", unread);
            } else {
              uint8_t lpp_type = 0;
              switch (field) {
                case DASH_TEMP: strcpy(label, "Temp"); lpp_type = LPP_TEMPERATURE;        break;
                case DASH_HUM:  strcpy(label, "Hum");  lpp_type = LPP_RELATIVE_HUMIDITY;  break;
                case DASH_PRES: strcpy(label, "Pres"); lpp_type = LPP_BAROMETRIC_PRESSURE; break;
                case DASH_ALT:  strcpy(label, "Alt");  lpp_type = LPP_ALTITUDE;           break;
                case DASH_LUX:  strcpy(label, "Lux");  lpp_type = LPP_LUMINOSITY;         break;
                case DASH_CO2:  strcpy(label, "CO2");  lpp_type = LPP_CONCENTRATION;      break;
              }
              if (lpp_type) {
                LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());
                uint8_t ch, type;
                while (r.readHeader(ch, type)) {
                  if (type == lpp_type) {
                    float v;
                    switch (lpp_type) {
                      case LPP_TEMPERATURE:         r.readTemperature(v);      snprintf(val, sizeof(val), "%.1f\xf8""C", v); break;
                      case LPP_RELATIVE_HUMIDITY:   r.readRelativeHumidity(v); snprintf(val, sizeof(val), "%.0f%%", v);      break;
                      case LPP_BAROMETRIC_PRESSURE: r.readPressure(v);         snprintf(val, sizeof(val), "%.0fhPa", v);     break;
                      case LPP_ALTITUDE:            r.readAltitude(v);         snprintf(val, sizeof(val), "%.0fm", v);       break;
                      case LPP_LUMINOSITY:          r.readLuminosity(v);       snprintf(val, sizeof(val), "%.0flux", v);     break;
                      case LPP_CONCENTRATION:       r.readConcentration(v);    snprintf(val, sizeof(val), "%.0fppm", v);     break;
                    }
                    break;
                  }
                  r.skipData(type);
                }
              }
              if (!val[0]) strcpy(val, "--");
            }

            if (val[0] && label[0]) {
              display.setColor(DisplayDriver::LIGHT);
              display.setCursor(0, FIELD_Y[fi]);
              display.print(label);
              int vw = display.getTextWidth(val);
              display.setCursor(display.width() - vw - 1, FIELD_Y[fi]);
              display.print(val);
            }
          }
        }
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::LIGHT);
      int y = content_y;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += step) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }
        
        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;
        
        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::LIGHT);
      // freq / sf
      display.setCursor(0, content_y);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, content_y + step);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power, noise floor
      display.setCursor(0, content_y + step * 2);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, content_y + step * 3);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      display.drawXbm((display.width() - 32) / 2, content_y,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off, 32, 32);
      const int text_y = content_y + 32 + 3;
      const bool waiting_for_pair = _task->isSerialEnabled() && !_task->hasConnection() && the_mesh.getBLEPin() != 0;
      if (waiting_for_pair && !display.isLandscape()) {
        char pin_buf[16];
        snprintf(pin_buf, sizeof(pin_buf), "PIN: %d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, text_y, pin_buf);
      } else if (waiting_for_pair) {
        char pin_buf[16];
        snprintf(pin_buf, sizeof(pin_buf), "PIN: %d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, text_y, pin_buf);
        display.drawTextCentered(display.width() / 2, text_y + step, "toggle: " PRESS_LABEL);
      } else {
        display.drawTextCentered(display.width() / 2, text_y, "toggle: " PRESS_LABEL);
      }
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::LIGHT);
      display.drawXbm((display.width() - 32) / 2, content_y, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, content_y + 32 + 3, "advert: " PRESS_LABEL);
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = content_y;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y += step;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = content_y;
      refresh_sensors();

      uint8_t avail_types[16];
      int avail_count = _sensors ? _sensors->getAvailableLPPTypes(avail_types, 16) : 0;
      bool need_scroll = avail_count > UI_RECENT_LIST_SIZE;
      int offset = need_scroll ? (sensors_scroll_offset % avail_count) : 0;
      int show_n = need_scroll ? UI_RECENT_LIST_SIZE : avail_count;

      for (int i = 0; i < show_n; i++) {
        uint8_t target = avail_types[(offset + i) % avail_count];

        // scan LPP buffer for this type
        LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());
        uint8_t ch, type;
        bool found = false;
        char buf[22] = "--";
        while (r.readHeader(ch, type)) {
          if (type == target) {
            float v, v2, v3;
            switch (type) {
              case LPP_GPS:
                r.readGPS(v, v2, v3);
                if (v != 0 || v2 != 0) snprintf(buf, sizeof(buf), "%.4f %.4f", v, v2);
                break;
              case LPP_VOLTAGE:    r.readVoltage(v);          snprintf(buf, sizeof(buf), "%.2fV", v); break;
              case LPP_CURRENT:    r.readCurrent(v);          snprintf(buf, sizeof(buf), "%.3fA", v); break;
              case LPP_POWER:      r.readPower(v);            snprintf(buf, sizeof(buf), "%.1fW", v); break;
              case LPP_TEMPERATURE:r.readTemperature(v);      snprintf(buf, sizeof(buf), "%.1f\xf8""C", v); break;
              case LPP_RELATIVE_HUMIDITY: r.readRelativeHumidity(v); snprintf(buf, sizeof(buf), "%.0f%%", v); break;
              case LPP_BAROMETRIC_PRESSURE: r.readPressure(v); snprintf(buf, sizeof(buf), "%.1fhPa", v); break;
              case LPP_ALTITUDE:   r.readAltitude(v);         snprintf(buf, sizeof(buf), "%.0fm", v); break;
              case LPP_LUMINOSITY: r.readLuminosity(v);       snprintf(buf, sizeof(buf), "%.0flux", v); break;
              case LPP_PERCENTAGE: r.readPercentage(v);       snprintf(buf, sizeof(buf), "%.0f%%", v); break;
              case LPP_DISTANCE:   r.readDistance(v);         snprintf(buf, sizeof(buf), "%.2fm", v); break;
              case LPP_CONCENTRATION: r.readConcentration(v); snprintf(buf, sizeof(buf), "%.0fppm", v); break;
              default:             r.skipData(type); continue;
            }
            found = true;
            break;
          }
          r.skipData(type);
        }
        (void)found;

        static const struct { uint8_t type; const char* name; } TYPE_NAMES[] = {
          { LPP_VOLTAGE,            "voltage"  },
          { LPP_GPS,                "gps"      },
          { LPP_TEMPERATURE,        "temp"     },
          { LPP_RELATIVE_HUMIDITY,  "humidity" },
          { LPP_BAROMETRIC_PRESSURE,"pressure" },
          { LPP_ALTITUDE,           "altitude" },
          { LPP_CURRENT,            "current"  },
          { LPP_POWER,              "power"    },
          { LPP_LUMINOSITY,         "light"    },
          { LPP_PERCENTAGE,         "moisture" },
          { LPP_DISTANCE,           "distance" },
          { LPP_CONCENTRATION,      "CO2"      },
        };
        const char* name = "sensor";
        for (auto& tn : TYPE_NAMES) { if (tn.type == target) { name = tn.name; break; } }

        display.setCursor(0, y);
        display.print(name);
        display.setCursor(display.width() - display.getTextWidth(buf) - 1, y);
        display.print(buf);
        y += step;
      }
      if (need_scroll) sensors_scroll_offset = (sensors_scroll_offset + 1) % avail_count;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SETTINGS) {
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, content_y, "Settings");
      display.drawTextCentered(display.width() / 2, content_y + step * 2, PRESS_LABEL " to open");
    } else if (_page == HomePage::TOOLS) {
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, content_y, "Tools");
      display.drawTextCentered(display.width() / 2, content_y + step * 2, PRESS_LABEL " to open");
    } else if (_page == HomePage::QUICK_MSG) {
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, content_y, "Messages");
      int total_unread = _task->getDMUnreadTotal() + _task->getChannelUnreadCount() + _task->getRoomUnreadCount();
      if (total_unread > 0) {
        char badge[20];
        snprintf(badge, sizeof(badge), "%d unread", total_unread);
        display.drawTextCentered(display.width() / 2, content_y + step, badge);
      }
      display.drawTextCentered(display.width() / 2, content_y + step * 2, PRESS_LABEL " to open");
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, content_y + step, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, content_y, power_icon, 32, 32);
        const int text_y = content_y + 32 + 3;
        const int lh1 = display.getLineHeight();
        if (text_y + lh1 <= display.height()) {
          char hib_hint[32];
          snprintf(hib_hint, sizeof(hib_hint), "hibernate: %s", PRESS_LABEL);
          if (display.getTextWidth(hib_hint) < display.width()) {
            display.drawTextCentered(display.width() / 2, text_y, hib_hint);
          } else {
            display.drawTextCentered(display.width() / 2, text_y, "hibernate:");
            if (text_y + step + lh1 <= display.height())
              display.drawTextCentered(display.width() / 2, text_y + step, PRESS_LABEL);
          }
        }
      }
    }
    bool auto_adv = _node_prefs && _node_prefs->advert_auto_interval_sec > 0;
#ifdef EINK_DISPLAY_MODEL
    if (_page == HomePage::CLOCK) return 30000;  // no seconds on e-ink; refresh every 30s
    return 30000;  // e-ink: limit base polling; new messages still force immediate refresh via notify()
#else
    if (_page == HomePage::CLOCK) {
      bool show_sec = !_node_prefs || !_node_prefs->clock_hide_seconds;
      return auto_adv ? 1000 : (show_sec ? 1000 : 60000);
    }
    return auto_adv ? 1000 : 5000;
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = navPage(_page, -1);
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = navPage(_page, +1);
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isSerialEnabled()) {  // toggle Bluetooth on/off
        _task->disableSerial();
      } else {
        _task->enableSerial();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SETTINGS) {
      _task->gotoSettingsScreen();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::TOOLS) {
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::QUICK_MSG) {
      _task->gotoQuickMsgScreen();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::CLOCK) {
      _task->gotoDashboardConfig();
      return true;
    }
    return false;
  }
};


void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;
  uint32_t aoff = autoOffMillis();
  _auto_off = millis() + (aoff > 0 ? aoff : AUTO_OFF_MILLIS);

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.setVolume(_node_prefs->buzzer_volume);
  buzzer.begin();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  // Set default quick message if slot 0 is empty (first boot)
  if (_node_prefs && _node_prefs->custom_msgs[0][0] == '\0') {
    strncpy(_node_prefs->custom_msgs[0], "OK", sizeof(_node_prefs->custom_msgs[0]) - 1);
  }

  ui_started_at = millis();
  _alert_expiry = 0;
  _batt_mv = AbstractUITask::getBattMilliVolts();  // seed EMA with first reading

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  settings = new SettingsScreen(this);
  quick_msg = new QuickMsgScreen(this);
  tools_screen  = new ToolsScreen(this);
  ringtone_edit = new RingtoneEditorScreen(this, node_prefs);
  bot_screen    = new BotScreen(this, node_prefs);
  nearby_screen = new NearbyScreen(this);
  dashboard_config = new DashboardConfigScreen(this, node_prefs);
  auto_advert_screen = new AutoAdvertScreen(this, node_prefs);
  applyBrightness();
  applyFont();
  applyRotation();
  setCurrScreen(splash);
}

void UITask::gotoSettingsScreen() {
  ((SettingsScreen*)settings)->markClean();
  setCurrScreen(settings);
}

void UITask::gotoToolsScreen() {
  setCurrScreen(tools_screen);
}

void UITask::gotoRingtoneEditor(int slot) {
  ((RingtoneEditorScreen*)ringtone_edit)->enter(slot);
  setCurrScreen(ringtone_edit);
}

void UITask::gotoBotScreen() {
  ((BotScreen*)bot_screen)->enter();
  setCurrScreen(bot_screen);
}

void UITask::gotoNearbyScreen() {
  ((NearbyScreen*)nearby_screen)->enter();
  setCurrScreen(nearby_screen);
}

void UITask::gotoDashboardConfig() {
  ((DashboardConfigScreen*)dashboard_config)->enter();
  setCurrScreen(dashboard_config);
}

void UITask::gotoAutoAdvertScreen() {
  ((AutoAdvertScreen*)auto_advert_screen)->enter();
  setCurrScreen(auto_advert_screen);
}

void UITask::playMelody(const char* melody) {
#ifdef PIN_BUZZER
  buzzer.playForced(melody);
#endif
}

void UITask::stopMelody() {
#ifdef PIN_BUZZER
  buzzer.stop();
#endif
}

bool UITask::isMelodyPlaying() {
#ifdef PIN_BUZZER
  return buzzer.isPlaying();
#else
  return false;
#endif
}

void UITask::gotoQuickMsgScreen() {
  ((QuickMsgScreen*)quick_msg)->reset();
  setCurrScreen(quick_msg);
}

void UITask::addChannelMsg(uint8_t channel_idx, const char* text) {
  _last_notif_ch_idx = (int)channel_idx;
  ((QuickMsgScreen*)quick_msg)->addChannelMsg(channel_idx, text);
}

int UITask::getChannelUnreadCount() const {
  return ((QuickMsgScreen*)quick_msg)->getTotalChannelUnread();
}

void UITask::addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text) {
  ((QuickMsgScreen*)quick_msg)->addDMMsg(pub_key, outgoing, text);
}

int UITask::getDMUnreadTotal() const {
  int total = 0;
  for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++)
    total += _dm_unread_table[i].count;
  return total;
}

void UITask::showAlert(const char* text, int duration_millis) {
  snprintf(_alert, sizeof(_alert), "%s", text);
  _alert_expiry = millis() + duration_millis;
}

static void buildMelodyFromPrefs(const NodePrefs* p, int slot, char* buf, int size) {
  static const uint16_t BPM_OPTS[]  = { 60, 90, 120, 150, 180 };
  static const uint8_t  DUR_VALS[]  = { 4, 8, 16, 32 };
  static const char     PITCHES[]   = { 'p', 'c', 'd', 'e', 'f', 'g', 'a', 'b' };
  const uint8_t* notes  = (slot == 2) ? p->ringtone2_notes  : p->ringtone_notes;
  uint8_t        len    = (slot == 2) ? p->ringtone2_len     : p->ringtone_len;
  uint8_t        bpm_i  = (slot == 2) ? p->ringtone2_bpm_idx : p->ringtone_bpm_idx;
  if (len == 0) { buf[0] = '\0'; return; }
  uint16_t bpm = BPM_OPTS[bpm_i < 5 ? bpm_i : 2];
  int pos = snprintf(buf, size, "Ring:d=8,o=5,b=%u:", bpm);
  for (int i = 0; i < len && pos < size - 8; i++) {
    if (i > 0 && pos < size - 1) buf[pos++] = ',';
    uint8_t pitch   = notes[i] & 0x07;
    uint8_t octave  = ((notes[i] >> 3) & 0x03) + 4;
    uint8_t dur_val = DUR_VALS[(notes[i] >> 5) & 0x03];
    if (pitch == 0) pos += snprintf(buf + pos, size - pos, "%dp", dur_val);
    else            pos += snprintf(buf + pos, size - pos, "%d%c%d", dur_val, PITCHES[pitch], octave);
  }
  if (pos < size) buf[pos] = '\0';
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage: {
    bool play = false;
    bool force = false;
    if (_last_notif_dm_valid && _node_prefs) {
      uint8_t state = 0;
      for (int i = 0; i < NodePrefs::DM_NOTIF_TABLE_MAX; i++) {
        if (_node_prefs->dm_notif[i].state &&
            memcmp(_node_prefs->dm_notif[i].prefix, _last_notif_dm_prefix, 4) == 0) {
          state = _node_prefs->dm_notif[i].state; break;
        }
      }
      if (state == 2) { play = true; force = true; }   // force-on
      else if (state == 1) { /* muted */ }
      else { play = !buzzer.isQuiet(); }               // default: follow global
    } else {
      play = !buzzer.isQuiet();
    }
    _last_notif_dm_valid = false;
    if (play) {
      int slot = _node_prefs ? (int)_node_prefs->notif_melody_dm : 0;
      if (_node_prefs) {
        for (int i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++)
          if (_node_prefs->dm_melody[i].slot &&
              memcmp(_node_prefs->dm_melody[i].prefix, _last_notif_dm_prefix, 4) == 0)
            { slot = _node_prefs->dm_melody[i].slot; break; }
      }
      bool custom_played = false;
      if (slot > 0 && _node_prefs) {
        buildMelodyFromPrefs(_node_prefs, slot, _notif_mel_buf, sizeof(_notif_mel_buf));
        if (_notif_mel_buf[0]) {
          if (force) buzzer.playForced(_notif_mel_buf); else buzzer.play(_notif_mel_buf);
          custom_played = true;
        }
      }
      if (!custom_played) {
        if (force) buzzer.playForced("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
        else       buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
      }
    }
    break;
  }
  case UIEventType::channelMessage: {
    bool play = false;
    bool force = false;
    if (_last_notif_ch_idx >= 0 && _last_notif_ch_idx < 64 && _node_prefs) {
      uint64_t mask = 1ULL << _last_notif_ch_idx;
      if (_node_prefs->ch_notif_override & mask) {
        if (!(_node_prefs->ch_notif_muted & mask)) { play = true; force = true; }
      } else {
        play = !buzzer.isQuiet();
      }
    } else {
      play = !buzzer.isQuiet();
    }
    if (play) {
      int slot = _node_prefs ? (int)_node_prefs->notif_melody_ch : 0;
      if (_last_notif_ch_idx >= 0 && _last_notif_ch_idx < 64 && _node_prefs) {
        uint64_t mask = 1ULL << _last_notif_ch_idx;
        if (_node_prefs->ch_notif_melody_set & mask)
          slot = (_node_prefs->ch_notif_melody_2 & mask) ? 2 : 1;
      }
      bool custom_played = false;
      if (slot > 0 && _node_prefs) {
        buildMelodyFromPrefs(_node_prefs, slot, _notif_mel_buf, sizeof(_notif_mel_buf));
        if (_notif_mel_buf[0]) {
          if (force) buzzer.playForced(_notif_mel_buf); else buzzer.play(_notif_mel_buf);
          custom_played = true;
        }
      }
      if (!custom_played) {
        if (force) buzzer.playForced("kerplop:d=16,o=6,b=120:32g#,32c#");
        else       buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
      }
    }
    _last_notif_ch_idx = -1;
    break;
  }
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    _room_unread = 0;
    memset(_dm_unread_table, 0, sizeof(_dm_unread_table));
    ((QuickMsgScreen*)quick_msg)->clearAllChannelUnread();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, uint8_t contact_type, const uint8_t* pub_key) {
  _msgcount = msgcount;
  if (contact_type == ADV_TYPE_ROOM && _room_unread < _msgcount) _room_unread++;
  if (contact_type == ADV_TYPE_CHAT && pub_key != nullptr) {
    memcpy(_last_notif_dm_prefix, pub_key, 4);
    _last_notif_dm_valid = true;
    int slot = -1, empty_slot = -1;
    for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++) {
      if (_dm_unread_table[i].count > 0 && memcmp(_dm_unread_table[i].prefix, pub_key, 4) == 0) { slot = i; break; }
      if (empty_slot < 0 && _dm_unread_table[i].count == 0) empty_slot = i;
    }
    if (slot >= 0) {
      if (_dm_unread_table[slot].count < 99) _dm_unread_table[slot].count++;
    } else if (empty_slot >= 0) {
      memcpy(_dm_unread_table[empty_slot].prefix, pub_key, 4);
      _dm_unread_table[empty_slot].count = 1;
    }
  }

  char alert_buf[80];
  snprintf(alert_buf, sizeof(alert_buf), "Msg: %.20s", from_name);
  showAlert(alert_buf, 3000);

  if (_display != NULL && !_locked) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
      uint32_t aoff = autoOffMillis();
      if (aoff > 0) _auto_off = millis() + aoff;
      _next_refresh = 100;
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){
  the_mesh.saveRTCTime();

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - buzzer_timer) < 2500)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

static void formatDashVal(uint8_t field, char* val, int val_len, uint16_t batt_mv,
                          CayenneLPP* lpp = nullptr) {
  val[0] = '\0';
  switch (field) {
    case DASH_NONE: return;
    case DASH_BATT:
      if (batt_mv > 0) snprintf(val, val_len, "%u.%02uV", batt_mv/1000, (batt_mv%1000)/10);
      else              strcpy(val, "--");
      return;
    case DASH_NODES:
      snprintf(val, val_len, "%d nodes", the_mesh.getNumContacts());
      return;
#if ENV_INCLUDE_GPS == 1
    case DASH_GPS: {
      LocationProvider* loc = sensors.getLocationProvider();
      if (loc && loc->isValid())
        snprintf(val, val_len, "%.2f %.2f",
                 loc->getLatitude()/1000000.0f, loc->getLongitude()/1000000.0f);
      else strcpy(val, "no fix");
      return;
    }
#endif
    default: break;
  }
  // LPP sensor fields
  uint8_t lpp_type = 0;
  switch (field) {
    case DASH_TEMP: lpp_type = LPP_TEMPERATURE;         break;
    case DASH_HUM:  lpp_type = LPP_RELATIVE_HUMIDITY;   break;
    case DASH_PRES: lpp_type = LPP_BAROMETRIC_PRESSURE; break;
    case DASH_ALT:  lpp_type = LPP_ALTITUDE;            break;
    case DASH_LUX:  lpp_type = LPP_LUMINOSITY;          break;
    case DASH_CO2:  lpp_type = LPP_CONCENTRATION;       break;
  }
  if (lpp_type) {
    CayenneLPP local_lpp(200);
    if (!lpp) { local_lpp.reset(); sensors.querySensors(0xFF, local_lpp); lpp = &local_lpp; }
    LPPReader r(lpp->getBuffer(), lpp->getSize());
    uint8_t ch, type;
    while (r.readHeader(ch, type)) {
      if (type == lpp_type) {
        float v;
        switch (lpp_type) {
          case LPP_TEMPERATURE:         r.readTemperature(v);      snprintf(val, val_len, "%.1f\xf8""C", v); return;
          case LPP_RELATIVE_HUMIDITY:   r.readRelativeHumidity(v); snprintf(val, val_len, "%.0f%%", v);      return;
          case LPP_BAROMETRIC_PRESSURE: r.readPressure(v);         snprintf(val, val_len, "%.0fhPa", v);     return;
          case LPP_ALTITUDE:            r.readAltitude(v);         snprintf(val, val_len, "%.0fm", v);       return;
          case LPP_LUMINOSITY:          r.readLuminosity(v);       snprintf(val, val_len, "%.0flux", v);     return;
          case LPP_CONCENTRATION:       r.readConcentration(v);    snprintf(val, val_len, "%.0fppm", v);     return;
        }
      }
      r.skipData(type);
    }
    strcpy(val, "--");
  }
}

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  uint8_t joy_rot = _node_prefs ? _node_prefs->joystick_rotation : JOYSTICK_ROTATION;
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    if (back_btn.isPressed()) {
      // Enter clicked while Back is held — lock/unlock sequence
      if (_display && !_display->isOn()) {
        _display->turnOn();  // turn on display so hints are visible
      }
      _lock_wake_until = millis() + 5000;  // keep display on during sequence
      if (millis() - _lock_seq_ms > 3000) _lock_seq_count = 0;  // timeout reset
      _lock_seq_count++;
      _lock_seq_ms = millis();
      _next_refresh = 0;  // update hint immediately on each press
      if (_lock_seq_count >= 3) {
        _lock_seq_count = 0;
        _lock_seq_used = true;  // suppress Back release click
        _locked = !_locked;
        if (_locked) {
          _lock_wake_until = millis() + 2000;
        } else {
          if (_display && !_display->isOn()) _display->turnOn();
          uint32_t aoff = autoOffMillis();
          if (aoff > 0) _auto_off = millis() + aoff;
        }
      }
      // eat the Enter — don't pass to curr
    } else {
      c = checkDisplayOn(KEY_ENTER);
    }
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
#if UI_HAS_JOYSTICK_UPDOWN
  ev = joystick_up.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(rotateJoystickKey(KEY_UP, joy_rot));
  }
  ev = joystick_down.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(rotateJoystickKey(KEY_DOWN, joy_rot));
  }
#endif
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(rotateJoystickKey(KEY_LEFT, joy_rot));
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(rotateJoystickKey(KEY_LEFT, joy_rot));
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(rotateJoystickKey(KEY_RIGHT, joy_rot));
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(rotateJoystickKey(KEY_RIGHT, joy_rot));
  }
  if (_lock_seq_used && millis() - _lock_seq_ms > 5000) {
    _lock_seq_used = false;  // safety reset if Back release event was missed
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    if (_lock_seq_count > 0 || _lock_seq_used) {
      // Back released mid-sequence or after completing it — cancel/suppress
      _lock_seq_count = 0;
      _lock_seq_used = false;
    } else {
      c = checkDisplayOn(KEY_CANCEL);
    }
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    if (!_locked) c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0) {
    if (!_locked && curr) {
      curr->handleInput(c);
      { uint32_t aoff = autoOffMillis(); if (aoff > 0) _auto_off = millis() + aoff; }  // extend auto-off timer
      _next_refresh = 100;  // trigger refresh
    } else if (_locked) {
      // Locked: eat all keys — wake window is set only when display first turns on
      _next_refresh = 0;
    }
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (_node_prefs && _node_prefs->buzzer_auto) {
    bool should_quiet = hasConnection();
    if (buzzer.isQuiet() != should_quiet) {
      buzzer.quiet(should_quiet);
      _next_refresh = 0;
    }
  }
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (_locked && millis() > _lock_wake_until) {
      _display->turnOff();
    } else if (_locked && millis() >= _next_refresh) {
      _display->startFrame();
      // Lock screen: clock + unlock hint popup
      uint32_t unix_ts = rtc_clock.getCurrentTime();
      _display->setColor(DisplayDriver::LIGHT);
      _display->setTextSize(1);
      const int lk_lh   = _display->getLineHeight();
      const int lk_step = _display->lineStep();
      if (unix_ts < 1000000000UL) {
        _display->drawTextCentered(_display->width() / 2, _display->height() / 2 - lk_step, "No time sync");
      } else {
        int8_t tz = _node_prefs ? _node_prefs->tz_offset_hours : 0;
        unix_ts += (int32_t)tz * 3600;
        time_t t = (time_t)unix_ts;
        struct tm* ti = gmtime(&t);
        char buf[12];
        _display->setTextSize(2);
        const int lh2   = _display->getLineHeight();  // sz2 line height
        const int clk_y = 2;
        if (_node_prefs && _node_prefs->clock_12h) {
          int h = ti->tm_hour % 12; if (h == 0) h = 12;
          sprintf(buf, "%d:%02d %s", h, ti->tm_min, ti->tm_hour < 12 ? "AM" : "PM");
        } else {
          sprintf(buf, "%02d:%02d", ti->tm_hour, ti->tm_min);
        }
        _display->drawTextCentered(_display->width() / 2, clk_y, buf);
        _display->setTextSize(1);
        static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        sprintf(buf, "%s %d %s", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon]);
        int date_y = clk_y + lh2 + 2;
        _display->drawTextCentered(_display->width() / 2, date_y, buf);

        // Two sensor values side by side (dashboard_fields[0] and [1])
        if (_node_prefs) {
          char v0[20] = "", v1[20] = "";
          CayenneLPP shared_lpp(200);
          CayenneLPP* lpp_ptr = nullptr;
          uint8_t f0 = _node_prefs->dashboard_fields[0], f1 = _node_prefs->dashboard_fields[1];
          auto isLPP = [](uint8_t f) {
            return f==DASH_TEMP||f==DASH_HUM||f==DASH_PRES||f==DASH_ALT||f==DASH_LUX||f==DASH_CO2;
          };
          if (isLPP(f0) || isLPP(f1)) {
            shared_lpp.reset(); sensors.querySensors(0xFF, shared_lpp); lpp_ptr = &shared_lpp;
          }
          formatDashVal(f0, v0, sizeof(v0), _batt_mv, lpp_ptr);
          formatDashVal(f1, v1, sizeof(v1), _batt_mv, lpp_ptr);
          if (v0[0] || v1[0]) {
            int sv_y = date_y + lk_step;
            _display->setColor(DisplayDriver::LIGHT);
            if (v0[0] && v1[0]) {
              _display->setCursor(0, sv_y);
              _display->print(v0);
              int vw = _display->getTextWidth(v1);
              _display->setCursor(_display->width() - vw, sv_y);
              _display->print(v1);
            } else {
              const char* sv = v0[0] ? v0 : v1;
              _display->drawTextCentered(_display->width() / 2, sv_y, sv);
            }
          }
        }
      }
      // Hint popup at bottom (like alert style)
      _display->setTextSize(1);
      const char* hint = _lock_seq_count == 0 ? "Hold Back + 3xEnter" :
                         _lock_seq_count == 1 ? "Enter x2 more..."   : "Enter x1 more...";
      int p = 3;
      int hy = _display->height() - lk_lh - p * 2;
      int hw = _display->getTextWidth(hint);
      int hx = (_display->width() - hw) / 2;
      _display->setColor(DisplayDriver::LIGHT);
      _display->fillRect(hx - p, hy - p, hw + p*2, lk_lh + p*2);
      _display->setColor(DisplayDriver::DARK);
      _display->setCursor(hx, hy);
      _display->print(hint);
      _display->endFrame();
#ifdef EINK_DISPLAY_MODEL
      _next_refresh = millis() + 30000;
#else
      _next_refresh = millis() + 1000;
#endif
    } else if (!_locked && millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry && curr == home) {  // render alert only on home screen
        _display->setTextSize(1);
        int lh    = _display->getLineHeight();
        int pad   = 3;
        int box_h = lh + pad * 2;
        int box_w = _display->width() - 8;
        int box_x = 4;
        int box_y = (_display->height() - box_h) / 2;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(box_x, box_y, box_w, box_h);
        _display->setColor(DisplayDriver::LIGHT);
        _display->drawRect(box_x, box_y, box_w, box_h);
        _display->drawTextCentered(_display->width() / 2, box_y + pad, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
    if (!_locked && autoOffMillis() > 0 && millis() > _auto_off) {
      _display->turnOff();
#ifdef PIN_LED
      digitalWrite(PIN_LED, LOW);  // turn off status LED with display to save power
#endif
      if (_node_prefs && _node_prefs->auto_lock) {
        _locked = true;
        _lock_wake_until = 0;
      }
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

  if (millis() > next_batt_chck) {
    uint16_t raw = AbstractUITask::getBattMilliVolts();
    if (raw > 0) {
      // EMA filter: alpha=0.2 (80% old, 20% new) — smooths ADC noise from uneven load
      _batt_mv = (_batt_mv == 0) ? raw : (uint16_t)((_batt_mv * 4u + raw) / 5u);
    }
    uint16_t low_mv = _node_prefs ? _node_prefs->low_batt_mv : 0;
    if (low_mv > 0 && _batt_mv > 0 && _batt_mv < low_mv) {
      if (_display != NULL) {
        _display->startFrame();
        _display->setTextSize(1);
        _display->setColor(DisplayDriver::LIGHT);
        int mid = _display->height() / 2;
        int step = _display->lineStep();
        _display->drawTextCentered(_display->width() / 2, mid - step, "Low Battery");
        _display->drawTextCentered(_display->width() / 2, mid, "Shutting down");
        _display->endFrame();
        delay(2000);
      }
      shutdown();
    }
    next_batt_chck = millis() + 8000;
  }
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();
#ifdef PIN_LED
      digitalWrite(PIN_LED, LOW);  // ensure LED is off when waking display (userLedHandler takes over)
#endif
      if (_locked) {
        _lock_wake_until = millis() + 5000;
        _next_refresh = 0;
        return 0;  // eat the waking key press
      }
      c = 0;
    }
    if (!_locked) {
      uint32_t aoff = autoOffMillis();
      if (aoff > 0) _auto_off = millis() + aoff;  // extend auto-off timer
    }
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    return 0;
  }
  if (c == KEY_ENTER) return KEY_CONTEXT_MENU;
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  } 
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::applyTxPower() {
  if (_node_prefs == NULL) return;
  radio_set_tx_power(_node_prefs->tx_power_dbm);
}

void UITask::applyGPSInterval() {
  if (_node_prefs == NULL) return;
  char buf[12];
  sprintf(buf, "%u", _node_prefs->gps_interval);
  sensors.setSettingValue("gps_interval", buf);
}

void UITask::applyBrightness() {
  if (_display != NULL && _node_prefs != NULL) {
    _display->setBrightness(_node_prefs->display_brightness);
  }
}

void UITask::applyFont() {
  if (_display != NULL && _node_prefs != NULL) {
    _display->setLemonFont(_node_prefs->use_lemon_font != 0);
    _next_refresh = 0;
  }
}

void UITask::applyRotation() {
  if (_display != NULL && _node_prefs != NULL) {
    _display->setDisplayRotation(_node_prefs->display_rotation);
    _next_refresh = 0;
  }
}

void UITask::setBrightnessLevel(uint8_t level) {
  if (_node_prefs == NULL) return;
  if (level > 4) level = 4;
  _node_prefs->display_brightness = level;
  applyBrightness();
  _next_refresh = 0;
}

void UITask::setBuzzerVolumeLevel(uint8_t level) {
#ifdef PIN_BUZZER
  if (_node_prefs == NULL) return;
  if (level > 4) level = 4;
  _node_prefs->buzzer_volume = level;
  buzzer.setVolume(level);
  if (level > 0) buzzer.playForced("Vol:d=16,o=6,b=120:c");
  _next_refresh = 0;
#endif
}

void UITask::toggleBuzzer() {
  #ifdef PIN_BUZZER
    if (_node_prefs) _node_prefs->buzzer_auto = 0;  // exit auto mode
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    if (_node_prefs) _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;
  #endif
}

int UITask::getBuzzerMode() {
#ifdef PIN_BUZZER
  if (_node_prefs && _node_prefs->buzzer_auto) return 2;
  return buzzer.isQuiet() ? 1 : 0;
#else
  return 1;
#endif
}

void UITask::cycleBuzzerMode() {
#ifdef PIN_BUZZER
  if (!_node_prefs) return;
  int mode = getBuzzerMode();
  mode = (mode + 1) % 3;  // ON(0) → OFF(1) → Auto(2) → ON
  _node_prefs->buzzer_auto = (mode == 2) ? 1 : 0;
  if (mode == 0) { buzzer.quiet(false); _node_prefs->buzzer_quiet = 0; notify(UIEventType::ack); }
  if (mode == 1) { buzzer.quiet(true);  _node_prefs->buzzer_quiet = 1; }
  if (mode == 2) { buzzer.quiet(hasConnection()); }
  static const char* labels[] = { "Buzzer: ON", "Buzzer: OFF", "Buzzer: Auto" };
  showAlert(labels[mode], 800);
  _next_refresh = 0;
#endif
}
