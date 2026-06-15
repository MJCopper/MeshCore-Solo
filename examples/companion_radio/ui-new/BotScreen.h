#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp after KeyboardWidget.h is defined.

class BotScreen : public UIScreen {
  UITask*    _task;
  NodePrefs* _prefs;

  // Items: 0=Enable(DM), 1=Channel, 2=Trigger DM, 3=Reply DM, 4=Trigger Ch,
  //        5=Reply Ch, 6=Commands, 7=Quiet from, 8=Quiet to
  static const int ITEM_COUNT = 9;

  int  _sel;
  int  _scroll;    // index of first visible item
  int  _visible;   // items fitting on screen; updated each render, used by handleInput
  bool _dirty;

  // keyboard state (reused for trigger and reply fields)
  int             _kb_field;   // -1=off, else the item index being edited (2..5)
  KeyboardWidget* _kb;

  // channel cache (refreshed on enter)
  int     _num_channels;
  uint8_t _channel_indices[MAX_GROUP_CHANNELS];

  void refreshChannels() {
    _num_channels = 0;
    ChannelDetails ch;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      if (the_mesh.getChannel(i, ch) && ch.name[0] != '\0')
        _channel_indices[_num_channels++] = (uint8_t)i;
    }
  }

  // Keep the selected row inside the visible window (_visible set by render()).
  void scrollToSel() {
    if (_sel < _scroll) _scroll = _sel;
    else if (_sel >= _scroll + _visible) _scroll = _sel - _visible + 1;
  }

  // Returns index into _channel_indices[] for current bot_channel_idx, or -1 if not found/disabled.
  int currentChannelListIdx() const {
    if (!_prefs->bot_channel_enabled) return -1;
    for (int i = 0; i < _num_channels; i++)
      if (_channel_indices[i] == _prefs->bot_channel_idx) return i;
    return -1;
  }

public:
  BotScreen(UITask* task, NodePrefs* prefs, KeyboardWidget* kb) : _task(task), _prefs(prefs), _kb(kb) {}

  void enter() {
    _sel      = 0;
    _scroll   = 0;
    _visible  = 1;
    _kb_field = -1;
    _dirty    = false;
    refreshChannels();
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    if (_kb_field >= 0) {
      return _kb->render(display);
    }

    int val_x = display.valCol();

    display.drawCenteredHeader("AUTO-REPLY BOT");
    // reply counter, right-aligned in the header
    uint16_t sent = the_mesh.botReplyCount();
    if (sent > 0) {
      char cbuf[12];
      snprintf(cbuf, sizeof(cbuf), "%u", (unsigned)sent);
      int cw = display.getTextWidth(cbuf);
      display.setCursor(display.width() - cw - 1, 0);
      display.print(cbuf);
    }

    static const char* labels[] = { "Enable", "Channel", "Trigger DM", "Reply DM",
                                    "Trigger Ch", "Reply Ch", "Commands",
                                    "Quiet from", "Quiet to" };
    _visible = drawList(display, ITEM_COUNT, _sel, _scroll, [&](int i, int y, bool sel, int reserve) {
      display.drawSelectionRow(0, y - 1, display.width() - reserve, display.lineStep() - 1, sel);
      display.setCursor(2, y);
      display.print(labels[i]);
      display.setCursor(val_x, y);

      if (i == 0) {
        display.print(_prefs->bot_enabled ? "ON" : "OFF");
      } else if (i == 1) {
        if (!_prefs->bot_channel_enabled || _num_channels == 0) {
          display.print("OFF");
        } else {
          ChannelDetails ch;
          if (the_mesh.getChannel(_prefs->bot_channel_idx, ch) && ch.name[0])
            display.drawTextEllipsized(val_x, y, display.width() - val_x - 1 - reserve,ch.name);
          else
            display.print("?");
        }
      } else if (i == 2 || i == 4) {
        const char* tr = (i == 2) ? _prefs->bot_trigger : _prefs->bot_trigger_ch;
        const char* shown = !tr[0] ? "(none)"
                          : (tr[0] == '*' && !tr[1]) ? "(any msg)"   // wildcard / away mode
                                                     : tr;
        display.drawTextEllipsized(val_x, y, display.width() - val_x - 1 - reserve,shown);
      } else if (i == 3 || i == 5) {
        const char* rp = (i == 3) ? _prefs->bot_reply_dm : _prefs->bot_reply_ch;
        display.drawTextEllipsized(val_x, y, display.width() - val_x - 1 - reserve,rp[0] ? rp : "(none)");
      } else if (i == 6) {
        display.print(_prefs->bot_commands_enabled ? "ON" : "OFF");
      } else {  // i == 7 (quiet from) or i == 8 (quiet to)
        bool off = (_prefs->bot_quiet_start == _prefs->bot_quiet_end);
        if (off) {
          display.print("OFF");
        } else {
          char hb[8];
          snprintf(hb, sizeof(hb), "%02d:00", i == 7 ? _prefs->bot_quiet_start : _prefs->bot_quiet_end);
          display.print(hb);
        }
      }
      display.setColor(DisplayDriver::LIGHT);
    });
    return 2000;
  }

  bool handleInput(char c) override {
    bool up    = (c == KEY_UP);
    bool down  = (c == KEY_DOWN);
    bool left  = keyIsPrev(c);
    bool right = keyIsNext(c);
    bool enter = (c == KEY_ENTER);
    bool cancel = (c == KEY_CANCEL || c == KEY_CONTEXT_MENU);

    if (_kb_field >= 0) {
      auto res = _kb->handleInput(c);
      if (res == KeyboardWidget::DONE) {
        char* dst = fieldBuf(_kb_field);
        int   cap = fieldCap(_kb_field);
        if (dst) { strncpy(dst, _kb->buf, cap - 1); dst[cap - 1] = '\0'; }
        _dirty    = true;
        _kb_field = -1;
      } else if (res == KeyboardWidget::CANCELLED) {
        _kb_field = -1;
      }
      return true;
    }

    if (cancel) {
      if (_dirty) the_mesh.savePrefs();
      _task->gotoToolsScreen();
      return true;
    }
    if (up   && _sel > 0)              { _sel--; scrollToSel(); return true; }
    if (down && _sel < ITEM_COUNT - 1) { _sel++; scrollToSel(); return true; }

    if (_sel == 0 && (enter || left || right)) {
      _prefs->bot_enabled ^= 1;
      _dirty = true;
      return true;
    }
    if (_sel == 1) {
      if (_num_channels == 0) return false;
      // Cycle: OFF → ch[0] → ch[1] → ... → OFF
      int idx = currentChannelListIdx();  // -1 = OFF
      if (right || enter) {
        idx++;
        if (idx >= _num_channels) idx = -1;  // wrap to OFF
      }
      if (left) {
        idx--;
        if (idx < -1) idx = _num_channels - 1;
      }
      if (left || right || enter) {
        if (idx < 0) {
          _prefs->bot_channel_enabled = 0;
        } else {
          _prefs->bot_channel_enabled = 1;
          _prefs->bot_channel_idx = _channel_indices[idx];
        }
        _dirty = true;
        return true;
      }
    }
    if (_sel == 6 && (enter || left || right)) {
      _prefs->bot_commands_enabled ^= 1;
      _dirty = true;
      return true;
    }
    if ((_sel == 7 || _sel == 8) && (left || right)) {
      uint8_t& h = (_sel == 7) ? _prefs->bot_quiet_start : _prefs->bot_quiet_end;
      h = right ? (h + 1) % 24 : (h + 23) % 24;
      _dirty = true;
      return true;
    }
    if ((_sel == 2 || _sel == 3 || _sel == 4 || _sel == 5) && enter) {
      _kb_field = _sel;
      _kb->begin(fieldBuf(_sel), fieldCap(_sel) - 1);
      bool is_trigger = (_sel == 2 || _sel == 4);
      if (is_trigger) _kb->clearPlaceholders();  // trigger is literal — placeholders never match
      else            kbAddSensorPlaceholders(*_kb, &sensors);
      return true;
    }
    return false;
  }

private:
  // Maps a keyboard-editable item index to its backing string + capacity.
  char* fieldBuf(int item) {
    switch (item) {
      case 2: return _prefs->bot_trigger;
      case 3: return _prefs->bot_reply_dm;
      case 4: return _prefs->bot_trigger_ch;
      case 5: return _prefs->bot_reply_ch;
      default: return nullptr;
    }
  }
  int fieldCap(int item) {
    switch (item) {
      case 2: return sizeof(_prefs->bot_trigger);
      case 3: return sizeof(_prefs->bot_reply_dm);
      case 4: return sizeof(_prefs->bot_trigger_ch);
      case 5: return sizeof(_prefs->bot_reply_ch);
      default: return 0;
    }
  }
};
