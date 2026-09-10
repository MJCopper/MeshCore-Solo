#pragma once

// Remote sensors share the home card's header/navigation. Only opening a node
// or explicitly refreshing sends a request; browsing does not poll the radio.
class SensorPage {
  enum Phase { TELEMETRY_WAIT, ACL_WAIT, LOGIN_OFFER, PASSWORD, PASSWORD_WAIT,
               AUTH_TELEMETRY_WAIT, ERROR };
  UITask* _task;
  int _selected = 0, _scroll = 0, _row = 0, _lines = 0;
  bool _open = false;
  Phase _phase = TELEMETRY_WAIT;
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
  bool requestTelemetry(bool authenticated) {
    _row = 0;
    bool sent = the_mesh.requestSensorTelemetry(_key);
    _phase = authenticated ? AUTH_TELEMETRY_WAIT : TELEMETRY_WAIT;
    if (!sent) _phase = ERROR;
    return sent;
  }
  bool startLogin(const char* password, bool entered_password) {
    ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
    bool sent = contact && _task->startRoomLogin(
        solo::RoomLoginCoordinator::SENSOR, *contact, password, false);
    if (sent) _phase = entered_password ? PASSWORD_WAIT : ACL_WAIT;
    else {
      _phase = entered_password ? PASSWORD : LOGIN_OFFER;
      _task->showAlert(_task->roomLoginBusy() ? "Login busy" : "Login failed", 1200);
    }
    return sent;
  }

public:
  explicit SensorPage(UITask* task) : _task(task) {}
  bool passwordEditing() const { return _open && _phase == PASSWORD; }
  int renderPassword(DisplayDriver& display) { return _task->keyboard().render(display); }
  void tick() {
    if (!_open || _phase != TELEMETRY_WAIT ||
        the_mesh.sensorReplyState() != MyMesh::SENSOR_FAILED) return;
    startLogin("", false);  // first ask the sensor to authenticate from its ACL
  }
  void onLoginResult(const uint8_t* key, bool success) {
    if (!_open || memcmp(_key, key, PUB_KEY_SIZE) != 0 ||
        (_phase != ACL_WAIT && _phase != PASSWORD_WAIT)) return;
    if (success) {
      requestTelemetry(true);
    } else if (_phase == ACL_WAIT) {
      _phase = LOGIN_OFFER;
    } else {
      _phase = PASSWORD;
      _task->keyboard().begin("", 15);
      _task->showAlert("Login failed", 1200);
    }
  }
  void onLoginTimeout(const uint8_t* key) {
    if (!_open || memcmp(_key, key, 4) != 0) return;
    if (_phase == ACL_WAIT) _phase = LOGIN_OFFER;
    else if (_phase == PASSWORD_WAIT) {
      _phase = PASSWORD;
      _task->keyboard().begin("", 15);
      _task->showAlert("No login reply", 1200);
    }
  }
  void close() {
    if (_open) _task->cancelRoomLogin(solo::RoomLoginCoordinator::SENSOR, _key);
    _open = false;
    _row = 0;
    the_mesh.cancelSensorTelemetry();
  }
  bool handleInput(char c) {
    if (_open) {
      if (c == KEY_CANCEL) { close(); return true; }
      if (_phase == PASSWORD) {
        auto result = _task->keyboard().handleInput(c);
        if (result == KeyboardWidget::DONE) {
          char password[16];
          snprintf(password, sizeof(password), "%s", _task->keyboard().buf);
          startLogin(password, true);
          _task->keyboard().begin("", 15);  // do not retain credentials in the shared editor
        }
        else if (result == KeyboardWidget::CANCELLED) _phase = LOGIN_OFFER;
        return true;
      }
      if (_phase == LOGIN_OFFER && c == KEY_ENTER) {
        _task->keyboard().begin("", 15);
        _task->keyboard().clearPlaceholders();
        _phase = PASSWORD;
        return true;
      }
      if (c == KEY_ENTER) {
        requestTelemetry(false);
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
        requestTelemetry(false);
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
      if (_phase == ACL_WAIT) status = "Checking ACL...";
      else if (_phase == PASSWORD_WAIT) status = "Logging in...";
      else if (_phase == LOGIN_OFFER) status = "Enter: Login";
      else if (_phase == ERROR ||
               (_phase == AUTH_TELEMETRY_WAIT && state == MyMesh::SENSOR_FAILED))
        status = "No reply/access denied";
      else if (state == MyMesh::SENSOR_WAITING) status = "Requesting...";
      else if (state == MyMesh::SENSOR_FAILED) status = "No reply/access denied";
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
        display.drawTextEllipsized(2, y + i * step, display.width() - 4, name,
                                   _scroll + i == _selected);
      }
    }
  }
};
