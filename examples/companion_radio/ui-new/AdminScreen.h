#pragma once
// Tools > Admin: administer a remote repeater/room you have admin rights on
// (over the mesh -- the on-device equivalent of the app's "repeater admin",
// see docs/cli_commands.md). This device's own settings live in Settings /
// Tools, not here -- Admin is purely for remote nodes.
//
// Entry is always a target-then-act flow: Tools > Admin (or a repeater/room's
// Hold-Enter "Admin" action in Nodes) opens Tools > Nodes in a pick-mode
// (NearbyScreen::startPickAdminTarget(), the same borrow-another-screen's-list
// idiom the channel/bot pickers use), and picking a node calls the single
// canonical UITask::openAdminFor(ContactInfo) -> startFor(). Backing out of a
// command screen returns to that picker; backing out of the picker returns to
// Tools.
//
// After login (session password, self-heals like room logins) a category tab
// carousel of AdminField{label, get_cmd, set_prefix} rows unlocks:
//   set_prefix==null -> action: send get_cmd literally (e.g. "reboot")
//   get_cmd==null    -> set-only: blank keyboard, send "<set_prefix> <value>"
//   both set         -> get, pre-fill keyboard with the reply, then set
// The trailing "Custom command..." row (both null) is a free-text escape hatch
// with {}-key CLI command-name completion (CLI_COMMANDS).

#include "FullscreenMsgView.h"
#include "TabBar.h"
#include <helpers/ClientACL.h>   // PERM_ACL_ADMIN / PERM_ACL_ROLE_MASK

class AdminScreen : public UIScreen {
  UITask* _task;

  enum Phase : uint8_t { LOGIN, COMMAND, REPLY };
  Phase _phase = LOGIN;

  ContactInfo _target;

  // LOGIN
  char _login_pw[16] = "";
  // True while waiting for a login result with no keyboard on screen -- either a
  // silent retry with a remembered password, or right after the typed password
  // was submitted. Distinct from the keyboard being open (kb().render()/
  // handleInput() only run while this is false).
  bool _login_waiting = false;

  // One-slot memo: the last contact successfully admin-logged-into this visit,
  // so re-entering COMMAND for the same target right after doesn't require
  // retyping the password. Cleared in onShow().
  uint8_t _admin_ok_prefix[4] = { 0, 0, 0, 0 };
  bool    _admin_ok = false;

  // ── COMMAND: category tabs / rows ───────────────────────────────────────────
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

  uint8_t _tab = 0;
  int     _row_sel = 0, _row_scroll = 0;

  bool        _kb_active = false;
  char        _cmd_text[161] = "";           // the command being built/sent
  bool        _waiting = false;              // awaiting sendAdminCommand()'s reply
  uint32_t    _cmd_deadline_ms = 0;
  bool        _fetch_for_edit = false;       // true while waiting on a "get" whose reply opens an editor
  const char* _pending_set_prefix = nullptr; // non-null => edited keyboard text gets this prefix on submit

  // REPLY
  FullscreenMsgView _reply_view;
  char _reply_text[200] = "";

  KeyboardWidget& kb() { return _task->keyboard(); }

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

  // Silent retry with a remembered password (see startFor()) -- same idea as
  // MessagesScreen's room-login flow: no keyboard shown, just a "Logging
  // in..." wait for the async result.
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

  // Open the free-text keyboard, pre-filled with `initial`. Shared by the
  // Custom-command row and remote set-only/get-then-edit fields. `cli_autocomplete`
  // wires the {} picker to CLI command-name completion (Custom-command row only).
  void openValueKb(const char* initial, bool cli_autocomplete = false) {
    kb().begin(initial, (int)sizeof(_cmd_text) - 1);   // begin() resets any previous placeholder hook
    kb().clearPlaceholders();   // a CLI command/value is literal, not a message
    if (cli_autocomplete) kb().setPlaceholderRefresh(refreshCmdPlaceholders, nullptr, "Commands:");
    _kb_active = true;
  }

  // Candidate completions for the Custom-command row's {} picker: every remotely-
  // usable command from docs/cli_commands.md (Serial-Only ones and the region
  // sub-grammar's many variants excluded). Entries needing a value end in a
  // trailing space, ready to type it.
  static const char* const CLI_COMMANDS[];
  static const int CLI_COMMAND_COUNT;

  // KeyboardWidget::PlaceholderRefreshFn -- repopulates the {} list with only the
  // CLI_COMMANDS entries matching the word currently being typed (text since the
  // last space), so it narrows as you type. An empty word matches everything.
  static void refreshCmdPlaceholders(KeyboardWidget& kbw, void* /*ctx*/) {
    kbw.clearPlaceholders();
    int word_start = kbw.len;
    while (word_start > 0 && kbw.buf[word_start - 1] != ' ') word_start--;
    const char* word = kbw.buf + word_start;
    int word_len = kbw.len - word_start;
    for (int i = 0; i < CLI_COMMAND_COUNT; i++)
      if (word_len == 0 || strncmp(CLI_COMMANDS[i], word, word_len) == 0)
        kbw.addPlaceholder(CLI_COMMANDS[i]);
  }

  // Dispatch Enter on a COMMAND row per the three field shapes (see file header).
  // The trailing "Custom command..." row (both null) is handled first.
  void activateField(const AdminField& f) {
    _fetch_for_edit = false;
    _pending_set_prefix = nullptr;
    if (f.get_cmd == nullptr && f.set_prefix == nullptr) {          // Custom command...
      openValueKb(_cmd_text, true);
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
  // wait) -- fall back to a blank editor rather than dead-ending, so the field
  // can still be set blind.
  void fallBackToBlankEdit() {
    _fetch_for_edit = false;
    openValueKb("");
  }

public:
  explicit AdminScreen(UITask* task) : _task(task) {}

  // Per-visit reset (see UIScreen::onShow). startFor() runs immediately after
  // and sets the real phase/target; these are just safe defaults.
  void onShow() override {
    _phase = LOGIN;
    _tab = 0;
    _row_sel = _row_scroll = 0;
    _kb_active = false;
    _waiting = false;
    _fetch_for_edit = false;
    _pending_set_prefix = nullptr;
    _login_waiting = false;
    _admin_ok = false;
  }

  // Canonical entry for a specific target -- called by UITask::openAdminFor(),
  // itself reached from NearbyScreen's "Admin" context-menu action or its
  // Admin-target pick-mode. Skips straight past login if this contact is already
  // admin-ok this visit.
  void startFor(const ContactInfo& ci) {
    _target = ci;
    if (isAdminOk(ci.id.pub_key)) {
      _tab = ATAB_SYSTEM; _row_sel = _row_scroll = 0;
      _phase = COMMAND;
    } else {
      char saved_pw[sizeof(_login_pw)];
      if (the_mesh.getRoomPassword(ci.id.pub_key, saved_pw, sizeof(saved_pw)))
        startLoginWithSaved(saved_pw);
      else
        startLogin();
    }
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
      _task->showAlert("Not admin on this node", 1600);
      _task->pickAdminTarget();
    } else {
      // Wrong/stale password -- forget it so the next attempt prompts fresh,
      // same self-healing behaviour as a room login.
      the_mesh.forgetRoomPassword(pub_key);
      _task->showAlert("Login failed", 1400);
      _task->pickAdminTarget();
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
    if (_phase == LOGIN) {
      if (_login_waiting) {
        if (c == KEY_CANCEL) { _login_waiting = false; _task->pickAdminTarget(); }
        return true;
      }
      auto r = kb().handleInput(c);
      if (r == KeyboardWidget::CANCELLED) {
        _task->pickAdminTarget();
      } else if (r == KeyboardWidget::DONE) {
        strncpy(_login_pw, kb().buf, sizeof(_login_pw) - 1);
        _login_pw[sizeof(_login_pw) - 1] = '\0';
        bool sent = the_mesh.sendRoomLogin(_target, _login_pw);
        _task->showAlert(sent ? "Logging in..." : "Login failed", sent ? 1000 : 1500);
        if (sent) _login_waiting = true; else _task->pickAdminTarget();
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
            sendCommand();
          } else {
            strncpy(_cmd_text, edited, sizeof(_cmd_text) - 1);
            _cmd_text[sizeof(_cmd_text) - 1] = '\0';
            if (_cmd_text[0]) sendCommand();
          }
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
      if (c == KEY_CANCEL) { _task->pickAdminTarget(); return true; }
      if (keyIsPrev(c)) { _tab = (_tab + ATAB_COUNT - 1) % ATAB_COUNT; _row_sel = _row_scroll = 0; return true; }
      if (keyIsNext(c)) { _tab = (_tab + 1) % ATAB_COUNT;             _row_sel = _row_scroll = 0; return true; }
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
  { "Send advert",          "advert",         nullptr },
  { "Send zero-hop advert", "advert.zerohop", nullptr },
  { "Sync clock",           "clock sync",     nullptr },
  { "Reboot",               "reboot",         nullptr },  // disruptive + no confirm: keep off the default row
  { "Custom command...",    nullptr,          nullptr },
};

const int AdminScreen::ROWS_PER_TAB[AdminScreen::ATAB_COUNT] = {
  sizeof(AdminScreen::SYSTEM_FIELDS)  / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::RADIO_FIELDS)   / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::ROUTING_FIELDS) / sizeof(AdminScreen::AdminField),
  sizeof(AdminScreen::ACTION_FIELDS)  / sizeof(AdminScreen::AdminField),
};

const char* const AdminScreen::CLI_COMMANDS[] = {
  "reboot", "poweroff", "shutdown", "clkreboot", "clock", "clock sync",
  "advert", "advert.zerohop", "start ota", "erase",
  "neighbors", "neighbor.remove ", "discover.neighbors",
  "ver", "board",
  "get radio", "set radio ", "get tx", "set tx ", "tempradio ",
  "get freq", "set freq ", "get radio.rxgain", "set radio.rxgain ",
  "get name", "set name ", "get lat", "set lat ", "get lon", "set lon ",
  "get owner.info", "set owner.info ", "get adc.multiplier", "set adc.multiplier ",
  "get public.key", "get role",
  "powersaving", "powersaving on", "powersaving off",
  "get repeat", "set repeat ",
  "get path.hash.mode", "set path.hash.mode ",
  "get loop.detect", "set loop.detect ",
  "get txdelay", "set txdelay ", "get direct.txdelay", "set direct.txdelay ",
  "get rxdelay", "set rxdelay ", "get dutycycle", "set dutycycle ",
  "get af", "set af ", "get int.thresh", "set int.thresh ",
  "get agc.reset.interval", "set agc.reset.interval ",
  "get multi.acks", "set multi.acks ",
  "get flood.advert.interval", "set flood.advert.interval ",
  "get advert.interval", "set advert.interval ",
  "get flood.max", "set flood.max ",
  "get flood.max.unscoped", "set flood.max.unscoped ",
  "get flood.max.advert", "set flood.max.advert ",
  "setperm ", "get acl", "get allow.read.only", "set allow.read.only ",
  "region", "region load", "region save", "region list ",
  "gps", "gps sync", "gps setloc", "gps advert ",
  "sensor list", "sensor get ", "sensor set ",
  "get bridge.type", "get bridge.enabled", "set bridge.enabled ",
  "get bridge.delay", "set bridge.delay ", "get bridge.source", "set bridge.source ",
  "get bridge.baud", "set bridge.baud ", "get bridge.channel", "set bridge.channel ",
  "get bridge.secret", "set bridge.secret ",
  "get pwrmgt.support", "get pwrmgt.source", "get pwrmgt.bootreason", "get pwrmgt.bootmv",
  "password ", "get guest.password", "set guest.password ",
};
const int AdminScreen::CLI_COMMAND_COUNT = sizeof(AdminScreen::CLI_COMMANDS) / sizeof(AdminScreen::CLI_COMMANDS[0]);
