## Messages Screen

[Go back](../../../README.md)

### Overview

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./overview_oled.png) | ![](./overview_eink.png) |

The Messages screen is split into three modes — **DMs**, **Channels**, and **Rooms** — selectable with UP/DOWN on the mode-select screen. Each mode shows the corresponding list of conversations with unread counters.

---

### Sending messages

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./compose_oled.png) | ![](./compose_eink.png) |

Press **Enter** on a contact or channel to open its history, then press **Enter** again (or select an empty send row) to compose a message. Choose between:

- **Custom message** — opens the on-screen keyboard
- **Q1–Q10** — quick reply templates editable in Settings › Messages

The keyboard supports placeholders that insert live data at send time:

| Placeholder | Value                | Availability                |
| ----------- | -------------------- | --------------------------- |
| `{time}`    | current time (HH:MM) | always                      |
| `{loc}`     | GPS coordinates      | always ("no GPS" if no fix) |
| `{temp}`    | temperature          | sensor connected            |
| `{hum}`     | humidity             | sensor connected            |
| `{pres}`    | barometric pressure  | sensor connected            |
| `{alt}`     | altitude             | sensor connected            |
| `{lux}`     | luminosity           | sensor connected            |
| `{co2}`     | CO₂ concentration    | sensor connected            |

Sensor placeholders appear automatically in the placeholder picker when the corresponding sensor is active. `{time}` and `{loc}` are always shown.

---

### Message history

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./history_oled.png) | ![](./history_eink.png) |

Each entry in the history list shows the sender name and a compact age indicator (`3m`, `2h`, `>1d`) in the top-right corner.

**Short Enter** on a message opens it in fullscreen. **Hold Enter** — on a history row or in fullscreen — opens the same options menu: Reply, plus **Navigate** / **Save waypoint** when the message contains a location (see Fullscreen message view). You don't need to open the message first.

---

### Fullscreen message view

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./fullscreen_oled.png) | ![](./fullscreen_eink.png) |

Navigate between messages with **LEFT** (newer) and **RIGHT** (older). Long messages scroll with **UP/DOWN**.

If the message is a reply addressed to someone (`@[nick]`), a **To: nick** bar is shown below the sender name and the body is displayed without the address prefix.

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./fullscreen_menu_oled.png) | ![](./fullscreen_menu_eink.png) |

**Hold Enter** in fullscreen opens the options menu. It always offers **Reply** for an incoming message, and when the message contains a **location** it adds two more:

- **Navigate** — opens the bearing/distance view to those coordinates (the same two-bearing screen as Waypoints and Nearby; **Back** returns to the message).
- **Save waypoint** — stores the location as a waypoint (visible on the trail map and in the Waypoints list).

A location is any `lat,lon` pair in the text — exactly what the `{loc}` placeholder inserts — so you can navigate to anything a contact shares. A `[WAY]lat,lon label` share also carries a name, used as the waypoint label. This works on DMs and channel messages, incoming or outgoing.

---

### Context menu — contact list

**Hold Enter** on a contact entry opens a context menu:

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./ctx_contact_oled.png) | ![](./ctx_contact_eink.png) |

| Item                         | Action                                                                         |
| ---------------------------- | ------------------------------------------------------------------------------ |
| Mark as read                 | Clears unread counter for this contact                                         |
| Notif: default / OFF / ON    | Per-contact notification override — **LEFT/RIGHT** to cycle                    |
| Melody: global / M1 / M2     | Per-contact melody override — **LEFT/RIGHT** to cycle                          |
| Pin to dial / Unpin (slot N) | Pin this contact to a Favourites Dial slot; if already pinned shows which slot |

When **Pin to dial** is selected, a slot picker opens (Slot 1–6 showing current occupant name or "empty"). Choosing a slot that already holds another contact moves the new contact there.

---

### Context menu — channel list

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./ctx_channel_oled.png) | ![](./ctx_channel_eink.png) |

**Hold Enter** on a channel entry opens a context menu:

| Item                      | Action                                                                |
| ------------------------- | --------------------------------------------------------------------- |
| Mark all read             | Clears all unread for this channel                                    |
| Notif: default / OFF / ON | Per-channel notification override — **LEFT/RIGHT** to cycle           |
| Melody: global / M1 / M2  | Per-channel melody override — **LEFT/RIGHT** to cycle                 |
| Fav: yes / no             | Add or remove this channel from favourites — **LEFT/RIGHT** to toggle |

---

### Mark all read

**Hold Enter** on the DM / Channels / Rooms mode-select screen to clear all unread counters for the highlighted category at once.
