# Feature roadmap

Joystick-only UX constraints: 4 directions + Enter + Back. No text entry except inside KeyboardWidget. Everything else navigable with cursor + press.

Status legend: 📋 planned · 🚧 in progress · ✅ done · ❌ rejected/deferred

---

## Priority queue

### ✅ Mark-all-read at type level

Hold Enter on the MESSAGE mode-select screen (DM / Channels / Rooms) opens a 1-item context menu "Mark all read". Acts on the currently highlighted mode and shows a brief confirmation alert.

Implementation:
- New `UITask::clearAllDMUnread()` — `memset` over `_dm_unread_table`
- `QuickMsgScreen::clearAllChannelUnread()` already existed
- `UITask::clearRoomUnread()` already existed
- Title is a `static const char*` table (PopupMenu stores the title pointer verbatim — locals would dangle)
- Zero schema impact, all counters live in RAM

### ✅ Favourites dial

Phase 1 ✅ (storage + read-only render + grid nav)
Phase 2 ✅ (pin from Contact options menu in QuickMsg + slot picker submenu)
  - Enter on a filled tile opens that contact's DM directly
  - Cancel from a Favourites-opened DM returns to the home screen
Phase 3 ✅ (in-place pin picker on empty tile: upstream-favourited contacts first,
   then recent DM contacts deduped; selecting a contact that's already pinned
   elsewhere moves it to the new slot)

**Follow-up to revisit**: on OLED, the unread-count badge in the top-right of a
favourite tile overlaps the contact name. Reserve a few pixels for the badge
when computing the name's `drawTextEllipsized` max width so the name shortens
to "Nam…" instead of running under the digit. HomeScreen FAVOURITES render in
UITask.cpp.

A 2×3 grid (six slots) of pinned contacts on its own home page, between Clock and Messages. Joystick picks a tile, Enter opens the existing DM conversation or sends a pre-set quick reply.

Data model:
- New field in NodePrefs: `uint8_t favourite_contacts[6][6]` — first 6 bytes of each contact's `pub_key` (enough to disambiguate locally)
- Lookup at render time: walk contacts, match prefix, render name + unread badge
- Empty slot renders as "+" placeholder; Enter on empty opens a contact picker (existing UI)

Pinning UX:
- In QuickMsg DM list, long-press on a contact → context menu → "Pin to dial" → asks which of the 6 slots
- Unpin via the same menu (only shown when contact is already pinned)

Schema bump: add `favourite_contacts` to NodePrefs, bump `SCHEMA_SENTINEL` low byte.

Render layout (250×122 landscape e-ink):
```
╔══════════════════════════════╗
║ Favourites                   ║
╠══════════════════════════════╣
║ ┌──────┐ ┌──────┐ ┌──────┐   ║
║ │Alice │ │Bob 3 │ │  +   │   ║
║ └──────┘ └──────┘ └──────┘   ║
║ ┌──────┐ ┌──────┐ ┌──────┐   ║
║ │Carol │ │  +   │ │  +   │   ║
║ └──────┘ └──────┘ └──────┘   ║
╚══════════════════════════════╝
```

Joystick navigation is natural with 6 tiles (UP/DOWN between rows, LEFT/RIGHT within row).

### 🚧 GPS trail (renamed from breadcrumb)

Phase 1 ✅ (storage + sampling + Summary view + G indicator in status bar)
Phase 2 ✅ (auto-fit Map view with cos(lat) aspect compensation; LEFT/RIGHT cycles views)
  - Summary scrolls on short panels (OLED) so hint stops overlapping
  - Status-bar G blinks at the same cadence as A (forces 1 s home refresh)
  - Default sampling 30 s + 5 m min-delta (was 60 s + 25 m — too sparse on foot)
  - "Avg speed" replaces "Speed"; Time uses RTC so it ticks every render
  - Stop → start creates a new segment; the map doesn't bridge dead time,
    and total distance skips segment boundaries
Phase 3 ✅ (per-point list view with HH:MM local time + delta-from-previous;
   segment-start rows show "start" instead of a delta)
Phase 4 📋 (context menu: reset, save → slot N, load ← slot N, GPX export over USB)
Phase 5 ✅ (Min-dist setting on a Config view that's the resting state of the
   Trail screen — LEFT/RIGHT cycles, Enter starts/resumes tracking; live
   views only reachable while tracking. Sampling cadence is fixed at 1 s,
   GPS upd setting also removed — both rely on the sensor manager's defaults)
Polish ✅ (map view: filled/open dot markers around segment breaks;
   "Waiting for GPS fix" status when started without a lock;
   capacity bumped to 512 points; elapsed/avg-speed run on millis() instead
   of RTC so they tick even before GPS time is synced)

Tools › Breadcrumb. Periodically samples `(lat, lon, ts)` into a RAM ring buffer; user explicitly saves snapshots to flash.

**Logging is a runtime state**, not a settings value. User starts/stops from the
Tools › Breadcrumb screen. Once active, sampling continues in the background
regardless of which screen is shown, and a `G` indicator appears in the status
bar (analogous to `A` for auto-advert). A reboot resets the active state to
off; the RAM trail is also lost on reboot unless saved to a flash slot first.

Settings only control sampling cadence and the min-distance gate — they don't
enable/disable the feature.

**Storage model — RAM ring with explicit save**

Rationale: auto-off only blanks the display, the firmware keeps running, so the RAM trail survives every idle scenario. Typical use is a single trip start→stop while wearing the device; persisting across reboots is rarely wanted. RAM-only avoids ~1400 flash writes/day and the LittleFS wear that comes with continuous logging.

- Live ring: `BreadcrumbEntry[BC_RAM_CAP]` in `UITask` (or a dedicated component). Each entry `int32_t lat_1e6, int32_t lon_1e6, uint32_t ts` = 12 B. Cap = 256 → 3 KB RAM. nRF52840 (256 KB RAM) has plenty of headroom.
- Wrap-on-write: oldest entry replaced when buffer full.
- Reboot wipes the live trail (intentional; matches the "this trip" model).

**Snapshot slots on flash** (user-initiated only):
- `/breadcrumb.0`, `/breadcrumb.1`, `/breadcrumb.2` — three named slots
- Each file: small header (count, start_ts, end_ts, total_distance_m) + entry array
- Written only on explicit "Save trail" action — zero background writes, zero wear concern
- Optional: auto-save to slot 0 on detected low-battery shutdown (single write before going dark)

UI screens (LEFT/RIGHT cycles):
1. **Summary** — total distance (km), elapsed time (h:mm), point count, current speed (from last 2 samples), GPS fix indicator
2. **Trail map** — ASCII bounding-box plot. Auto-fit the polygon, current position marked `X`, start marked `*`. UP/DOWN zoom, LEFT/RIGHT pan when zoomed.
3. **Last N entries list** — scroll through recent points with timestamp + delta from previous.

Joystick actions:
- Enter → toggle live logging on/off (status bar shows `*` when active, like auto-advert `A`)
- Back → exit
- Hold Enter → context menu:
  - "Reset trail" — clear RAM ring
  - "Save trail → slot N" — snapshot RAM into chosen flash slot
  - "Load trail ← slot N" — restore from chosen slot into RAM ring
  - "Export over USB" — dump live RAM trail as KML/GPX over serial

Statistics computed on the fly walking the ring:
- Total distance: sum of Haversine(p[i], p[i-1])
- Elapsed time: ts[last] - ts[first]
- Current speed: dist(last, prev) / (ts[last] - ts[prev])

Settings:
- Settings › GPS › Breadcrumb interval: 30 s / 1 min / 5 min / 15 min (default 1 min) — only the cadence; logging on/off is a Tools toggle
- Settings › GPS › Breadcrumb min delta: 5 m / 25 m / 100 m (skip near-stationary samples to keep the ring densely populated with real movement)
- Export format: GPX (standard for GPS tracks; OSMAnd / Garmin compatible)

Schema impact: new prefs fields `uint8_t breadcrumb_interval_idx`, `uint8_t breadcrumb_min_delta_idx`. Sentinel bump. The slot files are separate from prefs.

Edge cases:
- No GPS fix: skip sampling, status indicator dims
- Low-batt shutdown: optional auto-save to slot 0 (one write) before powerdown
- Memory: 3 KB RAM is negligible on this MCU; if RAM ever tightens, drop to 128 entries

---

## Backlog (not yet prioritised)

### Compass to contact

Select contact in Nearby → Enter → fullscreen compass. Arrow points to last-known position of contact relative to my GPS heading (need accel/mag or compute heading from movement). Distance below. Updates every 1 s.

Joystick: Enter from Nearby toggles, Back exits. Heading-from-GPS requires moving; for stationary use, would need magnetometer that the L1 board does or doesn't have — check.

### SOS broadcast

Configurable in Settings › System › SOS:
- Target: channel index or DM contact
- Message template (uses placeholders)

Trigger: Hold Back + Hold Enter for 3 s on any screen → confirmation popup ("Send SOS?") → Enter to send. Sends with `{loc}` and `{batt}` filled. 30 s cooldown.

### Range test

Tools › Range Test:
- Pick a node from contacts/nearby
- Enter starts pinging every 5 s, logs RTT + RSSI + SNR (ring ~30)
- Display shows current values + 30-sample sparkline (block characters)
- Enter stops; Hold Enter for context menu (reset, change target)

### Quiet hours

Settings › Sound › Quiet Hours:
- Enable on/off
- Start HH (LEFT/RIGHT to change, 24 h)
- End HH

When within window: buzzer set to "off" (overrides setting), display brightness → 0. Restores prefs values when window ends. Time source: rtc_clock.

### Channel scanner home page

Toggleable in Settings › Home Pages. Lists channels with: name, unread count, last message age. Enter opens the channel. Sort by recency by default; LEFT/RIGHT toggles to alphabetical.

### Contact distance sort

QuickMsg DM list: a 4-th sort mode (currently sorted by message count). LEFT/RIGHT on the list header cycles: name | message-count | recency | distance. Distance uses GPS pos from contact's last advert.

### Signal stats screen

Tools › Stats:
- Battery voltage 60-min sparkline
- RSSI of last 30 received packets
- Noise floor current
- Free heap (if available)

Read-only. UP/DOWN switches between metrics. Bottom shows current value as text.

### Auto-reply trigger words with sensor placeholders

Bot already has a single trigger word. Extend to a small table of (trigger, reply-with-placeholder) pairs:
- "temp?" → replies with `{temp}`
- "loc?" → replies with `{loc}`
- "batt?" → replies with `{batt}`

Stored in NodePrefs as fixed-size table (say 4 pairs × 16 B). UI: edit pairs in Tools › Auto-Reply Bot.

### Lock-screen unread count

Show `12 new` or two-line `5 DM / 7 ch` on lock screen below the time. Reuses existing `_hist`/`_dm_hist` unread counters. Cosmetic only, no schema change.

### Power profile presets

Settings › Profile: Indoor / Outdoor / Expedition. Each pre-fills:
- Auto-off seconds
- GPS interval
- Auto-advert interval
- Brightness

Single Enter applies. Stored as `uint8_t profile_idx` with hardcoded value tables.

### Battery curve calibration

Settings › System › Batt Calibration: edit 5 voltage breakpoints used to convert mV → %. UP/DOWN selects breakpoint, LEFT/RIGHT changes voltage in 50 mV steps. Helps users with non-standard LiPos report accurate %.

### Mark-read at type level — already covered above as priority

### Display test pattern

Tools › Display Test: full-screen grid + bars + Lemon glyph dump. Useful for verifying driver/font changes after flashing.

### Group "who's online" ping

From channel view, Hold Enter → "Who's online?". Sends 0-hop discovery to channel members, collects responses for 10 s, shows a list with RSSI. Similar to existing Nearby active discovery but scoped to a channel.

---

## Deferred

### ❌ Morse keyboard

Cute but niche. Skip unless explicitly requested.

### ❌ Buzzer EVENTS_STOPPED IRQ chaining

Marginal real-world gain (2 ms between notes during melody playback only), high risk on the PWM peripheral.

### ❌ Vibration feedback

Wio Tracker L1 doesn't have a haptic motor. N/A.

---

## Implementation order suggestion

1. **Mark-all-read** — smallest patch, biggest day-to-day comfort gain
2. **Favourites dial** — new home page + new prefs field; touches familiar areas (QuickMsg context menu, NodePrefs, schema sentinel bump)
3. **GPS breadcrumb** — largest of the three; introduces a new flash file and a Tools sub-screen with multiple views

After #3, re-prioritise the backlog with the user.
