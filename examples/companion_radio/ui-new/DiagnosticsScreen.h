#pragma once
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/DeviceDiag.h>
#include <Arduino.h>
#include "icons.h"
#include "PopupMenu.h"
#include "../MyMesh.h"

extern MyMesh the_mesh;

// Single-screen device diagnostics: packet counts by category (RX/TX), radio
// totals, uptime, heap and stack headroom. Built from Dispatcher's per-type
// counters (Mesh.h/Dispatcher.cpp) — grouped into a handful of categories
// rather than all 16 raw PAYLOAD_TYPE_* values, so it stays readable on a
// 128x64 OLED without needing the full type list. Falls back to scrolling
// (same drawList-style indicator as every other screen) on screens too small
// to show every row at once.
class DiagnosticsScreen : public UIScreen {
  UITask* _task;
  int _scroll = 0;
  PopupMenu _reset_menu;   // Hold Enter → 1-item "Reset counters" action menu (Back dismisses)

  struct Row { const char* label; char value[20]; };
  static const int MAX_ROWS = 14;
  Row _rows[MAX_ROWS];
  int _row_count = 0;

  void addRow(const char* label, const char* value) {
    if (_row_count >= MAX_ROWS) return;
    _rows[_row_count].label = label;
    strncpy(_rows[_row_count].value, value, sizeof(_rows[_row_count].value) - 1);
    _rows[_row_count].value[sizeof(_rows[_row_count].value) - 1] = 0;
    _row_count++;
  }
  // "12345/12345" rather than "rx12345 tx12345" — at 5-digit counts (a busy
  // repeater after weeks of uptime) the longer form collides with the value
  // column on a 128px OLED when paired with a long label like "Ack/Path".
  // The "Total rx/tx" label spells out the rx/tx order once for every row below it.
  void addRxTxRow(const char* label, uint32_t rx, uint32_t tx) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)rx, (unsigned long)tx);
    addRow(label, buf);
  }

  static uint32_t sumByTypes(bool recv, const uint8_t* types, int n) {
    uint32_t total = 0;
    for (int i = 0; i < n; i++)
      total += recv ? the_mesh.getNumRecvByType(types[i]) : the_mesh.getNumSentByType(types[i]);
    return total;
  }

  void buildRows() {
    _row_count = 0;

    uint32_t up_secs = millis() / 1000;
    char buf[20];
    uint32_t d = up_secs / 86400, h = (up_secs % 86400) / 3600, m = (up_secs % 3600) / 60, s = up_secs % 60;
    if (d > 0) snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else       snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    addRow("Uptime", buf);

    static const uint8_t MSG_TYPES[]   = { PAYLOAD_TYPE_TXT_MSG, PAYLOAD_TYPE_GRP_TXT };
    static const uint8_t ADVERT_TYPES[] = { PAYLOAD_TYPE_ADVERT };
    static const uint8_t ROUTE_TYPES[] = { PAYLOAD_TYPE_ACK, PAYLOAD_TYPE_PATH, PAYLOAD_TYPE_TRACE };
    static const uint8_t OTHER_TYPES[] = { PAYLOAD_TYPE_REQ, PAYLOAD_TYPE_RESPONSE, PAYLOAD_TYPE_ANON_REQ,
                                            PAYLOAD_TYPE_GRP_DATA, PAYLOAD_TYPE_MULTIPART, PAYLOAD_TYPE_CONTROL,
                                            PAYLOAD_TYPE_RAW_CUSTOM };

    uint32_t msg_rx = sumByTypes(true, MSG_TYPES, 2), msg_tx = sumByTypes(false, MSG_TYPES, 2);
    uint32_t adv_rx = sumByTypes(true, ADVERT_TYPES, 1), adv_tx = sumByTypes(false, ADVERT_TYPES, 1);
    uint32_t rte_rx = sumByTypes(true, ROUTE_TYPES, 3), rte_tx = sumByTypes(false, ROUTE_TYPES, 3);
    uint32_t oth_rx = sumByTypes(true, OTHER_TYPES, 7), oth_tx = sumByTypes(false, OTHER_TYPES, 7);

    addRxTxRow("Total rx/tx", msg_rx + adv_rx + rte_rx + oth_rx, msg_tx + adv_tx + rte_tx + oth_tx);
    addRxTxRow("Msg",     msg_rx, msg_tx);
    addRxTxRow("Advert",  adv_rx, adv_tx);
    addRxTxRow("Ack/Path", rte_rx, rte_tx);
    addRxTxRow("Other",   oth_rx, oth_tx);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)the_mesh.getNumForwarded());
    addRow("Forwarded", buf);

    uint32_t heap_free, heap_total;
    DeviceDiag::getHeapStats(heap_free, heap_total);
    if (heap_total > 0) snprintf(buf, sizeof(buf), "%lu/%luKB", (unsigned long)(heap_free / 1024), (unsigned long)(heap_total / 1024));
    else                strcpy(buf, "N/A");
    addRow("Heap free", buf);

    uint32_t stack_free = DeviceDiag::getStackFreeBytes();
    snprintf(buf, sizeof(buf), "%luB", (unsigned long)stack_free);
    addRow("Stack free", buf);

    snprintf(buf, sizeof(buf), "%d dBm", (int)radio_driver.getNoiseFloor());
    addRow("Noise floor", buf);
    snprintf(buf, sizeof(buf), "%d/%.1f", (int)radio_driver.getLastRSSI(), radio_driver.getLastSNR());
    addRow("RSSI/SNR", buf);

    snprintf(buf, sizeof(buf), "%d", the_mesh.getPoolFreeCount());
    addRow("Pool free", buf);
    snprintf(buf, sizeof(buf), "%d", the_mesh.getOutboundQueueLen());
    addRow("Queue", buf);

    // Radio error flags since boot/reset, decoded to short tokens (F=queue full,
    // C=CAD timeout, R=RX-start timeout). "OK" when none have fired.
    uint16_t err = the_mesh.getErrFlags();
    if (err == 0) strcpy(buf, "OK");
    else {
      buf[0] = '\0';
      if (err & ERR_EVENT_FULL)            strcat(buf, "F ");
      if (err & ERR_EVENT_CAD_TIMEOUT)     strcat(buf, "C ");
      if (err & ERR_EVENT_STARTRX_TIMEOUT) strcat(buf, "R ");
      int len = strlen(buf);   // drop the trailing space so right-alignment sits flush
      if (len > 0 && buf[len - 1] == ' ') buf[len - 1] = '\0';
    }
    addRow("Errors", buf);
  }

public:
  DiagnosticsScreen(UITask* task) : _task(task) {}

  int render(DisplayDriver& display) override {
    buildRows();
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("DIAGNOSTICS");

    const int item_h  = display.lineStep();
    const int start_y = display.listStart();
    int visible = display.listVisible(item_h);
    if (visible < 1) visible = 1;
    int max_scroll = _row_count - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (_scroll > max_scroll) _scroll = max_scroll;
    if (_scroll < 0) _scroll = 0;

    const int reserve = scrollIndicatorReserve(display, _row_count, visible);
    for (int i = 0; i < visible && (_scroll + i) < _row_count; i++) {
      const Row& r = _rows[_scroll + i];
      int y = start_y + i * item_h;
      display.setCursor(2, y);
      display.print(r.label);
      display.drawTextRightAlign(display.width() - reserve - 2, y, r.value);
    }
    drawScrollIndicator(display, start_y, visible * item_h, _row_count, visible, _scroll);
    display.setColor(DisplayDriver::LIGHT);
    if (_reset_menu.active) _reset_menu.render(display);
    return _reset_menu.active ? 50 : 1000;   // popup wants snappier redraw; stats otherwise refresh once a second
  }

  bool handleInput(char c) override {
    if (_reset_menu.active) {
      auto res = _reset_menu.handleInput(c);   // Back/Cancel dismisses; the only item is "Reset counters"
      if (res == PopupMenu::SELECTED) {
        the_mesh.resetStats();   // zeroes Dispatcher per-type counters + Mesh forward count + err flags
        _task->showAlert("Counters reset", 800);
      }
      return true;
    }
    if (c == KEY_UP)   { if (_scroll > 0) _scroll--; return true; }
    if (c == KEY_DOWN) { _scroll++; return true; }   // clamped to max_scroll in render()
    if (c == KEY_CONTEXT_MENU) {   // Hold Enter — same action-menu gesture as the rest of the UI
      _reset_menu.begin("Diagnostics", 1);
      _reset_menu.addItem("Reset counters");
      return true;
    }
    if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
    return false;
  }
};
