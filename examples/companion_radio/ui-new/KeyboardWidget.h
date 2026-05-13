#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>

// Layout constants shared by all keyboard users
static const char KB_CHARS[4][10] = {
  {'a','b','c','d','e','f','g','h','i','j'},
  {'k','l','m','n','o','p','q','r','s','t'},
  {'u','v','w','x','y','z','.',' ','!','?'},
  {'1','2','3','4','5','6','7','8','9','0'},
};
static const int KB_ROWS_CHAR  = 4;
static const int KB_COLS_CHAR  = 10;
static const int KB_SPECIAL    = 5;   // [^] [Sp] [Del] [{}] [OK]
static const char* KB_PH_LIST[] = { "{loc}", "{time}" };
static const int KB_PH_COUNT   = 2;
static const int KB_MAX_LEN    = 139;
static const int KB_CELL_W     = 12;  // 10 cols × 12px = 120px of 128px display
static const int KB_CELL_H     = 9;
static const int KB_TEXT_Y     = 0;
static const int KB_SEP_Y      = 9;
static const int KB_CHARS_Y    = 11;
static const int KB_SPECIAL_Y  = 48;

struct KeyboardWidget {
  char buf[KB_MAX_LEN + 1];
  int  len;
  int  max_len;
  char label[16];
  int  row, col;
  bool caps;
  bool ph_mode;
  int  ph_sel;

  enum Result { NONE, DONE, CANCELLED };

  void begin(const char* initial = "", int max = KB_MAX_LEN, const char* lbl = "") {
    strncpy(label, lbl, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
    max_len = (max > KB_MAX_LEN) ? KB_MAX_LEN : max;
    strncpy(buf, initial, max_len);
    buf[max_len] = '\0';
    len = strlen(buf);
    row = col = 0;
    caps = ph_mode = false;
    ph_sel = 0;
  }

  int render(DisplayDriver& display) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // text preview: scroll so cursor is always visible (last 20 chars)
    const char* disp_start = buf;
    int disp_len = len;
    if (disp_len > 20) { disp_start = buf + (disp_len - 20); disp_len = 20; }
    char preview[32];
    snprintf(preview, sizeof(preview), "%s%.*s_", label, disp_len, disp_start);
    display.setCursor(0, KB_TEXT_Y);
    display.print(preview);
    display.fillRect(0, KB_SEP_Y, display.width(), 1);

    // character grid
    for (int r = 0; r < KB_ROWS_CHAR; r++) {
      int y = KB_CHARS_Y + r * KB_CELL_H;
      for (int c = 0; c < KB_COLS_CHAR; c++) {
        bool sel = (row == r && col == c);
        char ch = KB_CHARS[r][c];
        if (caps && ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        char ch_buf[2] = { ch == ' ' ? '_' : ch, '\0' };
        int cx = c * KB_CELL_W;
        if (sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(cx, y - 1, KB_CELL_W - 1, KB_CELL_H);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
        }
        display.setCursor(cx + 3, y);
        display.print(ch_buf);
      }
    }

    // special row: [^] [Sp] [Del] [{}] [OK]
    const char* spec[] = { "[^]", "[Sp]", "[Del]", "[{}]", "[OK]" };
    for (int i = 0; i < KB_SPECIAL; i++) {
      bool sel    = (row == KB_ROWS_CHAR && col == i);
      bool active = (i == 0 && caps);
      int sx = i * 25;
      if (sel || active) {
        display.setColor(DisplayDriver::LIGHT);
        display.fillRect(sx, KB_SPECIAL_Y - 1, 24, KB_CELL_H);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }
      display.setCursor(sx + 1, KB_SPECIAL_Y);
      display.print(spec[i]);
      display.setColor(DisplayDriver::LIGHT);
    }

    // placeholder picker overlay
    if (ph_mode) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(20, 20, 88, 12 + KB_PH_COUNT * 10);
      display.setColor(DisplayDriver::LIGHT);
      display.drawRect(20, 20, 88, 12 + KB_PH_COUNT * 10);
      display.setCursor(24, 21);
      display.print("Placeholder:");
      display.fillRect(20, 30, 88, 1);
      for (int i = 0; i < KB_PH_COUNT; i++) {
        int py = 33 + i * 10;
        if (i == ph_sel) {
          display.setColor(DisplayDriver::LIGHT);
          display.fillRect(21, py - 1, 86, 10);
          display.setColor(DisplayDriver::DARK);
        } else {
          display.setColor(DisplayDriver::LIGHT);
        }
        display.setCursor(24, py);
        display.print(KB_PH_LIST[i]);
      }
      display.setColor(DisplayDriver::LIGHT);
    }

    return 50;
  }

  Result handleInput(char c) {
    // placeholder overlay consumes all input
    if (ph_mode) {
      if (c == KEY_UP   && ph_sel > 0)              { ph_sel--; return NONE; }
      if (c == KEY_DOWN && ph_sel < KB_PH_COUNT - 1){ ph_sel++; return NONE; }
      if (c == KEY_ENTER) {
        const char* ph = KB_PH_LIST[ph_sel];
        int ph_len = strlen(ph);
        if (len + ph_len <= max_len) {
          memcpy(buf + len, ph, ph_len);
          len += ph_len;
          buf[len] = '\0';
        }
        ph_mode = false;
        return NONE;
      }
      ph_mode = false;  // CANCEL or any other key closes the overlay
      return NONE;
    }

    if (c == KEY_CANCEL) return CANCELLED;

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
            ph_mode = true;
            ph_sel  = 0;
            break;
          case 4:
            return DONE;
        }
      }
    }
    return NONE;
  }
};
