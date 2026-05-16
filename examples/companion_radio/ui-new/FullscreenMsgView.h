#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>

static const int FS_CHARS   = 21;
static const int FS_LINE_H  = 8;
static const int FS_START_Y = 12;
static const int FS_VISIBLE = (64 - FS_START_Y) / FS_LINE_H;

struct FullscreenMsgView {
  int  scroll;
  bool active;

  FullscreenMsgView() : scroll(0), active(false) {}

  enum Result { NONE, PREV, NEXT, CLOSE };

  void begin() { scroll = 0; active = true; }

  static int wrapLines(const char* text, char out[][FS_CHARS + 1], int max_lines) {
    int count = 0;
    const char* p = text;
    while (*p && count < max_lines) {
      int len = strlen(p);
      if (len <= FS_CHARS) {
        strncpy(out[count++], p, FS_CHARS);
        out[count - 1][len] = '\0';
        break;
      }
      int brk = FS_CHARS;
      for (int i = FS_CHARS - 1; i > 0; i--) {
        if (p[i] == ' ') { brk = i; break; }
      }
      strncpy(out[count], p, brk);
      out[count++][brk] = '\0';
      p += brk + (p[brk] == ' ' ? 1 : 0);
    }
    return count;
  }

  int render(DisplayDriver& display, const char* sender, const char* text,
             bool has_prev, bool has_next) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, 0, display.width(), 10);
    display.setColor(DisplayDriver::DARK);
    display.drawTextEllipsized(2, 1, display.width() - 4, sender);
    display.setColor(DisplayDriver::LIGHT);

    char trans_text[512];
    display.translateUTF8ToBlocks(trans_text, text, sizeof(trans_text));
    char lines[12][FS_CHARS + 1];
    int lcount = wrapLines(trans_text, lines, 12);
    int max_scroll = lcount > FS_VISIBLE ? lcount - FS_VISIBLE : 0;
    if (scroll > max_scroll) scroll = max_scroll;

    for (int i = 0; i < FS_VISIBLE && (scroll + i) < lcount; i++) {
      display.setCursor(0, FS_START_Y + i * FS_LINE_H);
      display.print(lines[scroll + i]);
    }
    if (scroll > 0) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, FS_START_Y, 6, FS_LINE_H);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, FS_START_Y);
      display.print("^");
    }
    if (scroll < max_scroll) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, FS_START_Y + (FS_VISIBLE - 1) * FS_LINE_H, 6, FS_LINE_H);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, FS_START_Y + (FS_VISIBLE - 1) * FS_LINE_H);
      display.print("v");
    }
    if (has_next) {
      display.setCursor(0, 56);
      display.print("<");
    }
    if (has_prev) {
      display.setCursor(display.width() - 6, 56);
      display.print(">");
    }
    return 300;
  }

  Result handleInput(char c) {
    if (c == KEY_UP)     { if (scroll > 0) scroll--; return NONE; }
    if (c == KEY_DOWN)   { scroll++; return NONE; }
    if (c == KEY_LEFT)   return NEXT;
    if (c == KEY_RIGHT)  return PREV;
    if (c == KEY_ENTER || c == KEY_CANCEL) return CLOSE;
    return NONE;
  }
};
