#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>

static const int FS_CHARS_MAX = 80;  // max bytes per wrapped line

struct FullscreenMsgView {
  int  scroll;
  bool active;

  FullscreenMsgView() : scroll(0), active(false) {}

  enum Result { NONE, PREV, NEXT, CLOSE, REPLY };

  void begin() { scroll = 0; active = true; }

  // Pixel-accurate word-wrap: breaks at last space that fits within max_px,
  // hard-breaks long words. Works for both fixed- and variable-width fonts.
  static int wrapLines(DisplayDriver& display, const char* text, int max_px,
                       char out[][FS_CHARS_MAX], int max_lines) {
    int count = 0;
    const char* seg = text;
    while (*seg && count < max_lines) {
      const char* p       = seg;
      const char* last_sp = nullptr;
      const char* last_fit = seg;

      while (*p && (p - seg) < FS_CHARS_MAX - 1) {
        uint8_t b0 = (uint8_t)*p;
        int cb = (b0 < 0x80) ? 1 : (b0 < 0xE0) ? 2 : (b0 < 0xF0) ? 3 : 4;
        if ((p - seg) + cb >= FS_CHARS_MAX) break;

        char buf[FS_CHARS_MAX];
        int n = (int)(p - seg) + cb;
        memcpy(buf, seg, n); buf[n] = '\0';
        if (display.getTextWidth(buf) > max_px && p > seg) break;

        if (*p == ' ') last_sp = p;
        p       += cb;
        last_fit = p;
      }

      const char* end;
      const char* next_seg;
      if (!*p) {
        end = p; next_seg = p;
      } else if (last_sp && last_sp > seg) {
        end = last_sp; next_seg = last_sp + 1;
      } else {
        end = last_fit; next_seg = last_fit;
      }

      int len = (int)(end - seg);
      if (len <= 0) { seg = *seg ? seg + 1 : seg; continue; }
      if (len > FS_CHARS_MAX - 1) len = FS_CHARS_MAX - 1;
      memcpy(out[count], seg, len);
      out[count][len] = '\0';
      count++;
      seg = next_seg;
    }
    return count;
  }

  int render(DisplayDriver& display, const char* sender, const char* text,
             bool has_prev, bool has_next) {
    display.setTextSize(1);
    const int lineH  = display.getLineHeight();
    const int max_px = display.width() - 6;

    // detect @recipient at start of message
    char to_nick[32] = "";
    const char* body = text;
    if (text[0] == '@' && text[1] == '[') {
      const char* close = strchr(text + 2, ']');
      if (close && close[1] == ' ' && close[2]) {
        int len = (int)(close - text) - 2;
        if (len >= (int)sizeof(to_nick)) len = sizeof(to_nick) - 1;
        memcpy(to_nick, text + 2, len);
        to_nick[len] = '\0';
        body = close + 2;
      }
    }

    const int header_h = to_nick[0] ? 20 : 10;
    const int startY   = header_h + 2;
    const int visible  = (display.height() - startY - lineH) / lineH;

    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, 0, display.width(), header_h);
    display.setColor(DisplayDriver::DARK);
    display.drawTextEllipsized(2, 1, display.width() - 4, sender);
    if (to_nick[0]) {
      char trans_nick[32], to_label[36];
      display.translateUTF8ToBlocks(trans_nick, to_nick, sizeof(trans_nick));
      snprintf(to_label, sizeof(to_label), "To: %s", trans_nick);
      display.drawTextEllipsized(2, 11, display.width() - 4, to_label);
    }
    display.setColor(DisplayDriver::LIGHT);

    char trans_text[512];
    display.translateUTF8ToBlocks(trans_text, body, sizeof(trans_text));
    char lines[12][FS_CHARS_MAX];
    int lcount = wrapLines(display, trans_text, max_px, lines, 12);
    int max_scroll = lcount > visible ? lcount - visible : 0;
    if (scroll > max_scroll) scroll = max_scroll;

    for (int i = 0; i < visible && (scroll + i) < lcount; i++) {
      display.setCursor(0, startY + i * lineH);
      display.print(lines[scroll + i]);
    }
    if (scroll > 0) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, startY, 6, lineH);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, startY);
      display.print("^");
    }
    if (scroll < max_scroll) {
      display.setColor(DisplayDriver::DARK);
      display.fillRect(display.width() - 6, startY + (visible - 1) * lineH, 6, lineH);
      display.setColor(DisplayDriver::LIGHT);
      display.setCursor(display.width() - 6, startY + (visible - 1) * lineH);
      display.print("v");
    }
    const int nav_y = display.height() - lineH;
    if (has_next) {
      display.setCursor(0, nav_y);
      display.print("<");
    }
    if (has_prev) {
      display.setCursor(display.width() - 6, nav_y);
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
