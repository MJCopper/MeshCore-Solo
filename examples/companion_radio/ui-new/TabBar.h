#pragma once
// Shared circular tab-bar header, extracted after NearbyScreen and BotScreen
// had each independently grown byte-identical rendering code for the same
// "active tab centred as a pill, neighbours fan out either side and wrap
// around" layout (see NearbyScreen::drawFilterTabs / BotScreen::drawTabBar,
// pre-extraction). A third screen (AdminScreen) needing the same thing was
// the rule-of-three trigger to finally share it.
//
// Tab-switching itself (LEFT/RIGHT cycling the active index) is NOT part of
// this helper -- each screen's cycle has different side effects (refresh a
// list, reset a row selection, ...), so that one-liner stays inline per screen.

namespace tabbar {

// One tab: short label; the active one is an inverted pill (same look as a
// selected row) so it reads as "you are here" in the tab strip.
inline void drawPill(DisplayDriver& display, int x, int w, const char* label, bool active) {
  int lh = display.getLineHeight();
  if (active) {
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(x, 0, w, lh + 1);
    display.setColor(DisplayDriver::DARK);
  } else {
    display.setColor(DisplayDriver::LIGHT);
  }
  display.drawTextCentered(x + w / 2, 0, label);
  display.setColor(DisplayDriver::LIGHT);
}

// Truncate `label` to fit within `max_w` pixels of text (no padding), the same
// shrink-and-append-"..." approach as DisplayDriver::drawTextEllipsized (which
// can't be reused directly here: it draws immediately at a fixed x, but a tab
// pill needs the truncated width known *before* drawing, to size/centre the
// pill). Returns the resulting text's pixel width; writes the (possibly
// truncated) text into `out`.
inline int ellipsize(DisplayDriver& display, const char* label, int max_w, char* out, size_t out_sz) {
  strncpy(out, label, out_sz - 1);
  out[out_sz - 1] = '\0';
  int w = display.getTextWidth(out);
  if (w <= max_w) return w;

  const int ellipsis_w = display.getTextWidth("...");
  int len = (int)strlen(out);
  while (len > 0 && display.getTextWidth(out) > max_w - ellipsis_w) out[--len] = '\0';
  // Strip an orphaned UTF-8 lead byte left by the byte-at-a-time trim above.
  while (len > 0 && ((uint8_t)out[len - 1] & 0xC0) == 0xC0) out[--len] = '\0';
  strcat(out, "...");
  return display.getTextWidth(out);
}

// Header as a circular tab bar: the active tab sits centred as a filled pill,
// neighbours fan out either side and wrap around. A neighbour that doesn't
// fully fit in the space remaining on its side is drawn truncated with an
// ellipsis instead of being skipped, so the immediate left/right tabs stay
// visible (just shortened) rather than vanishing on a narrow display or a
// long active label -- only once even an ellipsis wouldn't fit does that side
// stop. `right_reserve` keeps any trailing decoration the caller draws
// afterwards (a context-menu hint, a counter) clear of the rightmost tab.
// Also draws the header separator line.
inline void draw(DisplayDriver& display, const char* const* labels, int count, int active, int right_reserve = 0) {
  const int pad = 3, gap = 2;
  const int min_w = pad * 2 + 6;   // smallest pill worth drawing (padding + a couple px of text)
  char buf[24];

  int aw = display.getTextWidth(labels[active]) + pad * 2;
  int ax = display.width() / 2 - aw / 2;
  drawPill(display, ax, aw, labels[active], true);

  int lx = ax - gap;
  int rx = ax + aw + gap;
  int rx_limit = display.width() - right_reserve;
  bool lfit = true, rfit = true;
  for (int k = 1; k <= count / 2 && (lfit || rfit); k++) {
    int li = (active - k + count) % count;
    int ri = (active + k) % count;
    if (lfit) {
      int avail = lx;   // space from x=0 to the running left edge
      if (avail >= min_w) {
        int tw = ellipsize(display, labels[li], avail - pad * 2, buf, sizeof(buf));
        int w = tw + pad * 2;
        drawPill(display, lx - w, w, buf, false);
        lx = lx - w - gap;
      } else lfit = false;
    }
    // Skip the right side when it lands on the same tab as the left (the
    // single opposite tab on an even count) so it isn't drawn twice.
    if (rfit && ri != li) {
      int avail = rx_limit - rx;
      if (avail >= min_w) {
        int tw = ellipsize(display, labels[ri], avail - pad * 2, buf, sizeof(buf));
        int w = tw + pad * 2;
        drawPill(display, rx, w, buf, false);
        rx += w + gap;
      } else rfit = false;
    }
  }
  display.setColor(DisplayDriver::LIGHT);
  display.fillRect(0, display.headerH() - display.sepH(), display.width(), display.sepH());
}

}  // namespace tabbar
