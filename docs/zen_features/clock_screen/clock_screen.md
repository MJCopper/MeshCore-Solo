## Clock Screen

[Go back](../../../README.md)

### Overview

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./overview_oled.png) | ![](./overview_eink.png) |

A clock page on the home carousel. It uses the same node-name, status, battery
and page-indicator bars as the other carousel pages, followed by the current
time and date with a fixed unread-message count below.

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

Until an authoritative time source succeeds, only the clock and date region is
replaced by **SYNC**. The separator and unread-message line remain visible.

---

### Time display

- **Format** — 24 h or 12 h with AM/PM; configurable in **Settings › Display**
- **Seconds** — shown by default on OLED; hidden on e-ink (always) and optionally on OLED via **Settings › Display › Clock seconds**; hiding reduces the refresh rate from 1 s to 60 s

---

### Message count

One fixed line is shown below the date separator. **Messages** displays the
total unread count across direct messages, channels and room servers. This
line is always present and is not configurable.

---

Alarm, Timer and Stopwatch are intentionally not included in Zen.

---

## Boot time synchronisation

After every boot the Clock page shows **SYNC** until an authoritative live time
update arrives from GPS, a companion connection or another network source. If
GPS is configured off, the firmware powers it temporarily in the background,
requests a time update, then powers it off again after synchronisation. A
bounded GPS attempt prevents an indoor device from leaving the receiver on
indefinitely; **SYNC** remains until another source supplies the time.

Only the clock and date region is replaced by **SYNC**. The horizontal separator
and the unread **Messages** line remain visible and continue updating.
