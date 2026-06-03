#pragma once
// Small 1-px drawing primitives the DisplayDriver abstraction doesn't provide
// (no line / circle in the GFX wrapper used here). Shared by the map and the
// compass so the Bresenham / midpoint routines aren't copy-pasted per screen.
// Everything is plotted as 1x1 fillRect to stay within DisplayDriver's API.

#include <helpers/ui/DisplayDriver.h>
#include <stdlib.h>   // abs

namespace gfx {

// Bresenham line between two points.
static inline void drawLine(DisplayDriver& d, int x0, int y0, int x1, int y1) {
  int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    d.fillRect(x0, y0, 1, 1);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Midpoint-circle outline.
static inline void drawCircle(DisplayDriver& d, int cx, int cy, int r) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    d.fillRect(cx + x, cy + y, 1, 1); d.fillRect(cx + y, cy + x, 1, 1);
    d.fillRect(cx - y, cy + x, 1, 1); d.fillRect(cx - x, cy + y, 1, 1);
    d.fillRect(cx - x, cy - y, 1, 1); d.fillRect(cx - y, cy - x, 1, 1);
    d.fillRect(cx + y, cy - x, 1, 1); d.fillRect(cx + x, cy - y, 1, 1);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

}  // namespace gfx
