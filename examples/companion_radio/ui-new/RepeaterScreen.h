#pragma once
// Tools › Repeater — consolidates the repeater toggle, its flood forwarding
// filters on one screen. Repeater mode always uses the companion's current
// radio parameters, so changing Settings > Radio changes both roles together.
// A dedicated
// screen (vs. the old Settings › Radio sub-items) gives full-width rows, so the
// longer labels no longer collide with the value column. Live forwarding stats
// live separately on Tools › Diagnostics.
//
// Included by UITask.cpp.

#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include "icons.h"
#include "../MyMesh.h"
#include "../solo/RepeaterTiming.h"

extern MyMesh the_mesh;

class RepeaterScreen : public UIScreen {
  UITask* _task;
  bool    _dirty;
  bool    _advanced;
  int     _sel;       // index into the interactive items currently shown
  int     _scroll;    // first visible row (render keeps _sel in view)

  enum Item {
    IT_REPEATER, IT_ADVANCED, IT_RX_DELAY, IT_FLOOD_TX, IT_DIRECT_TX,
    IT_YIELD, IT_SUPPRESS
  };
  uint8_t _items[6];
  int     _item_count;

  void buildItems(NodePrefs* p) {
    _item_count = 0;
    if (_advanced) {
      _items[_item_count++] = IT_RX_DELAY;
      _items[_item_count++] = IT_FLOOD_TX;
      _items[_item_count++] = IT_DIRECT_TX;
      _items[_item_count++] = IT_YIELD;
      _items[_item_count++] = IT_SUPPRESS;
    } else {
      _items[_item_count++] = IT_REPEATER;
      if (p && p->client_repeat) _items[_item_count++] = IT_ADVANCED;
    }
    if (_sel >= _item_count) _sel = _item_count - 1;
    if (_sel < 0) _sel = 0;
  }

  static const char* itemLabel(int item) {
    switch (item) {
      case IT_REPEATER: return "Repeater";
      case IT_ADVANCED: return "Advanced";
      case IT_RX_DELAY: return "RX delay";
      case IT_FLOOD_TX: return "Flood TX";
      case IT_DIRECT_TX:return "Direct TX";
      case IT_YIELD:    return "Yield";
      case IT_SUPPRESS: return "Suppress dup";
    }
    return "";
  }

  void itemValue(int item, NodePrefs* p, char* buf, size_t n) const {
    if (!p) { strncpy(buf, "OFF", n); buf[n-1]=0; return; }
    switch (item) {
      case IT_REPEATER: strncpy(buf, p->client_repeat ? "ON" : "OFF", n); break;
      case IT_ADVANCED: strncpy(buf, ">", n); break;
      case IT_RX_DELAY:
        if (p->repeat_rx_delay_base > 0.0f) snprintf(buf, n, "%.0f", p->repeat_rx_delay_base);
        else strncpy(buf, "OFF", n);
        break;
      case IT_FLOOD_TX: snprintf(buf, n, "%.1f", p->repeat_flood_tx_factor); break;
      case IT_DIRECT_TX: snprintf(buf, n, "%.1f", p->repeat_direct_tx_factor); break;
      case IT_YIELD:
        if (p->repeat_delay_boost > 0) snprintf(buf, n, "x%d", (int)p->repeat_delay_boost + 1);
        else strncpy(buf, "OFF", n);
        break;
      case IT_SUPPRESS: strncpy(buf, p->repeat_suppress_dup ? "ON" : "OFF", n); break;
      default: strncpy(buf, "", n); break;
    }
    buf[n - 1] = '\0';
  }

public:
  RepeaterScreen(UITask* task) : _task(task), _dirty(false), _advanced(false),
                                 _sel(0), _scroll(0), _item_count(1) {}

  void onShow() override {
    _dirty = false; _advanced = false; _sel = 0; _scroll = 0;
  }

  int render(DisplayDriver& display) override {
    NodePrefs* p = _task->getNodePrefs();
    buildItems(p);
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader(_advanced ? "RPT ADVANCED" : "REPEATER");

    // Config only — live forwarding stats live on Tools › Diagnostics.
    drawList(display, _item_count, _sel, _scroll, [&](int row, int y, bool sel, int reserve) {
      int item = _items[row];
      drawRowSelection(display, y, sel, reserve);
      display.setCursor(2, y);
      display.print(itemLabel(item));
      char val[16];
      itemValue(item, p, val, sizeof(val));
      display.drawTextRightAlign(display.width() - reserve - 2, y, val);
      display.setColor(DisplayDriver::LIGHT);
    });
    return 500;
  }

  bool handleInput(char c) override {
    NodePrefs* p = _task->getNodePrefs();

    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      if (_advanced) {
        _advanced = false; _sel = 1; _scroll = 0;
        return true;
      }
      _task->savePrefsIfDirty(_dirty);
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : _item_count - 1; return true; }
    if (c == KEY_DOWN) { _sel = (_sel < _item_count - 1) ? _sel + 1 : 0; return true; }
    if (!p) return false;

    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    bool enter = (c == KEY_ENTER);
    int item = _items[_sel];

    if (item == IT_ADVANCED && enter) {
      _advanced = true; _sel = 0; _scroll = 0;
      buildItems(p);
      return true;
    }

    if (item == IT_REPEATER && (left || right || enter)) {
      p->client_repeat ^= 1;
      _task->applyPowerSave();         // duty-cycle RX is forced off while repeating
      _task->applyApc();               // pin TX power to the ceiling while repeating (APC suppressed)
      buildItems(p);
      _dirty = true;
      return true;
    }
    int dir = right ? 1 : (left ? -1 : 0);
    if (item == IT_RX_DELAY && dir) {
      p->repeat_rx_delay_base += dir;
      if (p->repeat_rx_delay_base < 0.0f) p->repeat_rx_delay_base = 0.0f;
      if (p->repeat_rx_delay_base > solo::RepeaterTiming::MAX_RX_DELAY_BASE)
        p->repeat_rx_delay_base = solo::RepeaterTiming::MAX_RX_DELAY_BASE;
      _dirty = true; return true;
    }
    if (item == IT_FLOOD_TX && dir) {
      p->repeat_flood_tx_factor += dir * 0.1f;
      if (p->repeat_flood_tx_factor < 0.0f) p->repeat_flood_tx_factor = 0.0f;
      if (p->repeat_flood_tx_factor > solo::RepeaterTiming::MAX_TX_FACTOR)
        p->repeat_flood_tx_factor = solo::RepeaterTiming::MAX_TX_FACTOR;
      _dirty = true; return true;
    }
    if (item == IT_DIRECT_TX && dir) {
      p->repeat_direct_tx_factor += dir * 0.1f;
      if (p->repeat_direct_tx_factor < 0.0f) p->repeat_direct_tx_factor = 0.0f;
      if (p->repeat_direct_tx_factor > solo::RepeaterTiming::MAX_TX_FACTOR)
        p->repeat_direct_tx_factor = solo::RepeaterTiming::MAX_TX_FACTOR;
      _dirty = true; return true;
    }
    if (item == IT_YIELD) {
      if (right && p->repeat_delay_boost < 8) { p->repeat_delay_boost++; _dirty = true; return true; }
      if (left  && p->repeat_delay_boost > 0) { p->repeat_delay_boost--; _dirty = true; return true; }
    }
    if (item == IT_SUPPRESS && (left || right || enter)) {
      p->repeat_suppress_dup ^= 1; _dirty = true; return true;
    }
    return false;
  }
};
