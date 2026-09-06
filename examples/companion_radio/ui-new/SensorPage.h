#pragma once

// Remote sensors share the home card's header/navigation. Only opening a node
// or explicitly refreshing sends a request; browsing does not poll the radio.
class SensorPage {
  int _selected = 0, _scroll = 0, _row = 0, _lines = 0;
  bool _open = false;
  uint8_t _key[PUB_KEY_SIZE] = {};
  char _name[33] = {};

  int count() const {
    int n = 0;
    ContactInfo contact;
    for (int i = 0; the_mesh.getContactByIdx(i, contact); i++)
      if (contact.type == ADV_TYPE_SENSOR) n++;
    return n;
  }
  bool node(int index, ContactInfo& contact) const {
    for (int i = 0; the_mesh.getContactByIdx(i, contact); i++)
      if (contact.type == ADV_TYPE_SENSOR && index-- == 0) return true;
    return false;
  }

public:
  void close() {
    _open = false;
    _row = 0;
    the_mesh.cancelSensorTelemetry();
  }
  bool handleInput(char c) {
    if (_open) {
      if (c == KEY_CANCEL) { close(); return true; }
      if (c == KEY_ENTER) {
        _row = 0;
        the_mesh.requestSensorTelemetry(_key);
        return true;
      }
      if (c == KEY_UP) { if (_row > 0) _row--; return true; }
      if (c == KEY_DOWN) {
        if (_row + 1 < _lines) _row++;
        return true;
      }
      return false;
    }
    int n = count();
    if (_selected >= n) _selected = n ? n - 1 : 0;
    if (c == KEY_UP || c == KEY_DOWN) {
      if (n) _selected = (_selected + (c == KEY_UP ? n - 1 : 1)) % n;
      return true;
    }
    if (c == KEY_ENTER) {
      ContactInfo contact;
      if (node(_selected, contact)) {
        memcpy(_key, contact.id.pub_key, PUB_KEY_SIZE);
        memcpy(_name, contact.name, 32); _name[32] = 0;
        _open = true; _row = 0;
        the_mesh.requestSensorTelemetry(_key);
      }
      return true;
    }
    return false;
  }
  void render(DisplayDriver& display, int y) {
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    int step = display.lineStep();
    if (_open) {
      display.drawTextEllipsized(0, y, display.width(), _name);
      y += step;
    }
    int visible = (display.height() - y) / step;
    if (visible < 1) visible = 1;
    if (_open) {
      auto state = the_mesh.sensorReplyState();
      const char* status = nullptr;
      if (state == MyMesh::SENSOR_WAITING) status = "Requesting...";
      else if (state == MyMesh::SENSOR_FAILED) status = "No reply. Enter: Retry";
      else if (!the_mesh.sensorTelemetry.rows())
        status = the_mesh.sensorTelemetry.invalid() ? "Unsupported data" : "No telemetry";
      if (status) { display.drawTextEllipsized(0, y, display.width(), status); return; }
      // Wrap long values instead of dropping their last digits. Scroll uses
      // physical lines, including wrapped values, on either display size.
      char text[64];
      _lines = 0;
      int rows = the_mesh.sensorTelemetry.rows();
      for (int i = 0; i < rows + (the_mesh.sensorTelemetry.invalid() ? 1 : 0); i++) {
        if (i == rows) snprintf(text, sizeof(text), "Unsupported data follows");
        else the_mesh.sensorTelemetry.format(i, text, sizeof(text));
        char* start = text;
        while (*start) {
          int n = 1;
          while (start[n]) {
            char saved = start[n + 1]; start[n + 1] = 0;
            bool fits = display.getTextWidth(start) <= display.width();
            start[n + 1] = saved;
            if (!fits) break;
            n++;
          }
          char saved = start[n]; start[n] = 0;
          if (_lines >= _row && _lines < _row + visible)
            display.drawTextLeftAlign(0, y + (_lines - _row) * step, start);
          start[n] = saved;
          start += n;
          _lines++;
        }
      }
    } else {
      int n = count();
      if (!n) { display.drawTextLeftAlign(0, y, "No sensor nodes"); return; }
      if (_selected >= n) _selected = n - 1;
      if (_selected < _scroll) _scroll = _selected;
      if (_selected >= _scroll + visible) _scroll = _selected - visible + 1;
      ContactInfo contact;
      for (int i = 0; i < visible && node(_scroll + i, contact); i++) {
        char name[33]; memcpy(name, contact.name, 32); name[32] = 0;
        display.drawSelectionRow(0, y + i * step - 1, display.width(), step - 1, _scroll + i == _selected);
        display.drawTextEllipsized(2, y + i * step, display.width() - 4, name);
      }
    }
  }
};
