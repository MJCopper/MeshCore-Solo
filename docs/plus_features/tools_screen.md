### Nearby Nodes

Browse nodes that have recently advertised on the mesh. Filter by category (Favourites, All, Companion, Repeater, Room, Sensor). Select a node to see its coordinates, distance, bearing with cardinal direction, type, and last-heard time.

```
╔══════════════════════════════╗
║▓▓▓▓▓ WioTracker-Alice ▓▓▓▓▓▓ ║  ← node name
╠══════════════════════════════╣
║ Lat: 50.06190                ║
║ Lon: 19.94090                ║
║ Dist: 2.3km                  ║
║ Az: 145d (SE)                ║
║ Type: Companion              ║
║ Seen: 5m ago                 ║
╚══════════════════════════════╝
```

Use **Send my advert** (hold Enter → context menu) to broadcast your position and prompt nearby nodes to respond with theirs.

#### Active Discovery

Press **Enter** from the Nearby screen to send a live `NODE_DISCOVER_REQ` ping. All reachable repeaters, sensors and room servers within zero-hop range respond immediately with their name, type and signal data. Results appear as 2-line boxed cards with RSSI, SNR and the remote SNR (signal quality at the responder's end).

```
╔═══════════════════════════════╗
║▓▓▓▓▓▓▓ DISCOVER (2 found) ▓▓  ║
╠═══════════════════════════════╣
║▓▓▓▓▓▓▓▓▓▓▓▓▓ Rptr-A   Rpt ▓▓▓▓║  ← selected
║▓▓▓▓▓▓▓▓ RSSI:-79 SNR:9 ▓▓▓▓▓▓▓║
║┌────────────────────────────┐ ║
║│▓▓▓▓▓▓▓▓▓▓▓▓ Sensor-B  Snsr │ ║
║│ RSSI:-94 SNR:3             │ ║
║└────────────────────────────┘ ║
╚═══════════════════════════════╝
```

Navigate with **UP/DOWN**. Press **Enter** on a node to open a full-screen detail view showing the public key, RSSI, SNR, remote SNR and whether the node is already in your contacts.

Hold **Enter** from the discovery list to rescan. Press **Cancel** or **Back** to return.

### Tools Screen

#### GPS Trail

Records your route in a RAM ring buffer (up to 512 points). Sampling runs in the background whenever tracking is active — a blinking **G** indicator appears in the status bar. The trail survives display auto-off but is lost on reboot unless saved to flash first.

Three views cycle with **LEFT / RIGHT**:

| View | Content |
|------|---------|
| **Summary** | Total distance, elapsed time, avg speed or pace, point count, tracking status |
| **Map** | Auto-fit dot-and-line plot with cos(lat) aspect correction; segment breaks marked with open/filled dots; north arrow; scale grid (toggle with Enter) |
| **List** | Per-point rows showing local time (HH:MM) and delta distance from the previous point; segment-start rows show `start` |

**Hold Enter** opens the action menu:

| Item | Action |
|------|--------|
| Min dist | Cycle min-distance gate: 5 m / 10 m / 25 m / 100 m (LEFT/RIGHT while focused) |
| Units | Cycle speed/pace units: km/h / mph / min/km / min/mi (LEFT/RIGHT while focused) |
| Start / Stop tracking | Begin or end a recording session |
| Save trail | Write the current RAM ring to flash (`/trail`) |
| Load trail | Restore the saved flash trail into RAM |
| Export GPX | Stream the live RAM trail as GPX 1.1 over USB Serial |
| Export saved | Stream the saved flash trail as GPX 1.1 over USB Serial without loading it into RAM |
| Reset trail | Clear the RAM ring and elapsed time |

#### Downloading GPX to your computer

1. Connect the device to your computer with a USB cable.
2. Open a serial terminal at **115200 baud** on the device's serial port:
   - **macOS / Linux** — `screen /dev/tty.usbmodem* 115200` or `cat /dev/tty.usbmodem*` (replace with your port, e.g. `/dev/ttyACM0` on Linux); to save directly to a file use `cat /dev/tty.usbmodem* > track.gpx` and stop with Ctrl-C after the dump completes
   - **Windows** — open PuTTY (Connection type: Serial, Speed: 115200), or use Arduino IDE › Tools › Serial Monitor (set line ending to "No line ending")
3. On the device, navigate to **Tools › Trail**, then **Hold Enter** and select **Export GPX** (live ring) or **Export saved** (flash slot).
4. The device streams a GPX 1.1 XML document to the serial port. Copy the output starting from `<?xml` to `</gpx>` and save it as a `.gpx` file.
5. Import the file into any GPX-compatible application — OsmAnd, Garmin BaseCamp, GPX Studio, Google Earth, etc.

> **Note:** If the companion app is connected over **BLE**, the export is safe — the app ignores USB input while BLE is active and the device alert will read *"GPX N B (USB)"*. If you are using the app over **USB**, disconnect from the app first before exporting; the raw XML stream will otherwise disrupt the app's serial framing. The alert *"GPX N B - disc. app"* indicates no BLE link was detected and serves as a reminder to reconnect the app afterwards.

#### Auto-Advert

Periodically broadcasts a 0-hop advert with your GPS position so nearby nodes can track your location automatically. Configurable interval: off / 30 s / 1 min / 2 min / 5 min / 10 min / 30 min / 1 h.

A blinking **A** indicator appears in the status bar while Auto-Advert is active.

#### Ringtone Editor

A step sequencer for composing custom notification melodies stored on the device. Two independent melody slots are available — **Melody 1** and **Melody 2** — switchable from within the editor. Each melody supports up to 32 notes with adjustable pitch (C–B + pause), octave (4–7), duration (1/4 / 1/8 / 1/16 / 1/32), and BPM (60 / 90 / 120 / 150 / 180). Playback preview is available directly from the editor.

Melodies can be assigned as notification sounds per message type (DM / channel) in Settings, and individually overridden per channel or per contact from the message screen context menu.

#### Auto-Reply Bot

Automatically replies to incoming messages that contain a configured trigger word (case-insensitive).

- **DM mode** — when enabled, the bot listens to all incoming private messages and replies with the DM reply text.
- **Channel mode** — optionally, select a channel for the bot to monitor. When a trigger is matched, it replies with a separate channel reply text.
- Both modes can be active simultaneously and share the same trigger word but use independent reply texts.
- Replies support placeholders (`{time}`, `{loc}`).
- A 10-second cooldown prevents repeated replies in quick succession.