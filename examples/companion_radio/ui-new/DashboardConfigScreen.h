#pragma once
// Configures which data fields appear on the clock home page.
// Included by UITask.cpp after BotScreen.h.

// Field type constants — used here and in UITask.cpp HP_CLOCK render.
static const uint8_t DASH_NONE   = 0;
static const uint8_t DASH_BATT   = 1;
static const uint8_t DASH_TEMP   = 2;
static const uint8_t DASH_HUM    = 3;
static const uint8_t DASH_PRES   = 4;
static const uint8_t DASH_GPS    = 5;
static const uint8_t DASH_ALT    = 6;
static const uint8_t DASH_LUX    = 7;
static const uint8_t DASH_CO2    = 8;
static const uint8_t DASH_NODES  = 9;
static const uint8_t DASH_COUNT  = 10;

class DashboardConfigScreen : public UIScreen {
  UITask*    _task;
  NodePrefs* _prefs;

  static const int FIELD_SLOTS = 3;
  static const int ITEM_H      = 13;
  static const int START_Y     = 13;
  static const int VAL_X       = 62;

  static const char* OPTION_NAMES[DASH_COUNT];

  int _sel;

  void cycle(int slot, int dir) {
    uint8_t& f = _prefs->dashboard_fields[slot];
    f = (uint8_t)((f + DASH_COUNT + dir) % DASH_COUNT);
  }

public:
  DashboardConfigScreen(UITask* task, NodePrefs* prefs) : _task(task), _prefs(prefs) {}

  void enter() { _sel = 0; }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0, "CLOCK FIELDS");
    display.fillRect(0, 10, display.width(), 1);

    static const char* labels[] = { "Field 1", "Field 2", "Field 3" };
    for (int i = 0; i < FIELD_SLOTS; i++) {
      int y = START_Y + i * ITEM_H;
      bool sel = (i == _sel);
      if (sel) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(0, y - 1, display.width(), ITEM_H);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }
      display.setCursor(2, y);
      display.print(labels[i]);
      display.setCursor(VAL_X, y);
      uint8_t f = _prefs->dashboard_fields[i];
      display.print(OPTION_NAMES[f < DASH_COUNT ? f : DASH_NONE]);
      display.setColor(DisplayDriver::LIGHT);
    }
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      the_mesh.savePrefs();
      _task->gotoHomeScreen();
      return true;
    }
    if (c == KEY_UP   && _sel > 0)              { _sel--; return true; }
    if (c == KEY_DOWN && _sel < FIELD_SLOTS - 1){ _sel++; return true; }
    if (c == KEY_LEFT  || c == KEY_PREV)  { cycle(_sel, -1); return true; }
    if (c == KEY_RIGHT || c == KEY_NEXT)  { cycle(_sel,  1); return true; }
    if (c == KEY_ENTER)                   { cycle(_sel,  1); return true; }
    return false;
  }
};

const char* DashboardConfigScreen::OPTION_NAMES[DASH_COUNT] = {
  "None", "Battery", "Temp", "Humidity", "Pressure",
  "GPS", "Altitude", "Lux", "CO2", "Contacts"
};
