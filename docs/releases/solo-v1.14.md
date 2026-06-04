# Wio Tracker L1 — Solo Firmware v1.14

Based on MeshCore upstream v1.15.

## Highlights

- **GPS navigation suite** — the L1 becomes a standalone handheld navigator, no phone needed: **waypoints** (mark your spot or type coordinates, then get live bearing + distance back), a **GPS compass**, **navigate-to-anything** (a waypoint, the trail start, a mesh node, or a location someone texts you), and **share/save locations over the mesh**. No extra hardware — heading comes from GPS course-over-ground.
- **Renamed Plus → Solo** — to better reflect that this firmware is designed for *off-grid, standalone* use without needing a companion app on the phone; release artefacts are now `solo-v1.14-oled.uf2` and `solo-v1.14-eink.uf2`.
- **Global Units setting** — one Metric/Imperial switch (Settings › System) drives every distance and speed in the UI — Nearby, Trail, navigation, the min-distance gate and the map scale bar.
- **E-ink screenshot support** — `tools/screenshot.py` now works with both OLED and the e-ink panel; auto-detects rotation 0–3 and uses GxEPD2's visible dimensions; build with `-D ENABLE_SCREENSHOT`.
- **Battery percentage field** — new `Batt %` option for the Clock page dashboard, now sharing the same LiPo discharge curve and `low_batt_mv` cutoff as the top-bar indicator (so both readings agree).
- **Channel favourites** — mark individual channels as favourites from the channel context menu; new Settings › Contacts filter to hide non-favourite channels.
- **Context menu cycling unified** — LEFT/RIGHT now cycles values everywhere (Notif, Melody, Fav, Duration, BPM) without closing the popup; ENTER reserved for one-shot actions.
- **Popup menus expand to fit the screen** — on portrait e-ink up to 16 items fit without scrolling; on OLED items are now hard-capped to what physically fits.

## Changes

### Navigation (new)

- **Waypoints** — *Tools › Trail › Hold Enter*: **Mark here** drops one at the current GPS fix; **Waypoints** opens the list, which carries a synthetic **Trail start** backtrack row, a usage counter (`WAYPOINTS 3/16`), and an **+ Add by coords** row to enter a point by lat/lon/label with no fix required. Per-waypoint **Rename / Delete / Send**, plus **Clear waypoints**. Stored in their own `/waypoints` file (16 max) — independent of the trail and **not** cleared by Reset trail.
- **Navigate-to-point view** — distance plus two *absolute* bearings, **To:** (target) and **Hdg:** (your course over ground), to compare by eye — no magnetometer needed. Reused by waypoints, trail backtrack, Nearby-node navigation and message-shared locations.
- **GPS compass** — *Tools › Compass*: a heads-up **scrolling heading tape** (N..E..S..W under a fixed travel-direction pointer) with a large degrees + cardinal readout, derived from GPS movement. Works whether or not a trail is recording.
- **Course-over-ground source** — a heading ring in UITask, sampled ~1 s independently of trail logging, with gross-GPS-jump rejection and a minimum-movement gate; resets after a GPS gap so a reacquired fix can't imply a teleport heading.
- **Trail map as a live view** — the Map draws your current position and all waypoints continuously, even with no trail recording. While a trail exists the view frames the recorded route and clamps far-off waypoints to the nearest edge (a distant mark can't blow up the scale); with no trail it auto-fits to your waypoints and position. Waypoint markers show the first two label characters, placed edge-aware so labels stay on-map.
- **Navigate / save a shared location** — any message carrying a `lat,lon` (what `{loc}` inserts) or a `[WAY]lat,lon label` share offers **Navigate** and **Save waypoint** from its **Hold Enter** menu — on the history row or in fullscreen, for DMs and channels. **Send** on a waypoint shares it to a contact or channel, pre-filling the message for you to confirm.
- **Waypoints in GPX** — the GPX export now includes saved waypoints as `<wpt>` elements (label → `<name>`, plus `<time>`) alongside the track, so they import as pins in OsmAnd / Garmin / GPX Studio.
- **Trail readout & units** — the Trail action menu's old four-way km/h·mph·min/km·min/mi cycle is now a **Speed / Pace** toggle; the metric-vs-imperial unit follows the global Units setting. The min-distance gate shows metric (5/10/25/100 m) or imperial (15/30/75/300 ft) accordingly.

### New features

- **E-ink screenshot** — `WioTrackerL1Eink_companion_dual_dev` env enables capture; protocol header widened to 11 bytes (display type, rotation, uint16 width/height/chunk fields) so it scales to any GxEPD2 panel and rotation.
- **Dashboard `Batt %` field** — new Clock-page option computed from the same piecewise LiPo curve used by the top-bar battery indicator.
- **Channel favourites** — `ch_fav_bitmask` in prefs; toggle from Channel context menu; Settings › Contacts › Channels filter (all / favourites only).
- **GPS trail — scale grid on map** — toggle from the Trail action menu (Grid row); cycles with LEFT/RIGHT now (previously ENTER only).
- **Ringtone editor migrated to PopupMenu** — Duration and BPM now cycle in-place with LEFT/RIGHT; separate BPM+/BPM- rows removed.

### Fixes

- **Long messages no longer clipped** — message history and compose buffers were smaller than the over-the-air maximum, so long messages (and Polish text, where each accented char is two UTF-8 bytes) lost their tail; channel messages were worst because the sender name is embedded as `Name: body`. History, compose and keyboard buffers are now sized to `MAX_TEXT_LEN` (160 B), with per-field bounds kept on the smaller stores (custom messages, bot replies).
- **OLED joystick rotation** — enforce reset moved after prefs load, so stale e-ink rotation values no longer reverse joystick mapping on OLED builds (`FEAT_JOYSTICK_ROTATION_SETTING=0`).
- **First melody note duration** — preview-only side effect that shortened the first note in `playMelody()` no longer corrupts the stored sequence.
- **Home pages migration** — pages added in this release (Favourites, Trail) are appended to stored `page_order` instead of being hidden; SHUTDOWN is evicted from a full order to make room for required pages.
- **Pin picker fallback** — pressing `+` on an empty Favourites tile with no starred contacts and no recent DMs now lists all chat contacts instead of showing "No fav contacts".
- **Popup menus on OLED** — height-clamped to what fits on screen (max 4 items at 64 px), so long context menus no longer draw outside the panel.
- **Tools list scrolling** — the Tools menu now scrolls (with ▲/▼ indicators) so the added Compass entry stays reachable on the short OLED panel.
- **Trail action menu** — Grid toggle now also works with LEFT/RIGHT (not just ENTER); Export labels shortened to `Export (live)` / `Export (saved)` to fit.
- **E-ink default font** — unknown-character fallback rectangle aligned with the text cell baseline instead of one pixel below.
- **Build** — added `DisplayDriver::getBuffer`/`getBufferSize` virtuals so MyMesh no longer needs to know about concrete display types.

### Internal

- Shared `geo::` helpers (haversine, bearing, cardinal, distance formatting, `lat,lon` parsing) in `GeoUtils.h` and 1-px `gfx::` line/circle primitives in `GfxUtils.h`, replacing per-screen copies; a single `UITask::currentLocation()` GPS accessor used by every nav/compass/map screen.
- Prefs schema bumped to `0xC0DE0006` (`units_imperial`, `trail_show_pace`; the old combined `trail_units_idx` retired) — upgraders default to metric + speed.
