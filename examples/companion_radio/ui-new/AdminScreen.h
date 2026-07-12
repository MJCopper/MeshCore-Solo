#pragma once
// Tools > Admin: send CLI commands to a repeater/room server you have admin
// permission on, and see the text reply -- the on-device equivalent of the
// companion app's "repeater admin" feature (see docs/cli_commands.md for the
// command grammar). The wire protocol already supports this in full
// (MyMesh::sendRoomLogin() / sendAdminCommand() / onCommandDataRecv()); this
// screen is just the missing UI layer.
//
// Credentials are session-only by design: the admin password is typed fresh
// each time this screen is entered for a given contact (onShow() clears the
// one-slot "logged in" memo below) rather than persisted like room chat
// passwords are -- admin actions are infrequent, and keeping them out of the
// same store as room chat credentials keeps the two concerns separate.
//
// Once logged in, commands are organised into a category tab carousel (same
// "true carousel" idiom BotScreen uses: LEFT/RIGHT exclusively switches tabs,
// UP/DOWN moves within the active tab's rows, Enter drives everything else)
// instead of a single free-text box, so common settings don't need the CLI
// grammar memorised. Each row is one of three shapes, driven by a single
// AdminField{label, get_cmd, set_prefix} table per tab -- see fieldAt():
//   - set_prefix == nullptr        -> action: get_cmd is the literal command,
//                                     sent immediately (e.g. "reboot").
//   - get_cmd == nullptr           -> set-only: opens a blank keyboard for the
//                                     new value (there's no getter for it).
//   - both set                     -> get-then-edit-then-set: fetches get_cmd,
//                                     opens the keyboard pre-filled with the
//                                     raw reply, sends "<set_prefix> <edited>".
// The trailing "Custom command..." row (both null) is the escape hatch: it
// opens the same free-text entry this screen originally shipped with, for
// anything not covered by a field row.

#include "FullscreenMsgView.h"
#include "TabBar.h"
#include <helpers/ClientACL.h>   // PERM_ACL_ADMIN / PERM_ACL_ROLE_MASK

class AdminScreen : public UIScreen {
  UITask* _task;

  enum Phase : uint8_t { PICKER, LOGIN, COMMAND, REPLY };
  Phase _phase = PICKER;

  // PICKER -- contacts of type ADV_TYPE_REPEATER or ADV_TYPE_ROOM only; a
  // plain chat contact can never have an admin ACL entry to log into.
  int      _sel = 0, _scroll = 0;
  int      _num = 0;
  uint16_t _indices[MAX_CONTACTS];
  ContactInfo _target;

  // LOGIN
  char _login_pw[16] = "";
  // True while waiting for the result of a login attempt with no keyboard on
  // screen -- either a silent retry with a remembered password, or right after
  // the keyboard-typed password was submitted. Distinct from the keyboard
  // itself being open (kb().render()/handleInput() only run while this is false).
  bool _login_waiting = false;

  // One-slot memo: the last contact this screen successfully admin-logged
  // into this visit, so re-entering COMMAND for the same target right after
  // doesn't require retyping the password. Cleared in onShow() -- see the
  // session-only design note above.
  uint8_t _admin_ok_prefix[4] = { 0, 0, 0, 0 };
  bool    _admin_ok = false;

  // COMMAND -- category tabs + rows (see file header for the row shapes).
  enum AdminTab : uint8_t { ATAB_SYSTEM, ATAB_RADIO, ATAB_ROUTING, ATAB_ACTIONS, ATAB_COUNT };
  static const char* TAB_LABELS[ATAB_COUNT];

  struct AdminField { const char* label; const char* get_cmd; const char* set_prefix; };
  static const AdminField SYSTEM_FIELDS[];
  static const AdminField RADIO_FIELDS[];
  static const AdminField ROUTING_FIELDS[];
  static const AdminField ACTION_FIELDS[];
  static const int ROWS_PER_TAB[ATAB_COUNT];

  static const AdminField& fieldAt(int tab, int row) {
    switch (tab) {
      case ATAB_SYSTEM:  return SYSTEM_FIELDS[row];
      case ATAB_RADIO:   return RADIO_FIELDS[row];
      case ATAB_ROUTING: return ROUTING_FIELDS[row];
      default:           return ACTION_FIELDS[row];
    }
  }

  uint8_t _tab = ATAB_SYSTEM;
  int     _row_sel = 0, _row_scroll = 0;

  bool        _kb_active = false;
  char        _cmd_text[161] = "";
  bool        _waiting = false;
  uint32_t    _cmd_deadline_ms = 0;
  bool        _fetch_for_edit = false;      // true while waiting on a "get" whose reply opens an editor
  const char* _pending_set_prefix = nullptr; // non-null => the edited keyboard text gets this prefix on submit

  // REPLY
  FullscreenMsgView _reply_view;
  char _reply_text[200] = "";

  KeyboardWidget& kb() { return _task->keyboard(); }

  void buildList() {
    ContactInfo c;
    int total = the_mesh.getNumContacts();
    _num = 0;
    for (int i = 0; i < total; i++) {
      if (!the_mesh.getContactByIdx(i, c)) continue;
      if (c.type != ADV_TYPE_REPEATER && c.type != ADV_TYPE_ROOM) continue;
      _indices[_num++] = (uint16_t)i;
    }
  }

  bool isAdminOk(const uint8_t* pub_key) const {
    return _admin_ok && memcmp(_admin_ok_prefix, pub_key, 4) == 0;
  }

  void startLogin() {
    _login_pw[0] = '\0';
    _login_waiting = false;
    kb().begin("", 15);       // admin password: same max length as room/repeater login
    kb().clearPlaceholders(); // {loc}/{time} are for messages, not a password
    _phase = LOGIN;
  }

  // Silent retry with a remembered password (see PICKER's Enter handler) --
  // same idea as MessagesScreen's room-login flow: no keyboard shown, just a
  // "Logging in..." wait for the async result.
  void startLoginWithSaved(const char* password) {
    strncpy(_login_pw, password, sizeof(_login_pw) - 1);
    _login_pw[sizeof(_login_pw) - 1] = '\0';
    bool sent = the_mesh.sendRoomLogin(_target, _login_pw);
    _task->showAlert(sent ? "Logging in..." : "Login failed", sent ? 1000 : 1500);
    if (sent) { _login_waiting = true; _phase = LOGIN; }
  }

  void sendCommand() {
    uint32_t est_timeout = 0;
    if (!the_mesh.sendAdminCommand(_target, _cmd_text, est_timeout)) {
      _task->showAlert("Send failed", 1200);
      _fetch_for_edit = false;
      _pending_set_prefix = nullptr;
      return;
    }
    _waiting = true;
    // Generous margin over the base estimate, same reasoning as the DM ACK
    // deadline in MessagesScreen::sendText() -- a slow multi-hop reply isn't
    // prematurely shown as a timeout.
    _cmd_deadline_ms = millis() + est_timeout + 4000;
    _task->showAlert(_fetch_for_edit ? "Fetching..." : "Sent, waiting...", 800);
  }

  // Open the free-text command keyboard, pre-filled with `initial`. Shared by
  // the Custom-command row, set-only fields, and the get-then-edit reply path.
  void openValueKb(const char* initial) {
    kb().begin(initial, (int)sizeof(_cmd_text) - 1);
    kb().clearPlaceholders();   // a CLI command/value is literal, not a message
    _kb_active = true;
  }

  // Dispatch Enter on a COMMAND-phase row per the three field shapes (see
  // file header). idx == ROWS_PER_TAB[_tab]-1 on the Actions tab is the
  // "Custom command..." sentinel (both null), handled first.
  void activateField(const AdminField& f) {
    _fetch_for_edit = false;
    _pending_set_prefix = nullptr;
    if (f.get_cmd == nullptr && f.set_prefix == nullptr) {          // Custom command...
      openValueKb(_cmd_text);
    } else if (f.set_prefix == nullptr) {                            // Action
      strncpy(_cmd_text, f.get_cmd, sizeof(_cmd_text) - 1);
      _cmd_text[sizeof(_cmd_text) - 1] = '\0';
      sendCommand();
    } else if (f.get_cmd == nullptr) {                               // Set-only
      _pending_set_prefix = f.set_prefix;
      openValueKb("");
    } else {                                                         // Get-then-edit-then-set
      _pending_set_prefix = f.set_prefix;
      _fetch_for_edit = true;
      strncpy(_cmd_text, f.get_cmd, sizeof(_cmd_text) - 1);
      _cmd_text[sizeof(_cmd_text) - 1] = '\0';
      sendCommand();
    }
  }

  // A fetch-for-edit didn't get a reply (timeout or the user cancelled the
  // wait) -- fall back to a blank editor rather than dead-ending, so the
  // field can still be set blind.
  void fallBackToBlankEdit() {
    _fetch_for_edit = false;
    openValueKb("");
  }

public:
  explicit AdminScreen(UITask* task) : _task(task) {}

  // Central per-visit reset (see UIScreen::onShow) -- always starts back at
  // the target picker and forgets the admin-login memo, so credentials really
  // are re-typed each time the tool is (re-)entered.
  void onShow() override {
    _phase = PICKER;
    _sel = _scroll = 0;
    buildList();
    _tab = ATAB_SYSTEM;
    _row_sel = _row_scroll = 0;
    _kb_active = false;
    _waiting = false;
    _fetch_for_edit = false;
    _pending_set_prefix = nullptr;
    _login_waiting = false;
    _admin_ok = false;
  }

  // Async result of the on-device login, routed here from UITask::onRoomLoginResult()
  // only while this screen is current (see that routing for why).
  void onRoomLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) {
    if (_phase != LOGIN) return;   // stale/unrelated result
    _login_waiting = false;
    if (success && (permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_ADMIN) {
      memcpy(_admin_ok_prefix, pub_key, 4);
      _admin_ok = true;
      the_mesh.saveRoomPassword(pub_key, _login_pw);   // remember it -- same logic as room login
      _tab = ATAB_SYSTEM;
      _row_sel = _row_scroll = 0;
      _phase = COMMAND;
      _task->showAlert("Logged in (admin)", 1000);
    } else if (success) {
      // Correct password, just insufficient permission -- leave any saved
      // password alone, retyping the same one won't change the outcome.
      _phase = PICKER;
      _task->showAlert("Not admin on this node", 1600);
    } else {
      // Wrong/stale password -- forget it so the next attempt prompts fresh,
      // same self-healing behaviour as a room login.
      the_mesh.forgetRoomPassword(pub_key);
      _phase = PICKER;
      _task->showAlert("Login failed", 1400);
    }
  }

  // Async CLI reply, routed here from UITask::onAdminReply().
  void onAdminReply(const uint8_t* pub_key, const char* text) {
    if (!_waiting || memcmp(_target.id.pub_key, pub_key, 4) != 0) return;
    _waiting = false;
    if (_fetch_for_edit) {
      _fetch_for_edit = false;
      char trimmed[161];
      strncpy(trimmed, text, sizeof(trimmed) - 1);
      trimmed[sizeof(trimmed) - 1] = '\0';
      size_t n = strlen(trimmed);
      while (n > 0 && (trimmed[n-1] == '\n' || trimmed[n-1] == '\r' || trimmed[n-1] == ' ')) trimmed[--n] = '\0';
      openValueKb(trimmed);   // _pending_set_prefix is already set from activateField()
      return;
    }
    strncpy(_reply_text, text, sizeof(_reply_text) - 1);
    _reply_text[sizeof(_reply_text) - 1] = '\0';
    _reply_view.begin();
    _phase = REPLY;
  }

  void poll() override {
    if (_phase == COMMAND && _waiting && (int32_t)(millis() - _cmd_deadline_ms) >= 0) {
      _waiting = false;
      if (_fetch_for_edit) { fallBackToBlankEdit(); _task->showAlert("Fetch failed - enter value", 1400); }
      else                 { _task->showAlert("No response (timeout)", 1600); }
    }
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    if (_phase == PICKER) {
      display.drawCenteredHeader("ADMIN: SELECT NODE");
      if (_num == 0) {
        display.drawTextCentered(display.width() / 2, display.height() / 2, "No repeaters/rooms");
        return 5000;
      }
      drawList(display, _num, _sel, _scroll, [&](int idx, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        ContactInfo c;
        if (the_mesh.getContactByIdx(_indices[idx], c)) {
          char filtered[sizeof(c.name)];
          display.translateUTF8ToBlocks(filtered, c.name, sizeof(filtered));
          display.drawTextEllipsized(2, y, display.width() - 4 - reserve, filtered);
        }
      });
      return 2000;
    }

    if (_phase == LOGIN) {
      if (_login_waiting) {
        display.drawCenteredHeader("ADMIN LOGIN");
        display.setCursor(2, display.listStart());
        display.print("Logging in...");
        return 500;
      }
      return kb().render(display);
    }

    if (_phase == COMMAND) {
      if (_kb_active) return kb().render(display);
      if (_waiting) {
        char title[24];
        snprintf(title, sizeof(title), "%.23s", _target.name);
        display.drawCenteredHeader(title);
        display.setCursor(2, display.listStart());
        display.print(_fetch_for_edit ? "Fetching..." : "Waiting for reply...");
        return 500;
      }
      tabbar::draw(display, TAB_LABELS, ATAB_COUNT, _tab);
      int n = ROWS_PER_TAB[_tab];
      drawList(display, n, _row_sel, _row_scroll, [&](int i, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve, fieldAt(_tab, i).label);
      });
      return 2000;
    }

    // REPLY
    return _reply_view.render(display, _target.name, _reply_text, false, false);
  }

  bool handleInput(char c) override {
    if (_phase == PICKER) {
      if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
      if (c == KEY_UP   && _num > 0) { _sel = (_sel > 0) ? _sel - 1 : _num - 1; return true; }
      if (c == KEY_DOWN && _num > 0) { _sel = (_sel < _num - 1) ? _sel + 1 : 0; return true; }
      if (c == KEY_ENTER && _num > 0) {
        if (the_mesh.getContactByIdx(_indices[_sel], _target)) {
          if (isAdminOk(_target.id.pub_key)) {
            _tab = ATAB_SYSTEM; _row_sel = _row_scroll = 0;
            _phase = COMMAND;
          } else {
            // Known password from an earlier successful admin login (this boot
            // or a previous one) -- retry silently instead of re-prompting,
            // same as MessagesScreen's room-login flow.
            char saved_pw[sizeof(_login_pw)];
            if (the_mesh.getRoomPassword(_target.id.pub_key, saved_pw, sizeof(saved_pw)))
              startLoginWithSaved(saved_pw);
            else
              startLogin();
          }
        }
        return true;
      }
      return true;
    }

    if (_phase == LOGIN) {
      if (_login_waiting) {
        if (c == KEY_CANCEL) { _login_waiting = false; _phase = PICKER; }
        return true;
      }
      auto r = kb().handleInput(c);
      if (r == KeyboardWidget::CANCELLED) {
        _phase = PICKER;
      } else if (r == KeyboardWidget::DONE) {
        strncpy(_login_pw, kb().buf, sizeof(_login_pw) - 1);
        _login_pw[sizeof(_login_pw) - 1] = '\0';
        bool sent = the_mesh.sendRoomLogin(_target, _login_pw);
        _task->showAlert(sent ? "Logging in..." : "Login failed", sent ? 1000 : 1500);
        if (sent) _login_waiting = true; else _phase = PICKER;
        // else: stay in LOGIN until onRoomLoginResult() fires above.
      }
      return true;
    }

    if (_phase == COMMAND) {
      if (_kb_active) {
        auto r = kb().handleInput(c);
        if (r == KeyboardWidget::DONE) {
          _kb_active = false;
          char edited[sizeof(_cmd_text)];
          strncpy(edited, kb().buf, sizeof(edited) - 1);
          edited[sizeof(edited) - 1] = '\0';
          if (_pending_set_prefix) {
            snprintf(_cmd_text, sizeof(_cmd_text), "%s %s", _pending_set_prefix, edited);
            _pending_set_prefix = nullptr;
          } else {
            strncpy(_cmd_text, edited, sizeof(_cmd_text) - 1);
            _cmd_text[sizeof(_cmd_text) - 1] = '\0';
          }
          if (_cmd_text[0]) sendCommand();
        } else if (r == KeyboardWidget::CANCELLED) {
          _kb_active = false;
          _pending_set_prefix = nullptr;
        }
        return true;
      }
      if (_waiting) {
        if (c == KEY_CANCEL) {
          _waiting = false;
          if (_fetch_for_edit) fallBackToBlankEdit();
        }
        return true;
      }
      if (c == KEY_CANCEL) { _phase = PICKER; return true; }
      if (keyIsPrev(c)) { _tab = (_tab + ATAB_COUNT - 1) % ATAB_COUNT; _row_sel = _row_scroll = 0; return true; }
      if (keyIsNext(c)) { _tab = (_tab + 1) % ATAB_COUNT;              _row_sel = _row_scroll = 0; return true; }
      int n = ROWS_PER_TAB[_tab];
      if (c == KEY_UP)   { _row_sel = (_row_sel > 0) ? _row_sel - 1 : n - 1; return true; }
      if (c == KEY_DOWN) { _row_sel = (_row_sel < n - 1) ? _row_sel + 1 : 0; return true; }
      if (c == KEY_ENTER) { activateField(fieldAt(_tab, _row_sel)); return true; }
      return true;
    }

    // REPLY
    auto res = _reply_view.handleInput(c);
    if (res != FullscreenMsgView::NONE) _phase = COMMAND;   // CLOSE / PREV / NEXT / REPLY all just return here
    return true;
  }
};

const char* AdminScreen::TAB_LABELS[AdminScreen::ATAB_COUNT] = { "System", "Radio", "Routing", "Actions" };

const AdminScreen::AdminField AdminScreen::SYSTEM_FIELDS[] = {
  { "Name",           "get name",       "set name" },
  { "Owner info",     "get owner.info", "set owner.info" },
  { "Admin password", nullptr,          "password" },
};
const AdminScreen::AdminField AdminScreen::RADIO_FIELDS[] = {
  { "Radio (f,bw,sf,cr)", "get radio", "set radio" },
  { "TX power",           "get tx",    "set tx" },
};
const AdminScreen::AdminField AdminScreen::ROUTING_FIELDS[] = {
  { "Repeat",                    "get repeat",               "set repeat" },
  { "Advert interval (min)",     "get advert.interval",       "set advert.interval" },
  { "Flood advert interval (h)", "get flood.advert.interval", "set flood.advert.interval" },
  { "Max hops",                  "get flood.max",             "set flood.max" },
};
const AdminScreen::AdminField AdminScreen::ACTION_FIELDS[] = {
  { "Reboot",               "reboot",         nullptr },
  { "Send advert",          "advert",         nullptr },
  { "Send zero-hop advert", "advert.zerohop", nullptr },
  { "Sync clock",           "clock sync",     nullptr },
  { "Custom command...",    nullptr,          nullptr },
};

const int AdminScreen::ROWS_PER_TAB[AdminScreen::ATAB_COUNT] = {
  sizeof(AdminScreen::SYSTEM_FIELDS)  / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::RADIO_FIELDS)   / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::ROUTING_FIELDS) / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::ACTION_FIELDS)  / sizeof(AdminScreen::AdminField),
};
