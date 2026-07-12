#pragma once
// Tools > Admin: administer a remote repeater/room you have admin rights on
// (over the mesh -- the on-device equivalent of the app's "repeater admin",
// see docs/cli_commands.md) or this device itself. Also reachable from Tools >
// Nodes' Hold-Enter menu (NearbyScreen::ACT_ADMIN).
//
// CHOOSER -> "This device" (LOCAL mode) or "Remote node..." (opens Tools > Nodes
// in pick-mode -- NearbyScreen::startPickAdminTarget(), the same borrow-another-
// screen's-list idiom the channel/bot pickers use, so it looks exactly like
// normal Nodes browsing). Both the picker and the Nodes context-menu converge
// on the single canonical UITask::openAdminFor(ContactInfo) -> startFor().
//
// REMOTE mode: login (session password, self-heals like room logins) unlocks a
// category tab carousel of AdminField{label, get_cmd, set_prefix} rows:
//   set_prefix==null -> action: send get_cmd literally (e.g. "reboot")
//   get_cmd==null    -> set-only: blank keyboard, send "<set_prefix> <value>"
//   both set         -> get, pre-fill keyboard with the reply, then set
// The trailing "Custom command..." row (both null) is a free-text escape hatch
// with {}-key CLI command-name completion (CLI_COMMANDS).
//
// LOCAL mode: no login, no network -- a 2-tab carousel of LocalField{label, kind}
// rows mapped straight onto NodePrefs/sensors, reusing Settings' own apply chains
// (UITask::applyRadioParams()/applyTxPower(), MyMesh::savePrefs()) rather than a
// CLI-grammar port (this firmware has no CommonCLI -- see CODE_REVIEW.md). Fields
// pre-fill with the current value read synchronously and write+apply on submit.

#include "FullscreenMsgView.h"
#include "TabBar.h"
#include <helpers/ClientACL.h>   // PERM_ACL_ADMIN / PERM_ACL_ROLE_MASK

class AdminScreen : public UIScreen {
  UITask* _task;

  enum Phase : uint8_t { CHOOSER, LOGIN, COMMAND, REPLY };
  Phase _phase = CHOOSER;

  // CHOOSER -- "This device" (row 0) / "Remote node..." (row 1). Reuses _sel.
  int _sel = 0;

  bool _local_mode = false;   // COMMAND is administering this device, not _target
  ContactInfo _target;        // valid only when !_local_mode

  // LOGIN (remote only)
  char _login_pw[16] = "";
  // True while waiting for the result of a login attempt with no keyboard on
  // screen -- either a silent retry with a remembered password, or right after
  // the keyboard-typed password was submitted. Distinct from the keyboard
  // itself being open (kb().render()/handleInput() only run while this is false).
  bool _login_waiting = false;

  // One-slot memo: the last contact this screen successfully admin-logged
  // into this visit, so re-entering COMMAND for the same target right after
  // doesn't require retyping the password. Cleared in onShow() -- see the
  // session-only design note in the file header.
  uint8_t _admin_ok_prefix[4] = { 0, 0, 0, 0 };
  bool    _admin_ok = false;

  // ── COMMAND: remote (CLI-grammar) tabs/rows ─────────────────────────────────
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

  // ── COMMAND: local (this-device) tabs/rows ──────────────────────────────────
  static const int LOCAL_TAB_COUNT = 2;
  static const char* LOCAL_TAB_LABELS[LOCAL_TAB_COUNT];

  enum LocalKind : uint8_t { LK_NONE, LK_NAME, LK_RADIO, LK_TXPOWER, LK_LAT, LK_LON, LK_REBOOT, LK_ADVERT };
  struct LocalField { const char* label; LocalKind kind; };
  static const LocalField LOCAL_SYSTEM_FIELDS[];
  static const LocalField LOCAL_ACTION_FIELDS[];
  static const int LOCAL_ROWS_PER_TAB[LOCAL_TAB_COUNT];

  static const LocalField& localFieldAt(int tab, int row) {
    return (tab == 0) ? LOCAL_SYSTEM_FIELDS[row] : LOCAL_ACTION_FIELDS[row];
  }

  int tabCount() const { return _local_mode ? LOCAL_TAB_COUNT : (int)ATAB_COUNT; }
  int rowsInTab(int tab) const { return _local_mode ? LOCAL_ROWS_PER_TAB[tab] : ROWS_PER_TAB[tab]; }
  const char* rowLabel(int tab, int row) const {
    return _local_mode ? localFieldAt(tab, row).label : fieldAt(tab, row).label;
  }

  uint8_t _tab = 0;
  int     _row_sel = 0, _row_scroll = 0;

  bool        _kb_active = false;
  char        _cmd_text[161] = "";           // remote: the command being built/sent
  bool        _waiting = false;              // remote: awaiting sendAdminCommand()'s reply
  uint32_t    _cmd_deadline_ms = 0;
  bool        _fetch_for_edit = false;       // remote: true while waiting on a "get" whose reply opens an editor
  const char* _pending_set_prefix = nullptr; // remote: non-null => edited keyboard text gets this prefix on submit
  LocalKind   _pending_local_kind = LK_NONE; // local: which field the open keyboard is editing

  // REPLY (remote only)
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
  // Custom-command row, remote set-only/get-then-edit fields, and every local
  // field. `cli_autocomplete` wires up the {} picker to complete CLI command
  // names contextually -- only meaningful for the remote Custom-command row.
  void openValueKb(const char* initial, bool cli_autocomplete = false) {
    kb().begin(initial, (int)sizeof(_cmd_text) - 1);   // begin() resets any previous placeholder hook
    kb().clearPlaceholders();   // a CLI command/value is literal, not a message
    if (cli_autocomplete) kb().setPlaceholderRefresh(refreshCmdPlaceholders, nullptr, "Commands:");
    _kb_active = true;
  }

  // Candidate completions for the remote Custom-command row's {} picker: every
  // remotely-usable command from docs/cli_commands.md (Serial-Only ones and
  // the region sub-grammar's many variants excluded -- typing those out stays
  // manual). Entries needing a value end in a trailing space, ready to type it.
  static const char* const CLI_COMMANDS[];
  static const int CLI_COMMAND_COUNT;

  // KeyboardWidget::PlaceholderRefreshFn -- repopulates the {} list with only
  // the CLI_COMMANDS entries matching the word currently being typed (the
  // text since the last space), so the list narrows as you type. An empty
  // word (nothing typed yet, or just after a space) matches everything.
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

  // Dispatch Enter on a remote COMMAND row per the three field shapes (see
  // file header). The trailing "Custom command..." row (both null) is
  // handled first.
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
  // wait) -- fall back to a blank editor rather than dead-ending, so the
  // field can still be set blind.
  void fallBackToBlankEdit() {
    _fetch_for_edit = false;
    openValueKb("");
  }

  // Dispatch Enter on a local COMMAND row: actions fire immediately; value
  // fields open the keyboard pre-filled with the current reading.
  void activateLocalField(const LocalField& f) {
    NodePrefs* p = _task->getNodePrefs();
    char buf[32];
    switch (f.kind) {
      case LK_NAME:
        _pending_local_kind = LK_NAME;
        openValueKb(the_mesh.getNodeName());
        break;
      case LK_RADIO:
        if (p) snprintf(buf, sizeof(buf), "%.3f,%.0f,%d,%d", p->freq, p->bw, (int)p->sf, (int)p->cr);
        else   buf[0] = '\0';
        _pending_local_kind = LK_RADIO;
        openValueKb(buf);
        break;
      case LK_TXPOWER:
        snprintf(buf, sizeof(buf), "%d", p ? (int)p->tx_power_dbm : 0);
        _pending_local_kind = LK_TXPOWER;
        openValueKb(buf);
        break;
      case LK_LAT:
        snprintf(buf, sizeof(buf), "%.6f", sensors.node_lat);
        _pending_local_kind = LK_LAT;
        openValueKb(buf);
        break;
      case LK_LON:
        snprintf(buf, sizeof(buf), "%.6f", sensors.node_lon);
        _pending_local_kind = LK_LON;
        openValueKb(buf);
        break;
      case LK_REBOOT:
        _task->showAlert("Rebooting...", 800);
        board.reboot();
        break;
      case LK_ADVERT:
        _task->showAlert(the_mesh.advert() ? "Advert sent!" : "Advert failed", 1200);
        break;
      default: break;
    }
  }

  // Parse+apply a submitted local field value. Deliberately light validation
  // (parse-success only, no chip/physical-range checks like Settings' radio
  // editor does) -- this panel is a fast power-user shim over the same
  // settings, not a replacement for Settings' fuller, bounds-checked editors.
  void commitLocalField(LocalKind kind, const char* text) {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return;
    switch (kind) {
      case LK_NAME: {
        strncpy(p->node_name, text, sizeof(p->node_name) - 1);
        p->node_name[sizeof(p->node_name) - 1] = '\0';
        the_mesh.savePrefs();
        _task->showAlert("Saved", 800);
        break;
      }
      case LK_RADIO: {
        float freq, bw; int sf, cr;
        if (sscanf(text, "%f,%f,%d,%d", &freq, &bw, &sf, &cr) == 4) {
          p->freq = freq; p->bw = bw; p->sf = (uint8_t)sf; p->cr = (uint8_t)cr;
          _task->applyRadioParams();
          the_mesh.savePrefs();
          _task->showAlert("Saved", 800);
        } else {
          _task->showAlert("Expected freq,bw,sf,cr", 1600);
        }
        break;
      }
      case LK_TXPOWER: {
        int dbm;
        if (sscanf(text, "%d", &dbm) == 1) {
          p->tx_power_dbm = (int8_t)dbm;
          _task->applyTxPower();
          the_mesh.savePrefs();
          _task->showAlert("Saved", 800);
        } else {
          _task->showAlert("Invalid number", 1400);
        }
        break;
      }
      case LK_LAT: {
        double v;
        if (sscanf(text, "%lf", &v) == 1 && v >= -90.0 && v <= 90.0) {
          sensors.node_lat = v;
          the_mesh.savePrefs();
          _task->showAlert("Saved", 800);
        } else {
          _task->showAlert("Lat must be -90..90", 1600);
        }
        break;
      }
      case LK_LON: {
        double v;
        if (sscanf(text, "%lf", &v) == 1 && v >= -180.0 && v <= 180.0) {
          sensors.node_lon = v;
          the_mesh.savePrefs();
          _task->showAlert("Saved", 800);
        } else {
          _task->showAlert("Lon must be -180..180", 1600);
        }
        break;
      }
      default: break;
    }
  }

public:
  explicit AdminScreen(UITask* task) : _task(task) {}

  // Central per-visit reset (see UIScreen::onShow) -- always starts back at
  // the This-device/Remote-node chooser and forgets the admin-login memo, so
  // remote credentials really are re-typed each time the tool is (re-)entered.
  void onShow() override {
    _phase = CHOOSER;
    _sel = 0;
    _local_mode = false;
    _tab = 0;
    _row_sel = _row_scroll = 0;
    _kb_active = false;
    _waiting = false;
    _fetch_for_edit = false;
    _pending_set_prefix = nullptr;
    _pending_local_kind = LK_NONE;
    _login_waiting = false;
    _admin_ok = false;
  }

  // Canonical entry for a specific remote target -- called by
  // UITask::openAdminFor(), itself reached from NearbyScreen's "Admin"
  // context-menu action or its Admin-target pick-mode. Skips straight past
  // login if this contact is already admin-ok this visit.
  void startFor(const ContactInfo& ci) {
    _target = ci;
    _local_mode = false;
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
      _phase = CHOOSER; _sel = 0;
      _task->showAlert("Not admin on this node", 1600);
    } else {
      // Wrong/stale password -- forget it so the next attempt prompts fresh,
      // same self-healing behaviour as a room login.
      the_mesh.forgetRoomPassword(pub_key);
      _phase = CHOOSER; _sel = 0;
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

    if (_phase == CHOOSER) {
      display.drawCenteredHeader("ADMIN");
      static const char* const ROWS[2] = { "This device", "Remote node..." };
      int scroll = 0;
      drawList(display, 2, _sel, scroll, [&](int i, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve, ROWS[i]);
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
      tabbar::draw(display, _local_mode ? LOCAL_TAB_LABELS : TAB_LABELS, tabCount(), _tab);
      int n = rowsInTab(_tab);
      drawList(display, n, _row_sel, _row_scroll, [&](int i, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve, rowLabel(_tab, i));
      });
      return 2000;
    }

    // REPLY
    return _reply_view.render(display, _target.name, _reply_text, false, false);
  }

  bool handleInput(char c) override {
    if (_phase == CHOOSER) {
      if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
      if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : 1; return true; }
      if (c == KEY_DOWN) { _sel = (_sel < 1) ? _sel + 1 : 0; return true; }
      if (c == KEY_ENTER) {
        if (_sel == 0) {
          _local_mode = true;
          _tab = 0; _row_sel = _row_scroll = 0;
          _phase = COMMAND;
        } else {
          _task->pickAdminTarget();   // -> Tools > Nodes pick-mode -> startFor()
        }
        return true;
      }
      return true;
    }

    if (_phase == LOGIN) {
      if (_login_waiting) {
        if (c == KEY_CANCEL) { _login_waiting = false; _phase = CHOOSER; _sel = 0; }
        return true;
      }
      auto r = kb().handleInput(c);
      if (r == KeyboardWidget::CANCELLED) {
        _phase = CHOOSER; _sel = 0;
      } else if (r == KeyboardWidget::DONE) {
        strncpy(_login_pw, kb().buf, sizeof(_login_pw) - 1);
        _login_pw[sizeof(_login_pw) - 1] = '\0';
        bool sent = the_mesh.sendRoomLogin(_target, _login_pw);
        _task->showAlert(sent ? "Logging in..." : "Login failed", sent ? 1000 : 1500);
        if (sent) _login_waiting = true; else { _phase = CHOOSER; _sel = 0; }
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
          if (_local_mode) {
            LocalKind k = _pending_local_kind;
            _pending_local_kind = LK_NONE;
            commitLocalField(k, edited);
          } else if (_pending_set_prefix) {
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
          _pending_local_kind = LK_NONE;
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
      if (c == KEY_CANCEL) { _phase = CHOOSER; _sel = 0; return true; }
      int count = tabCount();
      if (keyIsPrev(c)) { _tab = (_tab + count - 1) % count; _row_sel = _row_scroll = 0; return true; }
      if (keyIsNext(c)) { _tab = (_tab + 1) % count;         _row_sel = _row_scroll = 0; return true; }
      int n = rowsInTab(_tab);
      if (c == KEY_UP)   { _row_sel = (_row_sel > 0) ? _row_sel - 1 : n - 1; return true; }
      if (c == KEY_DOWN) { _row_sel = (_row_sel < n - 1) ? _row_sel + 1 : 0; return true; }
      if (c == KEY_ENTER) {
        if (_local_mode) activateLocalField(localFieldAt(_tab, _row_sel));
        else             activateField(fieldAt(_tab, _row_sel));
        return true;
      }
      return true;
    }

    // REPLY
    auto res = _reply_view.handleInput(c);
    if (res != FullscreenMsgView::NONE) _phase = COMMAND;   // CLOSE / PREV / NEXT / REPLY all just return here
    return true;
  }
};

const char* AdminScreen::TAB_LABELS[AdminScreen::ATAB_COUNT] = { "System", "Radio", "Routing", "Actions" };
const char* AdminScreen::LOCAL_TAB_LABELS[AdminScreen::LOCAL_TAB_COUNT] = { "System", "Actions" };

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

const AdminScreen::LocalField AdminScreen::LOCAL_SYSTEM_FIELDS[] = {
  { "Name",               AdminScreen::LK_NAME },
  { "Radio (f,bw,sf,cr)", AdminScreen::LK_RADIO },
  { "TX power",           AdminScreen::LK_TXPOWER },
  { "Lat",                AdminScreen::LK_LAT },
  { "Lon",                AdminScreen::LK_LON },
};
const AdminScreen::LocalField AdminScreen::LOCAL_ACTION_FIELDS[] = {
  { "Send advert", AdminScreen::LK_ADVERT },
  { "Reboot",      AdminScreen::LK_REBOOT },  // disruptive + no confirm: keep off the default row
};
const int AdminScreen::LOCAL_ROWS_PER_TAB[AdminScreen::LOCAL_TAB_COUNT] = {
  sizeof(AdminScreen::LOCAL_SYSTEM_FIELDS) / sizeof(AdminScreen::LocalField),
  sizeof(AdminScreen::LOCAL_ACTION_FIELDS) / sizeof(AdminScreen::LocalField),
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
