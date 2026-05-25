#pragma once
// GPS breadcrumb viewer. Tools › Breadcrumb.
// Phase 1: Summary view only. Phases 2/3 add Map and List views.
// Included by UITask.cpp after Breadcrumb store + ToolsScreen.

#include "../Breadcrumb.h"

class BreadcrumbScreen : public UIScreen {
  UITask*          _task;
  BreadcrumbStore* _store;

public:
  BreadcrumbScreen(UITask* task, BreadcrumbStore* store) : _task(task), _store(store) {}

  void enter() { /* nothing to reset; live trail stays */ }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, 0, "BREADCRUMB");
    display.fillRect(0, display.headerH() - 1, display.width(), display.sepH());

    const int y0   = display.listStart();
    const int step = display.lineStep();

    char buf[28];

    // Status line: ON/OFF and point count.
    snprintf(buf, sizeof(buf), "Status: %s",
             _store->isActive() ? "tracking" : "stopped");
    display.setCursor(2, y0);
    display.print(buf);

    snprintf(buf, sizeof(buf), "Points: %d / %d", _store->count(), BreadcrumbStore::CAPACITY);
    display.setCursor(2, y0 + step);
    display.print(buf);

    // Distance — show m below 1 km, km otherwise.
    uint32_t dist = _store->totalDistanceMeters();
    if (dist < 1000) snprintf(buf, sizeof(buf), "Dist: %lu m", (unsigned long)dist);
    else             snprintf(buf, sizeof(buf), "Dist: %lu.%02lu km",
                              (unsigned long)(dist / 1000),
                              (unsigned long)((dist % 1000) / 10));
    display.setCursor(2, y0 + step * 2);
    display.print(buf);

    // Elapsed — h:mm format.
    uint32_t es = _store->elapsedSeconds();
    snprintf(buf, sizeof(buf), "Time: %lu:%02lu",
             (unsigned long)(es / 3600),
             (unsigned long)((es % 3600) / 60));
    display.setCursor(2, y0 + step * 3);
    display.print(buf);

    // Speed — current km/h.
    snprintf(buf, sizeof(buf), "Speed: %u km/h", (unsigned)_store->currentSpeedKmh());
    display.setCursor(2, y0 + step * 4);
    display.print(buf);

    // Hint at the bottom.
    int hint_y = display.height() - step;
    display.setCursor(2, hint_y);
    display.print(_store->isActive() ? "[Ent] stop  [Esc] back" : "[Ent] start  [Esc] back");

    return _store->isActive() ? 2000 : 5000;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) {
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_ENTER) {
      _store->setActive(!_store->isActive());
      _task->showAlert(_store->isActive() ? "Tracking started" : "Tracking stopped", 800);
      return true;
    }
    return false;
  }
};
