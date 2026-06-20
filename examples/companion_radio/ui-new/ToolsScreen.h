#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp just before HomeScreen.

class ToolsScreen : public UIScreen {
  UITask* _task;
  int _sel;
  int _scroll = 0;

  static const int ITEM_COUNT = 8;
  static const char* ITEMS[ITEM_COUNT];

public:
  ToolsScreen(UITask* task) : _task(task), _sel(0) {}

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("TOOLS");

    drawList(display, ITEM_COUNT, _sel, _scroll, [&](int idx, int y, bool sel, int reserve) {
      display.drawSelectionRow(0, y - 1, display.width() - reserve, display.lineStep() - 1, sel);
      display.setCursor(2, y);
      display.print(ITEMS[idx]);
    });
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : ITEM_COUNT - 1; return true; }
    if (c == KEY_DOWN) { _sel = (_sel < ITEM_COUNT - 1) ? _sel + 1 : 0; return true; }
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { _task->gotoHomeScreen(); return true; }
    if (c == KEY_ENTER) {
      if (_sel == 0) { _task->gotoRingtoneEditor();   return true; }
      if (_sel == 1) { _task->gotoBotScreen();        return true; }
      if (_sel == 2) { _task->gotoNearbyScreen();     return true; }
      if (_sel == 3) { _task->gotoAutoAdvertScreen(); return true; }
      if (_sel == 4) { _task->gotoTrailScreen(); return true; }
      if (_sel == 5) { _task->gotoCompassScreen(); return true; }
      if (_sel == 6) { _task->gotoDiagnosticsScreen(); return true; }
      if (_sel == 7) { _task->gotoRepeaterScreen(); return true; }
    }
    return false;
  }
};
const char* ToolsScreen::ITEMS[8] = { "Ringtone Editor", "Auto-Reply Bot", "Nearby Nodes", "Auto-Advert", "Trail", "Compass", "Diagnostics", "Repeater" };
