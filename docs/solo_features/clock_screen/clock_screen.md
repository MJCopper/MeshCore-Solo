## Clock Screen

[Go back](../../../README.md)

### Overview

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./overview_oled.png) | ![](./overview_eink.png) |

A clock page on the home carousel. It uses the same node-name, status, battery
and page-indicator bars as the other carousel pages, followed by the current
time and date with up to two configurable data fields below.

Clock is always enabled and is always the first home carousel page. Pressing
**Cancel/Back** on any other home carousel page returns directly to Clock.
Cancel still closes an active popup before this shortcut is applied.

If another home carousel page is left idle for five minutes, the display
returns to Clock automatically. Only normal UI refresh scheduling is used, so
this does not add a background polling loop or keep the display awake.

Press **Enter** on Clock to open the transcript containing the newest unread
message. If nothing is unread, it opens the most recently active sent-or-received
transcript instead. Direct messages, room posts and channel messages are all
supported. Conversations unavailable under Child Mode are skipped.

Time is synchronized from GPS or via the companion app. The initial GPS attempt
may run for five minutes. While the clock remains unsynchronised, a GPS receiver
configured off is then retried once per hour for up to 90 seconds, with all
automatic retries stopping 24 hours after boot. The receiver is powered down
between attempts. Timezone offset is applied from **Settings › System**.

If no time source is available, the screen shows _"! No time sync"_ with a hint to enable GPS or connect the app.

---

### Time display

- **Format** — 24 h or 12 h with AM/PM; configurable in **Settings › Display**
- **Seconds** — shown by default on OLED; hidden on e-ink (always) and optionally on OLED via **Settings › Display › Clock seconds**; hiding reduces the refresh rate from 1 s to 60 s

---

### Data fields

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./fields_oled.png) | ![](./fields_eink.png) |

Up to two data fields are shown below the date separator. Each field displays a label and a value on the same line.

| Field       | Label | Value                                                                     |
| ----------- | ----- | ------------------------------------------------------------------------- |
| None        | —     | —                                                                         |
| Batt V      | Batt  | Battery voltage (e.g. `3.92V`)                                            |
| Batt %      | Batt  | Battery percentage using LiPo curve anchored at the low-battery threshold |
| Temperature | Temp  | °C from onboard sensor                                                    |
| Humidity    | Hum   | % from onboard sensor                                                     |
| Pressure    | Pres  | hPa from onboard sensor                                                   |
| GPS         | GPS   | `lat lon` decimal degrees, or `no fix`                                    |
| Altitude    | Alt   | metres from onboard sensor (GPS or barometric)                            |
| Luminosity  | Lux   | lux from onboard sensor                                                   |
| CO₂         | CO2   | ppm from onboard sensor                                                   |
| Contacts    | Nodes | Total contacts in the mesh                                                |
| Messages    | Msgs  | Total unread message count                                                |

Sensor fields show `--` when the sensor is not connected or has no data.

---

### Configuring fields

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./config_oled.png) | ![](./config_eink.png) |

<!-- screenshot pending: Dashboard Config — two field slots cycled with LEFT/RIGHT -->

**Hold Enter** (or press the **Context menu** key) on the Clock page to open the Dashboard Config screen, where each of the two field slots can be cycled with **LEFT/RIGHT**.

---

### Clock tools

Alarm, Timer and Stopwatch are not included in this focused build.
---

## Boot time synchronisation

After every boot the Clock page shows **SYNC** until an authoritative live time
update arrives from GPS, a companion connection or another network source. If
GPS is configured off, the firmware powers it temporarily in the background,
requests a time update, then powers it off again after synchronisation. A
bounded GPS attempt prevents an indoor device from leaving the receiver on
indefinitely; **SYNC** remains until another source supplies the time.

Only the clock and date region is replaced by **SYNC**. The horizontal separator
and both configured dashboard fields remain visible and continue updating.
