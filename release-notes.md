## MeshCore Solo Companion Firmware v1.22

### What's new

- **T9 on-screen keyboard** — a new keyboard option under **Settings › Keyboard**. Alongside the existing alphabetical **ABC** grid, pick **T9** for phone-keypad-style entry: each key is labelled with its digit and a letter group (e.g. `2abc`), and repeated **Enter** presses cycle through the letters and then the digit. (The alphabetical grid was previously mislabelled *QWERTY* in Settings — it's always been a-b-c order and is now correctly named **ABC**.)
- **Auto-save GPS trail on shutdown** — a new **Tools › Trail › Settings › Auto-save** toggle (default off). With it on, the live trail is written to flash automatically at power-off, so a **low-battery auto-shutdown** no longer loses the whole route. It only writes when the trail actually has points, so an empty trail can't overwrite a previously saved one. Saves to the same `/trail` file as the manual **Trail › Save**.
- **Clock Tools from the Tools menu** — the Alarm / Timer / Stopwatch screen (reachable by pressing **Enter** on the Clock home page) now also has its own entry under **Tools › System**, so it's discoverable without first finding it on the clock face.
- **Map & Shutdown pages are reorderable** — the **Map** home page shipped with no entry under **Settings › Home Pages**, so it was always shown and couldn't be moved. It's now a first-class page there: **toggle its visibility** and **reorder it** like any other (visible by default; existing users keep it on). Reordering also now covers the **Shutdown** page, which was previously pinned to the end of the carousel.

### Fixes

- **Battery reads high and jittery on Wio Tracker L1** — a power-saving change had switched the battery-voltage divider on for only ~10 ms before each reading, too short for the high-impedance divider node to settle, so a full LiPo could report 4.6–5.0 V. The divider is now held on continuously again (a negligible ~2 µA on the 2×1 MΩ divider), giving stable, accurate readings. The CPU-sleep and LED energy savings from the same optimisation pass are untouched.
- **Home page visibility no longer resets on update** — the prefs schema-migration that turned the **Favourites** page on ran on *every* firmware update (any schema-version bump), so a page you'd deliberately hidden kept coming back after each release. It's now gated to the single upgrade that introduced Favourites, so your hidden pages stay hidden.
- **A batch of solo-mode fixes** from a UI audit:
  - a **locked device didn't show the alarm** — it woke the display, then immediately re-blanked, so a ringing alarm played to a dark screen. The lock screen now draws the alert and holds the wake for the whole ring.
  - **background-mode status icons vanished when Bluetooth was off** — the auto-advert / live-share / trail / repeater indicators were nested inside the "BT on" check and are now always shown while their mode is active.
  - a **long alert message** ("GPS on, tracking started") spilled past its box on a narrow screen — it now wraps to up to three lines and the box grows to fit.
  - a **contact name pushed from the phone app** wasn't guaranteed to be NUL-terminated.
  - the **timer / alarm could mis-fire** for a deadline landing past the ~49-day `millis()` rollover.
  - toggling a **channel's favourite from its context menu** could retarget the menu onto a different channel once the list re-sorted.
- **A repeater could appear twice** on the discovery scan and in the app — a discover response often arrives more than once (the direct copy plus a re-flooded copy carry different packet hashes, so the mesh's duplicate filter passes both), and each copy was listed. Duplicate copies of the same response are now dropped for a few seconds after the first.
- **Favourites self-heal** — a pinned favourite whose contact no longer exists (e.g. after clearing contacts) now reverts to an empty **+** tile instead of showing **(gone)**, and the stale pin is cleared from prefs.
- **Radio preset names no longer seed `{loc}`/`{time}` placeholders** — those only make sense in a message; the preset-name keyboard (Settings › Radio and Tools › Repeater "+ Save current…") now opens empty. The message-slot editor still keeps them. The room-login **password** keyboard drops them too.
- **Room chat opens automatically after a successful login** — logging in to a room server (first-time or a remembered password) used to drop you back on the room list, needing a second **Enter** to actually open the chat. The chat now opens on its own once the login succeeds, as long as you're still on that room in the picker.

### Under the hood

- **OLED draws less** — the SH1106 driver now skips pushing a frame over I²C when it's byte-identical to the last one, cutting redundant traffic and a little power on the static screens (clock, home) that don't change between updates.
- **e-ink: cleaner screen changes** — switching screens now forces a full (non-partial) refresh on the first frame of the new screen, clearing leftover ghosting from the previous screen that the periodic partial-refresh interval didn't catch.

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
