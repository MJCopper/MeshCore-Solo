# Zen UI framework — a guide for adding features

[Go back](../../README.md)

This is a developer guide to the reusable building blocks behind the
`companion_radio` **Zen** firmware UI (the `ui-new` screens). It is not a
user manual — for what each screen does, see [zen_features](../zen_features/).
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
- **`poll()`** runs every loop tick regardless of focus — reserve it for
  bounded background housekeeping that cannot use the render schedule.
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
   Only screens needing a parameter at entry add a typed call after it (for
   example, `gotoRingtoneEditor` selecting a melody slot).
4. Reach it from somewhere — usually a row in `ToolsScreen.h` (add an `Action`
   enum value, a row in the right section table, and a `dispatch()` case).

That's the whole contract. Steps 1, 3 and 4 are compiler-checked (a mismatch
won't link); only a forgotten step 2 can slip through — every screen pointer is
nullptr-initialised in `UITask.h` and `setCurrScreen()` bails on null, so a
missed `new` is an inert no-op rather than a null deref.

The constructor takes `UITask* task` plus whatever it needs (`NodePrefs*`,
`KeyboardWidget*`, …); the task back-pointer is how a screen calls shared
services such as `_task->showAlert(...)` and preference-saving helpers.

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

- `drawCenteredHeader(title, menu_hint=false, menu_open=false)` — plain centered
  title + separator.
- `drawInvertedHeader(label, menu_hint=false, menu_open=false)` — filled title
  bar (used by detail views).
- Both take an optional `menu_hint`: pass `true` on a screen with a Hold-Enter
  context menu to reserve a `≡` glyph (`menuHintWidth()`/`drawContextMenuHint()`)
  in the header, so the menu is discoverable without already knowing the
  shortcut; `menu_open` highlights it while the menu is actually up.
- `drawSelectionRow(x, y, w, h, sel)` — the highlight bar behind a list row.
- `drawTextEllipsized(x, y, max_w, str)` — truncates with `…`; **use this for
  any user string** (names, labels) so long/UTF-8 text can't overrun.
- `drawTextCentered(mid_x, y, str)`.
- All text helpers accept native UTF-8. Do not pre-convert display strings;
  clipping, measurement and glyph fallback are handled by the display driver.

---

## 3. Lists — `drawList`

`drawList` (`ui-new/icons.h`) is the workhorse for any scrolling list. It
computes the visible window from font metrics, keeps `sel` in view, reserves the
scrollbar column, draws each visible row through your callback, and draws the
indicator:

```cpp
drawList(display, count, _sel, _scroll, [&](int idx, int y, bool sel, int reserve) {
  drawRowSelection(display, y, sel, reserve);   // canonical highlight bar
  display.drawTextEllipsized(2, y, display.width() - reserve - 4, items[idx].name);
});
```

`reserve` is the width the scrollbar took (0 when the list fits) — subtract it
from any right-aligned content so nothing slides under the indicator. The row
callback owns its own selection bar: `drawRowSelection(d, y, sel, reserve)`
(`ui-new/icons.h`) draws the standard one (full row minus reserve, one pixel
short); call `display.drawSelectionRow()` directly only when a row needs a
non-standard geometry (full-width, custom height). For the
fold-in-place pattern (sections that expand/collapse) use `AccordionList`
instead (`ui-new/AccordionList.h`) — same idea, two callbacks (header + item).

Standalone scroll indicators (`drawScrollIndicator`, `…Px`) and the reserve
calculator (`scrollIndicatorReserve`) are exposed for hand-laid lists.

---

## 4. Reusable components

| Component | Header | Use for |
| --- | --- | --- |
| `PopupMenu` | `PopupMenu.h` | a modal action menu over any screen |
| `AccordionList` | `AccordionList.h` | legacy-compatible collapsible lists when genuinely needed |
| `KeyboardWidget` | `KeyboardWidget.h` | on-screen text entry |
| `DigitEditor` | `DigitEditor.h` | scroll-edit one number, digit by digit |
| `FullscreenMsgView` | `FullscreenMsgView.h` | scrollable full-message reader + word wrap |
| `MessageTranscriptView` | `MessageTranscriptView.h` | shared DM, room and channel transcript layout |

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
_menu.addItem("Ping"); _menu.addItem("Favourite");
_menu.active = true;
```

`KeyboardWidget` additionally supports **placeholders** (`{loc}`, `{time}`,
sensor tokens) via `addPlaceholder()` / `clearPlaceholders()`; the shared
`kbAddSensorPlaceholders()` (`ui-new/SensorPlaceholders.h`) adds only the tokens
the board's sensors actually provide. Expand them with `expandMsg()` at send time.

Two EN-US layouts share the text engine: **ABC** (one key per letter) and
**predictive T9**. In message fields, T9 turns phone-keypad digit sequences into
ranked words from Zen's 2,500-word Australianised conversational dictionary.
Its page cycle is predictive T9 → symbols → literal multi-tap → predictive T9,
so out-of-dictionary words can still be entered. Literal fields such as names
and passwords use multi-tap rather than predictive replacement.
The historical main/additional-script preference bytes remain serialized for
configuration compatibility but are no longer exposed or read by the keyboard.
Latin-diacritic letters are available from the corresponding ABC letter through
the shared accent popup. In completion-enabled message fields, Hold Enter opens
the completion dialog. The smiley special key opens the four-entry emoji picker;
CardKB uses Fn+M.
Shift is one-shot by default (capitalises the next letter, including whichever
candidate a T9 multi-tap cycle settles on) or Hold-Enter to toggle caps-lock;
Hold-Clear erases the whole field. **UP from the top letter row** enters
**cursor mode** (LEFT/RIGHT move the insertion point; UP/DOWN jump to
start/end, then — pressed again once already at that boundary — continue on
to the special row / letter grid, the same destinations the plain grid wrap
used to reach directly) so edits/inserts can target any point in the typed
text, not just the end; Enter/Cancel exit immediately from anywhere.
Hold-Enter on a Latin letter cell with accented variants instead opens
the **accent popup**: one horizontal row of `KB_ACCENT_VARIANTS[group]`
(a UTF-8 string per base letter, same shape as a T9 group string), LEFT/RIGHT
to pick, Enter to insert via the shared `insertGlyph()` helper, Cancel to
dismiss. Holding a letter with no variants, or any T9/symbols
cell, is a no-op.

`FullscreenMsgView::wrapLines()` is a standalone pixel-accurate word-wrapper
(O(n), variable-width-font aware) reusable by any multi-line layout; it writes
into the shared `s_wrap_lines` scratch (single-threaded render, never held
across a yield — see §9).

---

## 5. Domain helpers

**Formatting (`GeoUtils.h`, namespace `geo`, all pure/header-inline):**

- `haversineKm(lat1,lon1,lat2,lon2)`, `bearingDeg(...)`, `bearingCardinal(deg)`.
- `fmtDist(buf,n,km,imperial)` — "850m"/"2.3km" or feet/miles.
- `fmtAgeShort(buf,n,now,ts)` — compact "12s"/"5m"/"3h"/"2d" tag, "" for unknown.
  **This is the one age formatter** — don't reimplement the s/m/h ladder.

Zen does not compile the Map, Trail, Live Share, Locator, Compass or Waypoint
interfaces. New Zen features must not depend on their legacy screen fragments.

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
use `BIG_ICON` / `bigIconDraw`. The home status bar composes these right-to-left.
The advert indicator is a transient, steady icon shown for exactly five seconds
after an advert is queued; enabling automatic adverts alone does not display or
flash it. Persistent states such as repeater mode use their own status policy.

Icons are drawn from a fixed priority-ordered table (`HomeScreen::renderBatteryIndicator()`,
`UITask.cpp`); once the row runs out of horizontal space the loop just stops,
so the lowest-priority icons silently drop first rather than the whole bar
crushing the node name. Any animated status icon must reserve its width in every
phase so the row cannot shift as its drawing changes.

Screens with a Hold-Enter context menu (Messages, Nodes and Diagnostics) pass
`menu_hint=true` to their header call (see §2) so a `≡` glyph advertises the
menu; `KEY_CONTEXT_MENU` (Hold-Enter) opens it.

---

## 7. Input

Keys arrive as the `KEY_*` codes in `UIScreen.h`: `KEY_UP/DOWN/LEFT/RIGHT`,
`KEY_ENTER`, `KEY_CANCEL`, `KEY_CONTEXT_MENU` (the "Hold Enter" menu key).

Use the **prev/next convention** for value changes so the rotary encoder and the
D-pad agree: `keyIsPrev(c)` (LEFT or encoder-prev) and `keyIsNext(c)` (RIGHT or
encoder-next). `KEY_CANCEL` and `KEY_CONTEXT_MENU` stay screen-specific. Joystick
rotation is handled upstream (`rotateJoystickKey`) — screens see already-rotated
keys.

### From hardware to `handleInput`

Each physical button is a `MomentaryButton` (`src/helpers/ui/MomentaryButton.h`);
`UITask::begin()` must call `begin()` on **every** one (the joystick directions
and Back included, not just the user button) — that sets `pinMode` and, where
enabled, claims the interrupt. `UITask::loop()` polls each button, maps its event
to a `KEY_*` code, and dispatches it to the current screen.

Two mechanisms keep input responsive when **`endFrame()` blocks the loop** for a
slow e-ink refresh:

- **IRQ edge capture** (`-D BUTTON_USE_INTERRUPTS`, e-ink boards). A GPIO
  interrupt latches each press/release edge into a per-button ring buffer with
  its own timestamp, so taps that land *during* a refresh aren't lost; `check()`
  replays them afterwards. The nRF52 has only 8 GPIOTE channels (the radio takes
  one) — if none is free a button silently falls back to polling, so it still
  works, just without mid-refresh capture.
- **Key queue + coalesced redraw.** `loop()` drains *all* pending events from the
  buttons into a small key FIFO, applies the whole burst (`handleInput` per key),
  then redraws **once**. So three joystick flicks captured during one refresh move
  the selection three steps for the cost of a single panel update, instead of
  collapsing into an ignored multi-click or one-step-per-refresh. Buttons created
  with `multiclick=false` therefore emit one discrete `CLICK` per release;
  `multiclick=true` buttons still report double/triple-click.

---

## 8. Persistence

MeshCore-owned settings remain in `NodePrefs` and its upstream persistence path.
Zen-owned preferences are encoded through the versioned sidecar in
`solo/SoloPrefsCodec.h`; the historical internal name is intentionally retained
for stored-data compatibility. Defaults and validation live in
`SoloPrefsDefaults.h`, with legacy layouts handled by `SoloPrefsMigration.h`.
Add new Zen fields at this boundary rather than shifting upstream storage, and
provide a clamped default or migration for older files.

**The `_dirty` convention:** a multi-field editor screen mutates `_node_prefs`
live for instant feedback but only persists once, on exit, gated by a `_dirty`
flag — so LEFT/RIGHT value-cycling doesn't thrash flash. Set `_dirty = true` at
each edit site, then on the exit path call `_task->savePrefsIfDirty(_dirty)`
(`UITask`) — it saves once *iff* dirty and clears the flag, so every screen's
save-on-exit reads the same and the "did we touch flash?" decision lives in one
place. A one-shot action from a popup (no exit hook) calls `the_mesh.savePrefs()`
immediately. Follow whichever matches your screen.

### Child Mode policy

Child Mode is deliberately kept as a thin policy layer so upstream screen
changes remain easy to merge. Shared PIN rendering and allow-list predicates
live in `ui-new/ChildMode.h`; session state and transport enforcement live in
`UITask`. Screens should query `UITask::isChildModeLocked()` rather than reading
the preference directly, because a verified parent session temporarily lifts
the restrictions while leaving the persisted mode enabled.

The persisted configuration is owned by the Zen preference sidecar. Optional
pages use the existing `HomePageBit` positions, avoiding a second navigation model.
Keep the policy at existing boundaries when extending it:

- filter home pages in the central page-visibility predicate;
- filter message destinations where conversation lists are built;
- suppress mutation at the context-menu input boundary;
- use `UITask::applyChildMode()` for companion transport state; and
- suppress advert notification effects without dropping background advert
  processing.

The stored PIN hash is only an on-device UI check and is intentionally not a
cryptographic security boundary. Physical erase/reflash access remains a
recovery and bypass path.

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
  slot or per-contact notification/melody override) or a channel by
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
      drawRowSelection(d, y, sel, reserve);
      d.setCursor(4, y);
      d.print(i == 0 ? "Alpha" : i == 1 ? "Bravo" : "Charlie");
    });
    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      _task->savePrefsIfDirty(_dirty);   // saves once iff dirty, then clears
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
