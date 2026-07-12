#pragma once
// On-device channel Add/Edit form (Messages > Channels > "+ Add channel" / Edit).
// Owned by MessagesScreen, which delegates render()/handleInput() to it while
// active() — the same relationship WaypointsView has with TrailScreen.
//
// The Secret field toggles between two entry modes with LEFT/RIGHT:
//   - Passphrase (default): any text, SHA-256'd down to 16 bytes — the same
//     primitive BaseChatMesh::addChannel()/setChannel() already use to derive
//     a channel's routing hash, just applied here to a typed phrase instead of
//     a raw key. Easiest to agree on verbally, like a room password.
//   - Hex key: the exact 32-hex-char (128-bit) secret, e.g. from a channel QR
//     (see docs/qr_codes.md) — for joining a channel whose precise secret you
//     were given rather than agreeing on a new passphrase.
// Either way only a 16-byte (128-bit) secret is produced, matching the
// existing CMD_GET_CHANNEL comment ("NOTE: only 128-bit supported").

#include <Utils.h>

class ChannelsView {
  UITask* _task;

  enum Mode : uint8_t { OFF, ADD, EDIT };
  uint8_t _mode = OFF;
  int     _idx = -1;              // channel slot being added/edited

  char _name[32] = "";
  bool _hex_mode = false;         // false = passphrase, true = 32-hex-char raw key
  char _secret_text[33] = "";     // passphrase or hex string typed so far

  int  _sel = 0;                  // 0=Name 1=Secret 2=[Save]
  bool _kb_active = false;
  int  _kb_field = -1;            // 0=Name, 1=Secret

  KeyboardWidget& kb() { return _task->keyboard(); }

  void openKb(const char* initial, int max) {
    kb().begin(initial, max);
    kb().clearPlaceholders();     // literal field — {loc}/{time} make no sense here
    _kb_active = true;
  }

  // Turn the typed Secret field into a 16-byte channel secret. Returns false
  // (and leaves out[] untouched) if the field can't be parsed as configured.
  bool deriveSecret(uint8_t out[32]) const {
    if (_hex_mode) {
      if (strlen(_secret_text) != 32) return false;
      uint8_t tmp[16];
      for (int i = 0; i < 16; i++) {
        char byte_str[3] = { _secret_text[i * 2], _secret_text[i * 2 + 1], 0 };
        char* end = nullptr;
        long v = strtol(byte_str, &end, 16);
        if (*end != 0) return false;
        tmp[i] = (uint8_t)v;
      }
      memset(out, 0, 32);
      memcpy(out, tmp, 16);
      return true;
    }
    if (_secret_text[0] == '\0') return false;   // blank passphrase not allowed
    uint8_t digest[32];
    mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)_secret_text, (int)strlen(_secret_text));
    memset(out, 0, 32);
    memcpy(out, digest, 16);
    return true;
  }

  void commit() {
    if (_name[0] == '\0') { _task->showAlert("Name required", 1200); return; }
    uint8_t secret[32];
    if (!deriveSecret(secret)) {
      _task->showAlert(_hex_mode ? "Need 32 hex chars" : "Secret required", 1400);
      return;
    }
    ChannelDetails ch;
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.name, _name, sizeof(ch.name) - 1);
    memcpy(ch.channel.secret, secret, sizeof(ch.channel.secret));
    if (the_mesh.setChannelLocal((uint8_t)_idx, ch)) {
      _task->showAlert(_mode == ADD ? "Channel added" : "Channel updated", 1000);
      _mode = OFF;
    } else {
      _task->showAlert("Save failed", 1200);
    }
  }

public:
  explicit ChannelsView(UITask* task) : _task(task) {}

  bool active() const { return _mode != OFF || _kb_active; }
  void reset() { _mode = OFF; _kb_active = false; _kb_field = -1; }

  // idx = the slot to write on Save (first free slot for Add, existing slot
  // for Edit). Edit pre-fills Name; the Secret can't be redisplayed (only the
  // derived key is stored), so editing it means typing a new passphrase/hex —
  // same as re-logging into a room with a new password.
  void openAdd(int idx) {
    _mode = ADD; _idx = idx;
    _name[0] = '\0'; _secret_text[0] = '\0'; _hex_mode = false; _sel = 0;
  }
  void openEdit(int idx, const char* existing_name) {
    _mode = EDIT; _idx = idx;
    strncpy(_name, existing_name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = '\0';
    _secret_text[0] = '\0'; _hex_mode = false; _sel = 0;
  }

  int render(DisplayDriver& display) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    if (_kb_active) return kb().render(display);

    display.drawCenteredHeader(_mode == ADD ? "ADD CHANNEL" : "EDIT CHANNEL");
    const int top  = display.listStart();
    const int step = display.lineStep();
    for (int i = 0; i < 3; i++) {
      int y = top + i * step;
      bool sel = (i == _sel);
      display.drawSelectionRow(0, y - 1, display.width(), step - 1, sel);
      char row[40];
      if (i == 0)      snprintf(row, sizeof(row), "Name: %s", _name[0] ? _name : "(none)");
      else if (i == 1) snprintf(row, sizeof(row), "Secret (%s): %s",
                                _hex_mode ? "hex" : "phrase", _secret_text[0] ? _secret_text : "(none)");
      else             snprintf(row, sizeof(row), "[Save]");
      // Ellipsize rather than print() directly -- a long name/secret must not
      // wrap onto the next row's line (print() wraps by default).
      display.drawTextEllipsized(2, y, display.width() - 4, row);
      display.setColor(DisplayDriver::LIGHT);
    }
    return 1000;
  }

  bool handleInput(char c) {
    if (_kb_active) {
      auto r = kb().handleInput(c);
      if (r == KeyboardWidget::DONE) {
        if (_kb_field == 0) { strncpy(_name, kb().buf, sizeof(_name) - 1); _name[sizeof(_name) - 1] = '\0'; }
        else                { strncpy(_secret_text, kb().buf, sizeof(_secret_text) - 1); _secret_text[sizeof(_secret_text) - 1] = '\0'; }
        _kb_active = false; _kb_field = -1;
      } else if (r == KeyboardWidget::CANCELLED) {
        _kb_active = false; _kb_field = -1;
      }
      return true;
    }
    if (c == KEY_CANCEL) { _mode = OFF; return true; }
    if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : 2; return true; }
    if (c == KEY_DOWN) { _sel = (_sel < 2) ? _sel + 1 : 0; return true; }
    if ((keyIsPrev(c) || keyIsNext(c)) && _sel == 1) {
      _hex_mode = !_hex_mode;
      _secret_text[0] = '\0';   // switching mode invalidates whatever was typed
      return true;
    }
    if (c == KEY_ENTER) {
      if      (_sel == 0) { _kb_field = 0; openKb(_name, sizeof(_name) - 1); }
      else if (_sel == 1) { _kb_field = 1; openKb(_secret_text, _hex_mode ? 32 : (int)sizeof(_secret_text) - 1); }
      else                 commit();
      return true;
    }
    return true;
  }
};
