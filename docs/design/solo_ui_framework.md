# Solo UI framework — a guide for adding features

[Go back](../../README.md)

This is a developer guide to the reusable building blocks behind the
`companion_radio` **solo** firmware UI (the `ui-new` screens). It is not a
user manual — for what each screen *does*, see [solo_features](../solo_features/).
The goal here is so that adding a new screen or feature means *wiring together
existing helpers*, not reinventing list scrolling, text wrapping, or persistence.

Everything below lives under `examples/companion_radio/` unless a path says
otherwise. The screen fragments (`ui-new/*.h`) are all `#include`d, in order,
into one translation unit (`ui-new/UITask.cpp`) — so a `static inline` helper in
an earlier header is visible to later ones. Header-include order in `UITask.cpp`
therefore matters; new screens go near the others.

> **Single-TU only.** These fragments compile *only* as part of `UITask.cpp`.
> Some define external-linkage symbols at file scope (e.g.
> `NearbyScreen::FILTER_LABELS`), so including a fragment from a second `.cpp`
> is a duplicate-symbol link error. Anything genuinely shared across TUs must
> live in a real header (`icons.h`, `GeoUtils.h`, `DisplayDriver.h`), not a
> screen fragment.

---

## 1. The screen model

Every screen implements `UIScreen` (`src/helpers/ui/UIScreen.h`):

```cpp
class UIScreen {
public:
  virtual int  render(DisplayDriver& display) = 0;  // returns ms until the next render
  virtual bool handleInput(char c) { return false; }
  virtual void poll() { }
  virtual void onShow() { }                         // reset per-visit state
};
```

- **`render()`** draws one frame and returns *how long until it wants to be
  drawn again*, in milliseconds. Return a big number (`2000`) for a static
  screen, a small one (`50`–`200`) while something animates or a popup is open.
  This return value is the main lever for the e-ink cost/latency trade-off — see
  §9. `UITask` owns `startFrame()`/`endFrame()`; **`render()` must not call them.**
- **`handleInput(c)`** gets one key (`KEY_*`, see §7). Return `true` if consumed.
- **`poll()`** runs every loop tick regardless of focus — rare, for background
  housekeeping (e.g. the shutdown button).
- **`onShow()`** is called by `setCurrScreen()` every time the screen becomes
  current — override it to reset per-visit state (`_sel = 0`, `_dirty = false`,
  sub-views). Default no-op for screens that keep state across visits. Because
  it's invoked centrally, a navigator can't forget to reset on show.

### Wiring a screen into UITask

1. Add a `UIScreen* my_screen;` member in `UITask.h` (near the others).
2. Construct it in `UITask::begin()` (`UITask.cpp`):
   `my_screen = new MyScreen(this, …);`
3. Add a navigator — usually just the one line (the cast-free `onShow()` runs
   inside `setCurrScreen`):
   ```cpp
   void UITask::gotoMyScreen() { setCurrScreen(my_screen); }
   ```
   Only screens needing a *parameter* at entry add a typed call after it (e.g.
   `gotoRingtoneEditor` → `selectSlot(slot)`, `gotoMapScreen` → `showMapView()`).
4. Reach it from somewhere — usually a row in `ToolsScreen.h` (add an `Action`
   enum value, a row in the right section table, and a `dispatch()` case).

That's the whole contract. The constructor takes `UITask* task` plus whatever
it needs (`NodePrefs*`, `KeyboardWidget*`, …); the task back-pointer is how a
screen calls shared services (`_task->showAlert(...)`, `_task->waypoints()`, …).

---

## 2. Layout metrics & text (DisplayDriver)

`DisplayDriver` (`src/helpers/ui/DisplayDriver.h`) abstracts OLED vs e-ink and,
crucially, font scale: landscape e-ink renders text at 2×, so **never hard-code
pixel sizes** — derive everything from these:

| Call | Meaning |
| --- | --- |
| `getLineHeight()` | pixel rows per text line (8 at 1×, 16 at 2×) |
| `lineStep()` | row pitch = line height + gap; use for row `y` stepping |
| `getCharWidth()` / `getTextWidth(s)` | advance width; `getTextWidth` is font-accurate |
| `headerH()` / `listStart()` | title-bar height / first content row `y` |
| `listVisible(itemH)` | how many rows fit below the header |
| `valCol()` | conventional x for a right-hand value column |
| `width()` / `height()` | panel size in px |
| `isEink()` | true only on landscape e-ink; branch on this, not on pixel counts |

Drawing helpers (all clip/measure for you):

- `drawCenteredHeader(title)` — plain centered title + separator.
- `drawInvertedHeader(label)` — filled title bar (used by detail views).
- `drawSelectionRow(x, y, w, h, sel)` — the highlight bar behind a list row.
- `drawTextEllipsized(x, y, max_w, str)` — truncates with `…`; **use this for
  any user string** (names, labels) so long/UTF-8 text can't overrun.
- `drawTextCentered(mid_x, y, str)`.
- `translateUTF8ToBlocks(dst, src, n)` — map UTF-8 to the panel's glyph set for
  *display only*. Never run text through it before sending it over the air or
  storing it (it is lossy) — see the reply-prefix note in §5.

---

## 3. Lists — `drawList`

`drawList` (`ui-new/icons.h`) is the workhorse for any scrolling list. It
computes the visible window from font metrics, keeps `sel` in view, reserves the
scrollbar column, draws each visible row through your callback, and draws the
indicator:

```cpp
drawList(display, count, _sel, _scroll, [&](int idx, int y, bool sel, int reserve) {
  display.drawSelectionRow(0, y - 1, display.width() - reserve, display.lineStep() - 1, sel);
  display.drawTextEllipsized(2, y, display.width() - reserve - 4, items[idx].name);
});
```

`reserve` is the width the scrollbar took (0 when the list fits) — subtract it
from any right-aligned content so nothing slides under the indicator. The row
callback owns its own selection bar (so a screen can style it). For the
fold-in-place pattern (sections that expand/collapse) use `AccordionList`
instead (`ui-new/AccordionList.h`) — same idea, two callbacks (header + item).

Standalone scroll indicators (`drawScrollIndicator`, `…Px`) and the reserve
calculator (`scrollIndicatorReserve`) are exposed for hand-laid lists.

---

## 4. Reusable components

| Component | Header | Use for |
| --- | --- | --- |
| `PopupMenu` | `PopupMenu.h` | a modal action menu over any screen |
| `AccordionList` | `AccordionList.h` | collapsible sectioned lists (Tools, Settings) |
| `KeyboardWidget` | `KeyboardWidget.h` | on-screen text entry |
| `DigitEditor` | `DigitEditor.h` | scroll-edit one number, digit by digit |
| `FullscreenMsgView` | `FullscreenMsgView.h` | scrollable full-message reader + word wrap |
| `NavView` | `NavView.h` | bearing/distance/ETA "navigate to a point" view |

All follow the same shape: a `begin(...)` to open, an `active` flag, a
`handleInput(c)` returning a small `Result` enum, and a `render()`/`draw()`.
Typical embedding:

```cpp
if (_menu.active) {                       // popup eats input while open
  auto r = _menu.handleInput(c);
  if (r == PopupMenu::SELECTED) runAction(_menu.selectedIndex());
  return true;
}
...
_menu.begin("Options", 6);                // open it
_menu.addItem("Navigate"); _menu.addItem("Ping");
_menu.active = true;
```

`KeyboardWidget` additionally supports **placeholders** (`{loc}`, `{time}`,
sensor tokens) via `addPlaceholder()` / `clearPlaceholders()`; the shared
`kbAddSensorPlaceholders()` (`ui-new/SensorPlaceholders.h`) adds only the tokens
the board's sensors actually provide. Expand them with `expandMsg()` at send time.

`FullscreenMsgView::wrapLines()` is a standalone pixel-accurate word-wrapper
(O(n), variable-width-font aware) reusable by any multi-line layout; it writes
into the shared `s_wrap_trans` / `s_wrap_lines` scratch (single-threaded render,
never held across a yield — see §9).

---

## 5. Domain helpers

**Geo (`GeoUtils.h`, namespace `geo`, all pure/header-inline):**

- `haversineKm(lat1,lon1,lat2,lon2)`, `bearingDeg(...)`, `bearingCardinal(deg)`.
- `fmtDist(buf,n,km,imperial)` — "850m"/"2.3km" or feet/miles.
- `fmtAgeShort(buf,n,now,ts)` — compact "12s"/"5m"/"3h"/"2d" tag, "" for unknown.
  **This is the one age formatter** — don't reimplement the s/m/h ladder.
- `parseLatLon(text, lat, lon, label?, n?)` — pull a `lat,lon` out of message
  text; reads the `[WAY]` label if tagged.
- `parseLocShare(text, lat, lon)` — true only for an explicit `[LOC]` share.

Coordinates are **int32 degrees × 1e6** everywhere (GPS, contacts, trail,
prefs). The message tags are `LOCATION_MSG_TAG` (`[LOC]`, the sender's own live
position) and `WAYPOINT_MSG_TAG` (`[WAY]`, a saved point to share); both stay
human-readable on clients that don't know them.

**State stores:** `TrailStore` (`Trail.h`, GPS breadcrumb ring + GPX export),
`LiveTrackStore` (`LiveTrack.h`, RAM table of others' `[LOC]` positions, expiring),
`WaypointStore` (`Waypoint.h`, persisted saved points). Reach them via the task
(`_task->trail()`, `_task->liveTrack()`, `_task->waypoints()`).

**Message reply prefix:** `msgReplyBody(text, nick?, n?)` (`FullscreenMsgView.h`)
parses a leading `@[nick] ` reply marker, returning the body and optionally the
addressee. Use it instead of re-scanning for `@[`. The stored/sent prefix is raw
UTF-8 (it goes over the air) — never transliterate it.

---

## 6. Mini-icons & the status bar

Small glyphs are authored as ASCII art and packed at compile time
(`ui-new/icons.h`):

```cpp
MINI_ICON(ICON_FOO, 5,
  packRow("..#.."),
  packRow(".###."),
  packRow("#####"));
```

Draw with `miniIconDraw(display, x, topY, ICON_FOO)` (auto-scaled & centered),
`miniIconDrawTop` (exact placement), or the boxed/slot variants
(`drawBoxedIcon` = lit when active, `drawSlotIcon` = plain). Bigger page glyphs
use `BIG_ICON` / `bigIconDraw`. The home status bar composes these right-to-left
with a `blinkOn()` cadence for "leave it on and forget" broadcasts (auto-advert,
Live Share, trail, repeater) — follow that pattern when adding an indicator:
always shown on e-ink, blinking on OLED.

---

## 7. Input

Keys arrive as the `KEY_*` codes in `UIScreen.h`: `KEY_UP/DOWN/LEFT/RIGHT`,
`KEY_ENTER`, `KEY_CANCEL`, `KEY_CONTEXT_MENU` (the "Hold Enter" menu key).

Use the **prev/next convention** for value changes so the rotary encoder and the
D-pad agree: `keyIsPrev(c)` (LEFT or encoder-prev) and `keyIsNext(c)` (RIGHT or
encoder-next). `KEY_CANCEL` and `KEY_CONTEXT_MENU` stay screen-specific. Joystick
rotation is handled upstream (`rotateJoystickKey`) — screens see already-rotated
keys.

---

## 8. Persistence

Device settings live in one `NodePrefs` struct (`NodePrefs.h`), saved via
`the_mesh.savePrefs()` and loaded by `DataStore.cpp`. Rules when adding a field:

- **Append only**, and bump `NodePrefs::SCHEMA_SENTINEL`. Serialization is
  binary-positional, so order is the on-disk format; never insert in the middle.
- Add a matching `rd(...)` in `DataStore::loadPrefsInt()` and a `file.write(...)`
  in `savePrefs()`, in the same position, and **clamp on load** (an upgrader's
  file lacks the field and reads stray bytes — clamp to a sane default). Saves
  are atomic (temp-file + rename), so a crash mid-save can't corrupt settings.

**The `_dirty` convention:** a multi-field editor screen mutates `_node_prefs`
live for instant feedback but only calls `savePrefs()` once, on exit, gated by a
`_dirty` flag — so LEFT/RIGHT value-cycling doesn't thrash flash. A one-shot
action from a popup (no exit hook) saves immediately. Follow whichever matches
your screen.

**The shared "active target"** (Locator/Nav destination) is set through
`UITask::setTarget()` (defines it), `setTargetNow()` (defines + saves + toast),
or `clearTarget()` — one definition used by the Locator screen, the map, and the
Nearby/Waypoints "Set as target" actions. Resolve a person's current position
with `resolvePersonPos()` (live `[LOC]` share, else last-advertised fix).

---

## 9. Conventions & gotchas

- **Single-threaded render.** Rendering and input run on one thread, so shared
  static scratch (`s_wrap_*`) is safe *as long as it's never held across a
  yield*. Don't add scratch that outlives one `render()`.
- **e-ink blocks.** `endFrame()` on e-ink stalls the main loop for hundreds of
  ms. Keep `render()`'s return value honest so the panel isn't redrawn more than
  needed, and don't depend on `loop()` cadence for timing that must be exact
  (the ringtone player moved to a hardware timer for this reason).
- **Reference cleanup.** Anything that remembers a contact by pubkey (favourite
  slot, Locator/Live-Share target, per-contact mute/melody) or a channel by
  index must drop that reference when the entity goes away — hook
  `UITask::onContactRemoved()` / `onChannelRemoved()`. New per-contact or
  per-channel state should clear there too.
- **Toasts:** `_task->showAlert("msg", duration_ms)` overlays a transient banner
  over any screen; no redraw plumbing needed.
- **Strings:** always `strncpy`+NUL or `snprintf`; treat every name/label as
  untrusted-length and render through `drawTextEllipsized`.

---

## 10. Worked example — a new Tools screen

```cpp
// ui-new/MyToolScreen.h  — included by UITask.cpp near the other screens
#pragma once
#include "icons.h"          // drawList + mini-icons
#include "../NodePrefs.h"

class MyToolScreen : public UIScreen {
  UITask*    _task;
  NodePrefs* _prefs;
  int        _sel = 0, _scroll = 0;
  bool       _dirty = false;
  static const int ROWS = 3;
public:
  MyToolScreen(UITask* t, NodePrefs* p) : _task(t), _prefs(p) {}
  void onShow() override { _sel = 0; _scroll = 0; _dirty = false; }

  int render(DisplayDriver& d) override {
    d.setTextSize(1);
    d.drawCenteredHeader("MY TOOL");
    drawList(d, ROWS, _sel, _scroll, [&](int i, int y, bool sel, int reserve) {
      d.drawSelectionRow(0, y - 1, d.width() - reserve, d.lineStep() - 1, sel);
      d.setCursor(4, y);
      d.print(i == 0 ? "Alpha" : i == 1 ? "Bravo" : "Charlie");
    });
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      if (_dirty) { the_mesh.savePrefs(); _dirty = false; }
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_UP)   { _sel = (_sel + ROWS - 1) % ROWS; return true; }
    if (c == KEY_DOWN) { _sel = (_sel + 1) % ROWS;        return true; }
    if (keyIsPrev(c) || keyIsNext(c) || c == KEY_ENTER) {
      /* mutate _prefs…, set _dirty = true */ return true;
    }
    return false;
  }
};
```

Then: `#include "MyToolScreen.h"` in `UITask.cpp`, add the member + constructor +
`gotoMyToolScreen()` (§1), and add a row in `ToolsScreen.h`. Done — scrolling,
the scrollbar, font scaling, e-ink pacing and persistence batching all come from
the framework.
