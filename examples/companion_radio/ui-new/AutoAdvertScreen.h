#pragma once
// Configures periodic automatic 0-hop advert with GPS position.
// Included by UITask.cpp with the other settings screens.

class AutoAdvertScreen : public UIScreen {
  UITask*    _task;
  NodePrefs* _prefs;
  bool       _dirty;
  uint32_t   _initial_interval;

  static const int OPT_COUNT = 4;
  static const uint32_t OPTS[OPT_COUNT];
  static const char*    OPT_LABELS[OPT_COUNT];

  int currentIdx() const {
    for (int i = 0; i < OPT_COUNT; i++)
      if (OPTS[i] == _prefs->advert_auto_interval_sec) return i;
    return 0;
  }

public:
  AutoAdvertScreen(UITask* task, NodePrefs* prefs) : _task(task), _prefs(prefs) {}

  void onShow() override {
    _dirty = false;
    _initial_interval = _prefs->advert_auto_interval_sec;
    for (int i = 0; i < OPT_COUNT; i++)
      if (OPTS[i] == _prefs->advert_auto_interval_sec) return;
    // Retired short intervals must not keep running while the screen shows Off.
    _prefs->advert_auto_interval_sec = 0;
    _dirty = true;
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    int label_y = display.listStart();
    int bar_y   = label_y + display.lineStep();
    int bar_h   = display.lineStep();
    int tip_y   = bar_y + bar_h + 4;

    display.drawCenteredHeader("AUTO-ADVERT");

    display.setCursor(2, label_y);
    display.print("Interval:");

    int idx = currentIdx();
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, bar_y, display.width(), bar_h);
    display.setColor(DisplayDriver::DARK);
    display.drawTextCentered(display.width() / 2, bar_y + 1, OPT_LABELS[idx]);
    display.setColor(DisplayDriver::LIGHT);

    display.setCursor(2, tip_y);
    display.print("<  > to change");
    display.setCursor(2, tip_y + display.lineStep());
    display.print("[Esc] to save");
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      _dirty = _prefs->advert_auto_interval_sec != _initial_interval;
      _task->savePrefsIfDirty(_dirty);
      _task->gotoHomeScreen();
      return true;
    }
    bool right = keyIsNext(c) || c == KEY_ENTER;
    bool left  = keyIsPrev(c);
    if (right || left) {
      int idx = currentIdx();
      idx = right ? (idx + 1) % OPT_COUNT : (idx + OPT_COUNT - 1) % OPT_COUNT;
      _prefs->advert_auto_interval_sec = OPTS[idx];
      _dirty = true;
      return true;
    }
    return false;
  }
};

const uint32_t AutoAdvertScreen::OPTS[AutoAdvertScreen::OPT_COUNT]       = { 0, 3600, 10800, 21600 };
const char*    AutoAdvertScreen::OPT_LABELS[AutoAdvertScreen::OPT_COUNT] = { "off", "1h", "3h", "6h" };
