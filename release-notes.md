## MeshCore Solo Companion Firmware v1.22

### Fixes

- **Home page visibility no longer resets on update** — the prefs schema-migration that turned the **Favourites** page on ran on *every* firmware update (any schema-version bump), so a page you'd deliberately hidden kept coming back after each release. It's now gated to the single upgrade that introduced Favourites, so your hidden pages stay hidden.
- **Map page in Home Pages settings** — the **Map** home page shipped with no entry under **Settings › Home Pages**, so it was always shown and couldn't be moved. It's now a first-class page there: **toggle its visibility** and **reorder it** like any other (visible by default; existing users keep it on). Reordering also now covers the **Shutdown** page, which was previously pinned to the end of the carousel.

## MeshCore Solo Companion Firmware v1.21

### What's new

- **Live Location Sharing** — broadcast your position over the mesh as `[LOC]` messages, **movement-gated**, to a channel or a single contact. Others who share theirs show up as pins on the map with live **distance/bearing in Nearby Nodes**, and a **status-bar indicator** appears while your own share is active. `[LOC]` is parsed in DMs, channel messages **and room messages**; DM shares name the sender.
- **Locator** (geofence) — arm a geofence around a **target** — a saved waypoint *or* a person (their live `[LOC]` or last-known position) — and get an alert when you **arrive/leave** or they get **near/far**, with an optional **homing beeper** that ticks faster the closer you get (and overrides a muted buzzer). Arm it from **Tools › Locator** or straight from **Nearby Nodes** / **Waypoints**; pick the target from a list (favourites first, then any contact), and clear it via a **"None"** entry. The active target is drawn as a **flag** on the map.
- **One active target across Locator / Navigate / Map** — a single **"Set as target"** action everywhere, backed by a shared resolver that prefers a live `[LOC]` share over the last-advertised GPS, so the three features always agree on where you're headed.
- **Follow live contacts** — **Navigate** to a contact who is live-sharing and the view **follows them as they move**, adding an **ETA** line. **Quick-share** your own position straight from the Map.
- **Map & status-bar upgrades** — the home mini-map gains a **north marker** and a **scale tick**; the status line shows the tracked-node count and, with a fix, an **arrow + distance to the nearest tracked contact** (e.g. `Track:3 →120m`); a **GPS fix icon** sits in the top status bar (boxed once a fix is valid, plain while searching), shown only on GPS boards while **GPS** is enabled in Settings.
- **Trail auto-pause** — recording **freezes on stops** (banking elapsed time and breaking the map line across the idle gap) and **resumes on movement** without ending the session; the home-screen blink keeps going while paused.
- **Collapsible Tools** — tools are grouped into fold-in-place **Location / Comms / System** sections, the same model as Settings (Tools always opens folded to the section list), and the home carousel now uses **page-indicator icons** instead of dots.
- **Clock tools — alarm, timer, stopwatch** — press **Enter** on the Clock page for three time utilities: a **one-shot wake alarm** (set hour/minute with the digit editor; a **bell** marks it on the clock face and the status bar while armed), a **countdown timer** (full **HH:MM:SS**, single-digit cursor), and a **stopwatch**. The alarm and timer **ring with a melody — overriding mute** — and are silenced by **any key**. They keep running with the display off or locked, and the alarm is scheduled as an **absolute instant** so it survives the clock re-syncs the mesh / app / GPS / CLI can trigger at any time (it can't wake the device from a full Shutdown, where the CPU is off).
- **Waypoint coordinate editor** — add a waypoint by scroll-editing its latitude/longitude digit by digit.
- **On-device room login with saved passwords** — log in to a room server straight from the device (no phone app): pick the room and the password prompt appears automatically (a blank password works for open rooms), or re-login any time via the room's **context-menu "Login…"**. The password is **remembered across reboots**, so a room you've used before logs back in without retyping; a failed login (e.g. the server's password changed) forgets the stale password so the next attempt prompts again. Room passwords entered via the **phone app** are saved on the device too, so it can post to that room standalone after a reboot. Saved passwords are written with the same atomic, crash-safe persistence as contacts and channels.

### Fixes

- **Critical — low-heap hang and contact loss on RAM-tight builds.** With `MAX_CONTACTS=350` and `OFFLINE_QUEUE_SIZE=256` the device ran with very little free heap; an allocation in the input/menu path could then fail and **hang the UI** (notably when entering Diagnostics), and a crash or reset mid-save could **wipe all contacts**. Fixed by two independent changes:
  - **Right-sized message-history rings** — the on-device scrollback rings were halved (96→48 channel, 64→32 DM), recovering **~14 KB of free heap** (measured 3 → 17 KB). History is RAM-only, so the only cost is shorter on-device scrollback.
  - **Atomic persistence** — contacts, channels and prefs are now written to a temp file and **atomically renamed** into place; an interrupted save (crash, reset, full flash) leaves the previous good file intact instead of truncating it.
- **USB host stall** — `Serial.write()` is bounded so a stalled USB host can no longer hang the device indefinitely.
- **Nearby Nodes** — live `[LOC]` senders now respect the type filter and sort by their shared position; the distance-sorted list refreshes so live shares bubble to the top.
- **Map** — live contacts are labelled before waypoints, so a person's name shows rather than a nearby waypoint's.
- **GPS status icon** is hidden when GPS is turned off in Settings, instead of sitting there empty.
- **Trail — start with GPS off** now prompts **"GPS is off — Enable GPS & start"** instead of silently starting a session that shows "Waiting for GPS fix" forever and records nothing.
- Null-guarded the Locator target picker and clamped the loc-share channel index on load.
- **e-ink button responsiveness** — joystick and Back presses are no longer dropped during a slow panel refresh. The directional buttons were never initialised for interrupt edge-capture (only the user button was), so they silently stayed on the polling path; now every button is initialised, and a burst of taps captured while the panel is refreshing replays as **discrete navigation steps** instead of collapsing into a single ignored multi-click. Edge capture and the live-pin self-heal are both debounced, so **contact bounce can't surface one press as a double-tap** (e.g. start+stop on the stopwatch).

### Under the hood

- **Streaming trail simplification** — GPS points are simplified as they're recorded via a fixed-corridor (Reumann–Witkam) pass: straight runs collapse to their two endpoints while curves stay bounded to within the **Min dist** tolerance of the real track, so the 512-point buffer covers a far longer route than a flat point budget would suggest. The buffer is kept at 512 points to bound the static RAM footprint on nRF52.
- A UITask-decoupled active-target resolver (`resolvePersonPos` / `activeTargetPos`) replaces the duplicated target-resolution logic that lived separately in the Locator, Navigate and Map entry points.
- `-Os` size optimisation on the e-ink and GAT562 30S solo envs to keep them within the flash budget.
- **Input edge-capture + key queue** — on e-ink, button edges are latched by a GPIO interrupt during the blocking `endFrame()` refresh and replayed once the loop runs again; `loop()` then drains the whole burst into a small key FIFO and applies every key before a **single** redraw. Rapid navigation neither gets lost nor costs one slow refresh per step. (Buttons fall back to polling automatically if no GPIOTE channel is free.)
- **Ringtone player on a hardware timer** — note advance moved off the `loop()` poll onto an nRF52 TIMER1 compare interrupt, so a blocking e-ink refresh can no longer stretch or skip a note; alarm/timer melodies stay on tempo regardless of render cadence.

> **Upgrade note:** if a previous version lost your contacts after a hang, you'll need to re-add them once — the atomic-save protection applies from this release onward.
