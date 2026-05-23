#pragma once
#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>

// Generic scrollable popup menu overlay.
// Caller owns item string lifetimes — addItem() stores const char* pointers.
// Navigation wraps around: up from first item goes to last, and vice versa.
struct PopupMenu {
  static const int PM_MAX_ITEMS = 16;
  static const int PM_BX        = 12;
  static const int PM_BY        = 10;
  static const int PM_BW        = 104;
  static const int PM_ITEM_H    = 10;

  const char* _items[PM_MAX_ITEMS];
  int         _count;
  int         _sel;
  int         _scroll;
  int         _visible;
  bool        active;
  const char* _title;

  enum Result { NONE, SELECTED, CANCELLED };

  PopupMenu() : _count(0), _sel(0), _scroll(0), _visible(3), active(false), _title(nullptr) {}

  void begin(const char* title, int visible = 3) {
    _count = 0; _sel = 0; _scroll = 0;
    _visible = visible; active = true; _title = title;
  }

  void addItem(const char* item) {
    if (_count < PM_MAX_ITEMS) _items[_count++] = item;
  }

  int render(DisplayDriver& display) {
    int vis = (_count < _visible) ? _count : _visible;
    int bh  = 12 + vis * PM_ITEM_H;

    display.setColor(DisplayDriver::DARK);
    display.fillRect(PM_BX, PM_BY, PM_BW, bh);
    display.setColor(DisplayDriver::LIGHT);
    display.drawRect(PM_BX, PM_BY, PM_BW, bh);
    display.setCursor(PM_BX + 4, PM_BY + 1);
    display.print(_title);
    display.fillRect(PM_BX, PM_BY + 10, PM_BW, 1);

    if (_scroll > 0)
      { display.setCursor(PM_BX + PM_BW - 7, PM_BY + 1); display.print("^"); }
    if (_scroll + vis < _count)
      { display.setCursor(PM_BX + PM_BW - 7, PM_BY + 2 + vis * PM_ITEM_H); display.print("v"); }

    for (int i = 0; i < vis && (_scroll + i) < _count; i++) {
      int idx = _scroll + i;
      int py  = PM_BY + 12 + i * PM_ITEM_H;
      if (idx == _sel) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(PM_BX + 1, py - 1, PM_BW - 2, PM_ITEM_H);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }
      display.setCursor(PM_BX + 4, py);
      display.print(_items[idx]);
    }
    display.setColor(DisplayDriver::LIGHT);
    return 50;
  }

  Result handleInput(char c) {
    if (_count == 0) { active = false; return CANCELLED; }
    if (c == KEY_UP) {
      _sel = (_sel > 0) ? _sel - 1 : _count - 1;
      if (_sel < _scroll)                                 _scroll = _sel;
      else if (_sel == _count - 1 && _count > _visible)  _scroll = _count - _visible;
      return NONE;
    }
    if (c == KEY_DOWN) {
      _sel = (_sel < _count - 1) ? _sel + 1 : 0;
      if (_sel >= _scroll + _visible)  _scroll = _sel - _visible + 1;
      else if (_sel == 0)              _scroll = 0;
      return NONE;
    }
    if (c == KEY_ENTER)                           { active = false; return SELECTED;  }
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) { active = false; return CANCELLED; }
    return NONE;
  }

  int selectedIndex() const { return _sel; }
};
