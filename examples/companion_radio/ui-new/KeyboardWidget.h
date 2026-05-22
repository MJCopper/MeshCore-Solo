#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>
#include "PopupMenu.h"

// Layout constants shared by all keyboard users
static const char KB_CHARS[4][10] = {
  {'a','b','c','d','e','f','g','h','i','j'},
  {'k','l','m','n','o','p','q','r','s','t'},
  {'u','v','w','x','y','z','.',' ','!','?'},
  {'1','2','3','4','5','6','7','8','9','0'},
};
static const int KB_ROWS_CHAR  = 4;
static const int KB_COLS_CHAR  = 10;
static const int KB_SPECIAL    = 5;   // [^] [Sp] [De] [{}] [OK]
static const int KB_MAX_LEN    = 139;

static const int KB_PH_MAX     = 12;  // max placeholders in list
static const int KB_PH_LEN     = 9;   // max placeholder string length incl. null
static const int KB_PH_VISIBLE = 3;   // items shown at once in overlay

struct KeyboardWidget {
  char buf[KB_MAX_LEN + 1];
  int  len;
  int  max_len;
  int  row, col;
  bool caps;
  char _ph_buf[KB_PH_MAX][KB_PH_LEN];
  int  _ph_count;
  PopupMenu _ph_menu;

  enum Result { NONE, DONE, CANCELLED };

  void begin(const char* initial = "", int max = KB_MAX_LEN) {
    max_len = (max > KB_MAX_LEN) ? KB_MAX_LEN : max;
    strncpy(buf, initial, max_len);
    buf[max_len] = '\0';
    len = strlen(buf);
    row = col = 0;
    caps = false;
    _ph_menu.active = false;
    // default placeholders — always available
    _ph_count = 0;
    addPlaceholder("{loc}");
    addPlaceholder("{time}");
  }

  void addPlaceholder(const char* ph) {
    if (_ph_count < KB_PH_MAX) {
      strncpy(_ph_buf[_ph_count], ph, KB_PH_LEN - 1);
      _ph_buf[_ph_count][KB_PH_LEN - 1] = '\0';
      _ph_count++;
    }
  }

  int render(DisplayDriver& display) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    const int lh      = display.getLineHeight();
    const int cw      = display.getCharWidth();
    const int cell_w  = display.width() / KB_COLS_CHAR;
    const int sep_y   = lh;           // tight separator: preview row height only
    const int chars_y = sep_y + 1;
    const int cell_h  = (display.height() - chars_y) / (KB_ROWS_CHAR + 1);
    const int spec_y  = chars_y + KB_ROWS_CHAR * cell_h;
    const int spec_w  = display.width() / KB_SPECIAL;

    // text preview: last N chars + cursor (fit within screen width)
    int max_preview = display.width() / cw - 1;
    if (max_preview > 30) max_preview = 30;
    const char* disp_start = buf;
    int disp_len = len;
    if (disp_len > max_preview) { disp_start = buf + (disp_len - max_preview); disp_len = max_preview; }
    char preview[32];
    snprintf(preview, sizeof(preview), "%.*s_", disp_len, disp_start);
    display.setCursor(0, 0);
    display.print(preview);
    display.fillRect(0, sep_y, display.width(), 1);

    // character grid
    for (int r = 0; r < KB_ROWS_CHAR; r++) {
      int y = chars_y + r * cell_h;
      for (int c = 0; c < KB_COLS_CHAR; c++) {
        bool sel = (row == r && col == c);
        char ch = KB_CHARS[r][c];
        if (caps && ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        char ch_buf[2] = { ch == ' ' ? '_' : ch, '\0' };
        int cx = c * cell_w;
        if (sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(cx, y - 1, cell_w - 1, cell_h);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
        }
        display.setCursor(cx + (cell_w - cw) / 2, y);
        display.print(ch_buf);
      }
    }

    // special row: [^] [Sp] [Dl] [{}] [OK]
    const char* spec[] = { "[^]", "[Sp]", "[Dl]", "[{}]", "[OK]" };
    for (int i = 0; i < KB_SPECIAL; i++) {
      bool sel    = (row == KB_ROWS_CHAR && col == i);
      bool active = (i == 0 && caps);
      int sx = i * spec_w;
      if (sel || active) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(sx, spec_y - 1, spec_w - 1, cell_h);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }
      int tw = display.getTextWidth(spec[i]);
      display.setCursor(sx + (spec_w - tw) / 2, spec_y);
      display.print(spec[i]);
      display.setColor(DisplayDriver::LIGHT);
    }

    // placeholder picker overlay (drawn on top of keyboard)
    if (_ph_menu.active) _ph_menu.render(display);

    return 50;
  }

  Result handleInput(char c) {
    // placeholder overlay consumes all input
    if (_ph_menu.active) {
      auto res = _ph_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int idx = _ph_menu.selectedIndex();
        const char* ph = _ph_buf[idx];
        int ph_len = strlen(ph);
        if (len + ph_len <= max_len) {
          memcpy(buf + len, ph, ph_len);
          len += ph_len;
          buf[len] = '\0';
        }
      }
      return NONE;
    }

    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) return CANCELLED;

    if (c == KEY_UP) {
      if (row > 0) {
        row--;
        if (row == KB_ROWS_CHAR - 1)  // leaving special row upward
          col = col * KB_COLS_CHAR / KB_SPECIAL;
      }
      return NONE;
    }
    if (c == KEY_DOWN) {
      if (row < KB_ROWS_CHAR) {
        row++;
        if (row == KB_ROWS_CHAR)  // entering special row
          col = col * KB_SPECIAL / KB_COLS_CHAR;
      }
      return NONE;
    }
    if (c == KEY_LEFT) {
      if (col > 0) col--;
      return NONE;
    }
    if (c == KEY_RIGHT) {
      int max_col = (row == KB_ROWS_CHAR) ? KB_SPECIAL - 1 : KB_COLS_CHAR - 1;
      if (col < max_col) col++;
      return NONE;
    }
    if (c == KEY_ENTER) {
      if (row < KB_ROWS_CHAR) {
        if (len < max_len) {
          char ch = KB_CHARS[row][col];
          if (caps && ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
          buf[len++] = ch;
          buf[len] = '\0';
        }
      } else {
        switch (col) {
          case 0: caps = !caps; break;
          case 1:
            if (len < max_len) { buf[len++] = ' '; buf[len] = '\0'; }
            break;
          case 2:
            if (len > 0) buf[--len] = '\0';
            break;
          case 3:
            _ph_menu.begin("Placeholder:", KB_PH_VISIBLE);
            for (int i = 0; i < _ph_count; i++) _ph_menu.addItem(_ph_buf[i]);
            break;
          case 4:
            return DONE;
        }
      }
    }
    return NONE;
  }
};
