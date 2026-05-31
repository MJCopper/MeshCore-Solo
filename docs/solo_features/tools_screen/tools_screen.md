## Tools Screen

[Go back](../../../README.md)

### Overview

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_1_oled.png) | ![](./tls_scr_1_eink.png) |

The Tools screen is a hub for GPS trail recording, nearby node browsing, ringtone editing, auto-reply bot, and auto-advert. Navigate the tool list with **UP/DOWN** and press **Enter** to open a tool.

---

## Nearby Nodes

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_2_oled.png) | ![](./tls_scr_2_eink.png) |

Browse nodes that have recently advertised on the mesh. Filter by category with **LEFT/RIGHT**:

| Filter | Shows                          |
| ------ | ------------------------------ |
| Fav    | Upstream-starred contacts only |
| ALL    | All known nodes                |
| Comp   | Companion (chat) nodes         |
| Rpt    | Repeaters                      |
| Room   | Room servers                   |
| Snsr   | Sensors                        |

Select a node to see its coordinates, distance, bearing with cardinal direction, type, and last-heard time.

From the list view, **Hold Enter** opens the context menu, where **Discover nearby** sends a live `NODE_DISCOVER_REQ` scan.

From either node detail view, **Hold Enter** opens the Ping popup:

|              OLED              |             E-Ink              |
| :----------------------------: | :----------------------------: |
| ![](./tls_scr_2_ping_oled.png) | ![](./tls_scr_2_ping_eink.png) |

Use **Enter** on the popup’s `Ping` row to send a direct mesh ping to that node. The popup then shows the RTT and SNR values on the next lines, and can be used again immediately for another ping.

> [!TIP]
> Combined with **Auto-Advert** on the other device, Nearby Nodes becomes a passive location tracker — as long as the tracked device periodically broadcasts its GPS position, you can see its current distance and bearing without any manual interaction on either end.

---

### Active Discovery

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_3_oled.png) | ![](./tls_scr_3_eink.png) |

Sends a `NODE_DISCOVER_REQ` ping. Repeaters, sensors and room servers within zero-hop range respond immediately with name, type and signal data.

Results show as 2-line cards: node name + type, then RSSI / SNR / remote SNR.

- **UP/DOWN** — navigate results
- **Enter** — open full-screen detail (public key, signal data, contact status)
- **Hold Enter** — rescan
- **Hold Enter** in full-screen detail — open the Ping popup for the selected node
- **Cancel / Back** — return to nearby list

---

## GPS Trail

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_4_oled.png) | ![](./tls_scr_4_eink.png) |

Records your route in a RAM ring buffer (up to 512 points, sampled every 1 s). Tracking runs in the background — a blinking **G** appears in the status bar. The trail survives display auto-off but is lost on reboot unless saved to flash first.

Cycle views with **LEFT / RIGHT**:

| View        | Content                                                                                                                                                             |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Summary** | Distance, elapsed time, avg speed or pace, point count, tracking status                                                                                             |
| **Map**     | Auto-fit dot-and-line plot with cos(lat) aspect correction; segment breaks marked; north arrow; scale grid (toggle with **LEFT/RIGHT** on the Grid row in the menu) |
| **List**    | Per-point rows showing local time (HH:MM) and delta distance from the previous point; segment-start rows show `start`; scroll with **UP/DOWN**                      |

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_5_oled.png) | ![](./tls_scr_5_eink.png) |

**Hold Enter** opens the action menu:

| Item                  | Interaction         | Action                                              |
| --------------------- | ------------------- | --------------------------------------------------- |
| Min dist              | LEFT/RIGHT          | Filter gate: 5 m / 10 m / 25 m / 100 m              |
| Units                 | LEFT/RIGHT          | Speed/pace: km/h / mph / min/km / min/mi            |
| Grid                  | LEFT/RIGHT or Enter | Toggle scale grid on the map                        |
| Start / Stop tracking | Enter               | Begin or end a recording session                    |
| Save trail            | Enter               | Write RAM ring to flash (`/trail`)                  |
| Load trail            | Enter               | Restore flash trail into RAM                        |
| Export (live)         | Enter               | Stream live RAM trail as GPX 1.1 over USB Serial    |
| Export (saved)        | Enter               | Stream saved flash trail as GPX 1.1 over USB Serial |
| Reset trail           | Enter               | Clear RAM ring and elapsed time                     |

### Downloading GPX

**Recommended — `tools/trail_export.py`** (auto-detects the port, captures from `<?xml` to `</gpx>`, writes a timestamped file under `tools/gpx/`):

```sh
uv run tools/trail_export.py
```

Then on the device: **Tools › Trail** → **Hold Enter** → **Export (live)** or **Export (saved)**.

**Manual fallback** — open a serial terminal at **115200 baud** and capture the stream by hand:

- **macOS/Linux** — `cat /dev/tty.usbmodem* > track.gpx` (stop with Ctrl-C after the dump finishes)
- **Windows** — PuTTY (Serial, 115200) or Arduino IDE Serial Monitor with no line ending; copy the text from `<?xml` to `</gpx>` into a `.gpx` file

Either way, the resulting file imports into OsmAnd, Garmin BaseCamp, GPX Studio, Google Earth, etc.

> [!NOTE]
> If the companion app is connected via **BLE**, the export is safe — BLE and USB operate independently. If connected via **USB**, disconnect the app before exporting.

---

## Auto-Advert

Periodically broadcasts a 0-hop advert with your GPS position. Configurable interval: off / 30 s / 1 min / 2 min / 5 min / 10 min / 30 min / 1 h. A blinking **A** appears in the status bar while active.

---

## Ringtone Editor

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_6_oled.png) | ![](./tls_scr_6_eink.png) |

A step sequencer for composing custom notification melodies. Two slots — **Melody 1** and **Melody 2** — switchable from within the editor.

Each melody supports up to 32 notes:

| Parameter | Options                           |
| --------- | --------------------------------- |
| Pitch     | C / D / E / F / G / A / B / pause |
| Octave    | 4 – 7                             |
| Duration  | 1/4 / 1/8 / 1/16 / 1/32           |
| BPM       | 60 / 90 / 120 / 150 / 180         |

**Navigation in the editor:**

- **LEFT/RIGHT** — move between notes
- **UP/DOWN** — change pitch of selected note
- **Enter** — cycle octave of selected note
- **Hold Enter** (or context menu) — open options menu

**Options menu:**

| Item         | Interaction | Action                                 |
| ------------ | ----------- | -------------------------------------- |
| Play / Stop  | Enter       | Preview the melody                     |
| Melody 1 / 2 | Enter       | Switch to the other slot               |
| Duration     | LEFT/RIGHT  | Cycle duration for selected note       |
| BPM          | LEFT/RIGHT  | Cycle tempo                            |
| Insert       | Enter       | Insert a new note after the cursor     |
| Delete       | Enter       | Delete the note at cursor              |
| Save & Exit  | Enter       | Persist the melody and return to Tools |
| Discard      | Enter       | Return to Tools without saving         |

Melodies can be assigned in **Settings › Sound** (global default) or overridden per contact or channel from the Messages screen context menu.

---

## Auto-Reply Bot

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./tls_scr_7_oled.png) | ![](./tls_scr_7_eink.png) |

Automatically replies to incoming messages that contain a configured trigger word (case-insensitive, contains match).

When the bot is enabled it listens to DMs by default. Channel monitoring is an optional addition — select a channel separately to activate it alongside DM mode.

| Setting       | Description                                                                      |
| ------------- | -------------------------------------------------------------------------------- |
| Bot           | ON / OFF — enables DM listening                                                  |
| Channel mode  | ON / OFF — additionally monitors a selected channel                              |
| Channel       | Which channel to monitor (only relevant when channel mode is ON)                 |
| Trigger       | Word or phrase that activates the reply (shared by both modes, case-insensitive) |
| DM reply      | Reply text for DMs; supports `{time}` and `{loc}` placeholders                   |
| Channel reply | Reply text for channel messages; supports `{time}` and `{loc}` placeholders      |

A 10-second cooldown prevents repeated replies in quick succession.
