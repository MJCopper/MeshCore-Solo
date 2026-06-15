#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp just before HomeScreen.

class ToolsScreen : public UIScreen {
  UITask* _task;
  int _sel;
  int _scroll = 0;

  static const int ITEM_COUNT = 6;
  static const char* ITEMS[ITEM_COUNT];

public:
  ToolsScreen(UITask* task) : _task(task), _sel(0) {}

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0, "TOOLS");
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    int item_h  = display.lineStep();
    int start_y = display.listStart();
    int vis     = display.listVisible(item_h);
    if (vis < 1) vis = 1;

    // Keep the selection in view (short OLED panel can't show all 6 items).
    if (_sel < _scroll)            _scroll = _sel;
    if (_sel >= _scroll + vis)     _scroll = _sel - vis + 1;

    int reserve = scrollIndicatorReserve(display, ITEM_COUNT, vis);
    for (int i = 0; i < vis && (_scroll + i) < ITEM_COUNT; i++) {
      int idx = _scroll + i;
      int y   = start_y + i * item_h;
      bool sel = (idx == _sel);
      display.drawSelectionRow(0, y - 1, display.width() - reserve, item_h, sel);
      display.setCursor(2, y);
      display.print(ITEMS[idx]);
    }
    drawScrollIndicator(display, start_y, vis * item_h, ITEM_COUNT, vis, _scroll);
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_UP   && _sel > 0) { _sel--; return true; }
    if (c == KEY_DOWN && _sel < ITEM_COUNT - 1) { _sel++; return true; }
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { _task->gotoHomeScreen(); return true; }
    if (c == KEY_ENTER) {
      if (_sel == 0) { _task->gotoRingtoneEditor();   return true; }
      if (_sel == 1) { _task->gotoBotScreen();        return true; }
      if (_sel == 2) { _task->gotoNearbyScreen();     return true; }
      if (_sel == 3) { _task->gotoAutoAdvertScreen(); return true; }
      if (_sel == 4) { _task->gotoTrailScreen(); return true; }
      if (_sel == 5) { _task->gotoCompassScreen(); return true; }
    }
    return false;
  }
};
const char* ToolsScreen::ITEMS[6] = { "Ringtone Editor", "Auto-Reply Bot", "Nearby Nodes", "Auto-Advert", "Trail", "Compass" };
