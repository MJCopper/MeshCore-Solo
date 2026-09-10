#pragma once

// Node List > Hold Enter > Admin. UI only: command definitions and transport
// ownership live in solo/AdminCommands and solo/AdminSession respectively.
#include "FullscreenMsgView.h"
#include "RadioParamsEditor.h"
#include "../solo/AdminCommands.h"
#include "ConsoleEditorSupport.h"
#include <helpers/ClientACL.h>

class AdminScreen : public UIScreen {
  UITask* _task;
  enum Phase { LOGIN, ROOT, LIST, WAIT, EDIT, REPLY };
  Phase _phase = LOGIN;
  ContactInfo _target;
  bool _login_waiting = false;
  bool _authenticated = false;
  bool _fetching = false;
  bool _console = false;
  bool _text_edit = false;
  solo::admin::Group _group = solo::admin::STATUS;
  const solo::admin::Field* _field = nullptr;
  int _sel = 0, _scroll = 0;
  uint8_t _items[solo::admin::FIELD_COUNT];
  int _count = 0;
  uint32_t _deadline = 0;
  char _original[161]{};
  char _value[161]{};
  char _reply[201]{};
  const char* _reply_title = "Reply";
  PopupMenu _confirm;
  FullscreenMsgView _view;
  DigitEditor _frequency;
  float _number = 0, _freq = 0, _bw = 0;
  uint8_t _sf = 0, _cr = 0;

  KeyboardWidget& kb() { return _task->keyboard(); }
  bool allowed() const { return !_task->isChildModeLocked(); }
  solo::admin::TargetKind targetKind() const {
    if (_target.type == ADV_TYPE_ROOM) return solo::admin::TARGET_ROOM;
    if (_target.type == ADV_TYPE_SENSOR) return solo::admin::TARGET_SENSOR;
    return solo::admin::TARGET_REPEATER;
  }

  void root() { _phase = ROOT; _sel = _scroll = 0; _field = nullptr; }
  void leave() {
    closeSession();
    _task->returnFromAdmin();
  }
  void showReply(const char* title, const char* text) {
    _reply_title = title;
    StrHelper::strncpy(_reply, text, sizeof(_reply));
    _view.begin();
    _phase = REPLY;
  }
  void login() {
    ContactInfo* current = the_mesh.lookupContactByPubKey(_target.id.pub_key, PUB_KEY_SIZE);
    bool sent = current && allowed() && _task->startRoomLogin(
        solo::RoomLoginCoordinator::ADMIN, *current, kb().buf, false);
    kb().begin("", 15);  // no credential remains in the shared editor
    _login_waiting = sent;
    if (!sent) _task->showAlert("Login busy/failed", 1200);
  }
  void send(const char* command, bool fetching) {
    uint32_t timeout;
    if (!allowed() || !_authenticated ||
        !the_mesh.sendAdminCommand(_target, command, timeout)) {
      showReply("Not sent", "Busy or send failed");
      return;
    }
    _fetching = fetching;
    _deadline = millis() + timeout + 4000;
    _phase = WAIT;
  }
  void buildList() {
    _count = 0;
    for (int i = 0; i < solo::admin::FIELD_COUNT; i++)
      if (solo::admin::FIELDS[i].group == _group) _items[_count++] = i;
    _phase = LIST;
    _sel = _scroll = 0;
  }
  void openText(const char* value, int max_len) {
    kb().begin(value, max_len);
    kb().clearPlaceholders();  // CLI values are literal, not chat templates
    if (_console) consoleeditor::enable(kb());
    _text_edit = true;
    _phase = EDIT;
  }
  bool beginEdit(const char* value) {
    using namespace solo::admin;
    StrHelper::strncpy(_value, value, sizeof(_value));
    // Strip line endings, but preserve meaningful spaces in text values.
    size_t n = strlen(_value);
    while (n && (_value[n - 1] == '\r' || _value[n - 1] == '\n')) _value[--n] = 0;
    _text_edit = false;
    if (_field->kind == TEXT) {
      if (n > (size_t)_field->max) return false;
      openText(_value, (int)_field->max);
    } else if (radioTuple(_field->kind)) {
      if (!parseRadio(_value, _freq, _bw, _sf, _cr)) return false;
      formatRadio(_value, sizeof(_value), _freq, _bw, _sf, _cr);
      _frequency.begin(_freq, 150, 2500, 4, 3);
    } else if (_field->kind == TOGGLE) {
      if (strcmp(_value, "on") && strcmp(_value, "off")) return false;
      _number = strcmp(_value, "on") == 0 ? 1 : 0;
    } else {
      if (!number(_value, _field->min, _field->max, _number)) return false;
      snprintf(_value, sizeof(_value), _field->step < 1 ? "%.1f" : "%.0f", _number);
    }
    strcpy(_original, _value);
    _phase = EDIT;
    return true;
  }
  void formatValue() {
    using namespace solo::admin;
    if (_text_edit) StrHelper::strncpy(_value, kb().buf, sizeof(_value));
    else if (radioTuple(_field->kind)) {
      if (_field->kind == FREQUENCY) _freq = _frequency.value;
      formatRadio(_value, sizeof(_value), _freq, _bw, _sf, _cr);
    } else if (_field->kind == TOGGLE) strcpy(_value, _number ? "on" : "off");
    else snprintf(_value, sizeof(_value), _field->step < 1 ? "%.1f" : "%.0f", _number);
  }
  void reviewEdit() {
    formatValue();
    if (!_console && !strcmp(_original, _value)) { _phase = LIST; return; }
    if (_console && !_value[0]) { root(); return; }
    _confirm.begin(_console ? "Send command?" :
        (_group == solo::admin::RADIO ? "May lose contact" : "Apply change?"));
    _confirm.addItem("Cancel");
    _confirm.addItem(_console ? "Send" : "Apply");
    _confirm.addItem("Discard");
  }
  void activate() {
    _field = &solo::admin::FIELDS[_items[_sel]];
    _console = false;
    if (_field->kind == solo::admin::ACTION) {
      _confirm.begin(_field->label);
      _confirm.addItem("Cancel");
      _confirm.addItem(!strcmp(_field->get, "start ota") ? "Start" : "Confirm");
    } else send(_field->get, _field->set != nullptr);
  }

public:
  explicit AdminScreen(UITask* task) : _task(task) {}
  void onShow() override {
    _phase = LOGIN;
    _authenticated = _login_waiting = false;
    _confirm.active = false;
    _field = nullptr;
    _original[0] = _value[0] = _reply[0] = 0;
  }
  void startFor(const ContactInfo& contact) {
    _target = contact;
    kb().begin("", 15);
    login();  // empty password asks the remote node to check its ACL first
  }
  void closeSession() {
    _task->cancelRoomLogin(solo::RoomLoginCoordinator::ADMIN, _target.id.pub_key);
    the_mesh.closeAdminSession();
    _authenticated = _login_waiting = false;
    _confirm.active = false;
    memset(_original, 0, sizeof(_original));
    memset(_value, 0, sizeof(_value));
    memset(_reply, 0, sizeof(_reply));
    kb().begin("", 15);
  }
  void onRoomLoginResult(const uint8_t* key, bool success, uint8_t permissions) {
    if (!_login_waiting || _phase != LOGIN || memcmp(key, _target.id.pub_key, 4)) return;
    _login_waiting = false;
    if (!allowed()) { leave(); return; }
    if (success && (permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_ADMIN) {
      _authenticated = true;
      the_mesh.authorizeAdmin(_target.id.pub_key);
      root();
    } else {
      kb().begin("", 15);
      _task->showAlert(success ? "Admin rights required" : "Enter admin password", 1400);
    }
  }
  void onRoomLoginTimeout(const uint8_t* key) {
    if (_phase != LOGIN || !_login_waiting || memcmp(key, _target.id.pub_key, 4)) return;
    _login_waiting = false;
    kb().begin("", 15);
    _task->showAlert("No reply: enter password", 1400);
  }
  void onAdminReply(const uint8_t* key, const char* text) {
    if (_phase != WAIT || !allowed() || memcmp(key, _target.id.pub_key, PUB_KEY_SIZE)) return;
    if (_fetching) {
      const char* value = solo::admin::value(text);
      if (value && beginEdit(value)) return;
      showReply("Cannot edit", text);
    } else {
      showReply(solo::admin::confirmed(text) ? "Confirmed" :
          (!strncmp(text, "Error", 5) ? "Rejected" : "Reply"), text);
    }
  }
  void poll() override {
    if (_phase == WAIT && (int32_t)(millis() - _deadline) >= 0) {
      the_mesh.cancelAdminCommand();
      showReply("No reply", "Result unknown.\nNot retried.\nWait before retrying.");
    }
  }
  int render(DisplayDriver& d) override {
    d.setTextSize(1);
    d.setColor(DisplayDriver::LIGHT);
    if (_phase == REPLY) return _view.render(d, _reply_title, _reply, false, false);
    if ((_phase == LOGIN && !_login_waiting) || (_phase == EDIT && _text_edit)) {
      int delay = kb().render(d);
      if (_confirm.active) _confirm.render(d);
      return delay;
    }
    d.drawCenteredHeader(_target.name);
    if (_phase == LOGIN || _phase == WAIT) {
      d.setCursor(2, d.listStart());
      d.print(_phase == LOGIN ? "Logging in..." : "Waiting for reply");
    } else if (_phase == EDIT) {
      d.setCursor(2, d.listStart());
      d.print(_field->label);
      int y = d.listStart() + d.lineStep();
      if (_field->kind == solo::admin::FREQUENCY) {
        d.drawSelectionRow(0, y - 1, d.width(), d.lineStep(), true);
        _frequency.render(d, 2, y);
      } else {
        char val[24];
        float n = _number;
        if (_field->kind == solo::admin::BANDWIDTH) n = _bw;
        if (_field->kind == solo::admin::SF) n = _sf;
        if (_field->kind == solo::admin::CR) n = _cr;
        if (_field->kind == solo::admin::TOGGLE) strcpy(val, n ? "On" : "Off");
        else snprintf(val, sizeof(val), "%g", n);
        d.setCursor(2, y); d.print(val);
      }
    } else {
      int count = _phase == ROOT ? solo::admin::groupCount(targetKind()) : _count;
      drawList(d, count, _sel, _scroll, [&](int i, int y, bool selected, int reserve) {
        drawRowSelection(d, y, selected, reserve);
        int group = _phase == ROOT ? solo::admin::groupAt(targetKind(), i) : 0;
        const char* label = _phase == ROOT ? solo::admin::GROUP_LABELS[group] :
            solo::admin::FIELDS[_items[i]].label;
        d.drawTextEllipsized(2, y, d.width() - reserve - 4, label, selected);
      });
    }
    if (_confirm.active) _confirm.render(d);
    return 500;
  }
  bool handleInput(char c) override {
    if (!allowed()) { leave(); return true; }
    if (_confirm.active) {
      auto result = _confirm.handleInput(c);
      if (result != PopupMenu::NONE && _phase == EDIT && !_text_edit &&
          _field && _field->kind == solo::admin::FREQUENCY) _frequency.active = true;
      if (result == PopupMenu::SELECTED) {
        if (_confirm._sel == 1) {
          char command[161];
          if (_phase == LIST) send(_field->get, false);
          else if (_console) send(_value, false);
          else if (solo::admin::formatCommand(command, sizeof(command), _field->set, _value)) send(command, false);
          else showReply("Not sent", "Value too long");
        } else if (_confirm._sel == 2) {
          if (_console) root(); else _phase = LIST;
        }
      }
      return true;
    }
    if (_phase == LOGIN) {
      if (_login_waiting) { if (c == KEY_CANCEL) leave(); }
      else {
        auto result = kb().handleInput(c);
        if (result == KeyboardWidget::DONE) login();
        else if (result == KeyboardWidget::CANCELLED) leave();
      }
      return true;
    }
    if (_phase == WAIT) {
      if (c == KEY_CANCEL) {
        the_mesh.cancelAdminCommand();
        showReply("Cancelled wait", "Command may have run.\nWait before retrying.");
      }
      return true;
    }
    if (_phase == REPLY) {
      if (_view.handleInput(c) != FullscreenMsgView::NONE) {
        if (_console) root(); else _phase = LIST;
      }
      return true;
    }
    if (_phase == EDIT) {
      if (_text_edit) {
        auto result = kb().handleInput(c);
        if (result == KeyboardWidget::DONE || result == KeyboardWidget::CANCELLED) reviewEdit();
      } else if (_field->kind == solo::admin::FREQUENCY) {
        if (_frequency.handleInput(c) != DigitEditor::NONE) reviewEdit();
      } else if (c == KEY_ENTER || c == KEY_CANCEL) reviewEdit();
      else if (keyIsPrev(c) || keyIsNext(c)) {
        int dir = keyIsNext(c) ? 1 : -1;
        if (_field->kind == solo::admin::TOGGLE) _number = !_number;
        else if (_field->kind == solo::admin::BANDWIDTH) RadioParamsEditor::stepBW(_bw, dir);
        else if (_field->kind == solo::admin::SF) RadioParamsEditor::stepSF(_sf, dir);
        else if (_field->kind == solo::admin::CR) RadioParamsEditor::stepCR(_cr, dir);
        else {
          _number = solo::admin::stepNumber(*_field, _number, dir);
        }
      }
      return true;
    }
    int count = _phase == ROOT ? solo::admin::groupCount(targetKind()) : _count;
    if (c == KEY_CANCEL) { if (_phase == ROOT) leave(); else root(); }
    else if (c == KEY_UP && count) _sel = (_sel + count - 1) % count;
    else if (c == KEY_DOWN && count) _sel = (_sel + 1) % count;
    else if (c == KEY_ENTER && count) {
      if (_phase == LIST) activate();
      else {
        _group = solo::admin::groupAt(targetKind(), _sel);
        _console = _group == solo::admin::CONSOLE;
        if (_console) openText("", solo::AdminSession::TEXT_LIMIT); else buildList();
      }
    }
    return true;
  }
};
