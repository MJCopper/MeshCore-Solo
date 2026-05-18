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

  enum Result { NONE, PREV, NEXT, CLOSE, REPLY };

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

    // detect @recipient at start of message
    char to_nick[32] = "";
    const char* body = text;
    if (text[0] == '@') {
      const char* sp = strchr(text, ' ');
      if (sp && sp[1]) {
        int len = (int)(sp - text) - 1;
        if (len >= (int)sizeof(to_nick)) len = sizeof(to_nick) - 1;
        memcpy(to_nick, text + 1, len);
        to_nick[len] = '\0';
        body = sp + 1;
      }
    }

    const int header_h = to_nick[0] ? 20 : 10;
    const int startY   = header_h + 2;
    const int visible  = (display.height() - startY - FS_LINE_H) / FS_LINE_H;

    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, 0, display.width(), header_h);
    display.setColor(DisplayDriver::DARK);
    display.drawTextEllipsized(2, 1, display.width() - 4, sender);
    if (to_nick[0]) {
      char to_label[36];
      snprintf(to_label, sizeof(to_label), "To: %s", to_nick);
      display.drawTextEllipsized(2, 11, display.width() - 4, to_label);
    }
    display.setColor(DisplayDriver::LIGHT);

    char trans_text[512];
    display.translateUTF8ToBlocks(trans_text, body, sizeof(trans_text));
    char lines[12][FS_CHARS + 1];
    int lcount = wrapLines(trans_text, lines, 12);
    int max_scroll = lcount > visible ? lcount - visible : 0;
    if (scroll > max_scroll) scroll = max_scroll;

    for (int i = 0; i < visible && (scroll + i) < lcount; i++) {
      display.setCursor(0, startY + i * FS_LINE_H);
      display.print(lines[scroll + i]);
    }
    if (scroll > 0) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, startY, 6, FS_LINE_H);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, startY);
      display.print("^");
    }
    if (scroll < max_scroll) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, startY + (visible - 1) * FS_LINE_H, 6, FS_LINE_H);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, startY + (visible - 1) * FS_LINE_H);
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
    return 2000;
  }

  Result handleInput(char c) {
    if (c == KEY_UP)          { if (scroll > 0) scroll--; return NONE; }
    if (c == KEY_DOWN)        { scroll++; return NONE; }
    if (c == KEY_LEFT)        return NEXT;
    if (c == KEY_RIGHT)       return PREV;
    if (c == KEY_CONTEXT_MENU) return REPLY;
    if (c == KEY_ENTER || c == KEY_CANCEL) return CLOSE;
    return NONE;
  }
};
