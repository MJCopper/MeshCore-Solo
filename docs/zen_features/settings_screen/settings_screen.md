## Settings Screen

[Go back](../../../README.md)

### Overview

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./overview_oled.png) | ![](./overview_eink.png) |

All settings are saved to flash and restored on next boot. The Settings home card lists the available sections directly. Use **UP/DOWN** to select a section and press **Enter** to open its dedicated screen. Only that section's settings are shown; the old collapsible `+`/`-` section list is not used. Press **LEFT/RIGHT** to change a value, or **Enter** for toggle items.

Press **Cancel/Back** to save and return to the home screen. Settings are written
only if their final values differ from those present when Settings was opened;
cycling a value back to its original choice does not cause a flash write.

---

### Display

| Setting                              | Options                          | Notes                                                                                                 |
| ------------------------------------ | -------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Brightness                           | 1–5                              | LEFT/RIGHT; preview applies immediately                                                               |
| Auto-off                             | 5 s / 15 s / 30 s / 60 s / Never | LEFT/RIGHT                                                                                            |
| Battery                              | Icon / % / V                     | Display mode for the top-bar battery indicator                                                        |
| Clock seconds                        | On / Off                         | Turning seconds off reduces OLED refresh from 1 s to 60 s                                             |
| Clock format                         | 24 h / 12 h                      | 12 h appends AM/PM                                                                                    |
| Display rotation _(e-ink only)_      | 0° / 90° / 180° / 270°           | Applied immediately                                                                                   |
| Joystick rotation _(e-ink only)_     | 0° / 90° / 180° / 270°           | Rotates input mapping independently of display rotation; useful for custom enclosures                 |
| Full refresh interval _(e-ink only)_ | Off / 5 / 10 / 20 / 30           | Partial refreshes between full clears; reduces ghosting on long sessions                              |

---

### Sound

| Setting        | Options                        | Notes                                                        |
| -------------- | ------------------------------ | ------------------------------------------------------------ |
| Buzzer         | On / Off / Auto                | Auto: silences while BLE connected, re-enables on disconnect |
| Volume         | 1–5                            | LEFT/RIGHT; preview tone plays on each change                |
| DM Melody      | built-in / Melody 1 / Melody 2 / None | Notification sound for incoming private messages. `None` disables the sound for this event. |
| Channel Melody | built-in / Melody 1 / Melody 2 / None | Notification sound for incoming channel messages. `None` disables the sound for this event. |
| AD sound       | built-in / Melody 1 / Melody 2 / None | Sound played whenever an **advert** is received from *any* node — pairs with automatic adverts as an audible "in range" heartbeat. `None` disables the sound for this event. |
| AD scope       | All / Zero-hop                | Filters the AD sound so it plays for every advert or only for local zero-hop adverts. |
| Quiet Time     | Off / On / Active             | Enables the daily schedule; Active means the current local time is inside it |
| Quiet from     | 21:00                         | Start of Quiet Time in the configured local timezone |
| Quiet until    | 07:00                         | End of Quiet Time in the configured local timezone |

Melody 1 and Melody 2 are custom sequences editable in **Tools › Ringtone Editor**.

### Advert

Advert has its own entry on the Settings home card.

| Setting     | Options                         | Notes |
| ----------- | ------------------------------- | ----- |
| Auto Advert | Off / 1 hour / 3 hours / 6 hours | Periodically sends a zero-hop advert so nearby nodes can discover this device. |
| GPS Details | Hide / Share                    | Controls whether coordinates are included in every self advert, including automatic adverts and adverts triggered manually on the device or by a companion app. |

All self-advert entry points use this same location policy. Whenever an advert
is actually queued, the top-bar advert icon appears steadily for five seconds;
it is not shown merely because Auto Advert is enabled and it does not flash.

See [Quiet Time](../quiet_time/quiet_time.md) for schedule behaviour and time editing.

---

### Home Pages

|           OLED            |           E-Ink           |
| :-----------------------: | :-----------------------: |
| ![](./homepages_oled.png) | ![](./homepages_eink.png) |

Lists the configurable home screen pages. For each entry:

- **LEFT / RIGHT** — move the page earlier or later in the navigation sequence
- **Enter** — toggle the page ON / OFF

**Clock** is always the first home screen and is not shown in this list.
**Settings** and **Messages** are always visible and cannot be disabled.
Position numbers begin at **1** for the first configurable page after Clock.
The default order is **Messages**, **Favourites**, **GPS**, **Advert**,
**Bluetooth**, **Radio**, **Tools**, then **Settings**.

---

### Radio

| Setting   | Options    | Notes                                                                                                                                                              |
| --------- | ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| TX Pwr    | 2–22 dBm   | LEFT/RIGHT. Sets the fixed transmission power. |
| Preset    | named presets | LEFT/RIGHT cycles community RF presets (region frequency + bandwidth/SF/CR). **Enter** opens a popup to pick one, save the current settings as a named preset, or delete a saved one. Applies frequency, bandwidth, SF and CR together. |
| Freq      | chip range | **Enter** opens a digit-by-digit editor: LEFT/RIGHT moves between decimal places, UP/DOWN steps that digit. Bounds come from the radio chip's own validated range, so a value the radio would reject can't be entered. |
| SF        | 5–12       | LEFT/RIGHT. Spreading factor. |
| BW        | 7.8–500 kHz | LEFT/RIGHT cycles the standard LoRa bandwidths. |
| CR        | 5–8        | LEFT/RIGHT. Coding rate (4/5–4/8). |

The **repeater** mode and radio timing controls live on their own screen — see **Tools › Repeater**.

---

### System

| Setting     | Options                                             | Notes                                                                                  |
| ----------- | --------------------------------------------------- | -------------------------------------------------------------------------------------- |
| Name        | keyboard entry (up to 31 chars)                     | This device's node name, shown to others and in every advert. **Enter** opens the keyboard pre-filled with the current name; the final change is saved when Settings is exited |
| Timezone    | −12 h … +14 h                                       | UTC offset in whole hours                                                              |
| GPS         | Off / Continuous / 2 min / 5 min / 15 min / 30 min / 1 h / 3 h / 6 h | Continuous keeps the receiver on. Timed modes wake it to acquire and stabilise a fix, cache the position/time, then power it down until the selected interval |
| Bluetooth   | On / Off                                            | Persistently enables or disables Bluetooth; the USB companion interface is unaffected   |
| Low Battery | Off / 3.0 V / 3.1 V / 3.2 V / 3.3 V / 3.4 V / 3.5 V | Auto-shutdown threshold; also sets the 0 % anchor for the battery percentage indicator |
| Units       | Metric / Imperial                                   | Unit system for distance values shown by supported screens such as Nearby Nodes. Metric uses m/km; Imperial uses ft/mi. |
| Reboot      | action (**Enter**)                                  | Restarts this device. Pending setting changes are saved first. Last row, so it isn't the default-selected one |

GPS selections made here remain staged while Settings is open. Pressing
**Cancel/Back** applies the selected mode and saves it, so cycling the choices
does not repeatedly reconfigure the receiver or display GPS alert popups.

The GPS home card shows the selected operating mode and whether the receiver is
searching, has a fix, or is sleeping. Press **Enter** on that card to cycle the
same modes. A timed interval begins after the previous acquisition finishes;
the last valid coordinates remain available while the receiver sleeps.

---

### Keyboard

| Setting  | Options    | Notes                                                                                              |
| -------- | ---------- | -------------------------------------------------------------------------------------------------- |
| Type     | ABC / T9   | English on-screen keyboard style; default **T9**. **ABC** provides one key per letter. In message fields, **T9** predicts words from phone-keypad digit sequences; literal fields retain traditional multi-tap. CardKB remains direct QWERTY. |
| CardKB | Found / Missing | Read-only boot detection and connection status for an optional Grove CardKB |

The selected type applies consistently to message composition, quick-message
editing, room passwords, device names and preset names. Earlier releases called
the grid QWERTY; the on-screen layout is alphabetical and is now named **ABC**.

Message composition also provides a small common-word completion dictionary.
In the Full editor, the best match is shown as uncommitted text after the
underscore cursor (for example, `hel_lo`). In Compact CardKB mode the hint shows
comma-separated candidates, clipped at the display edge when the final word
does not fit.
Type the beginning of a word, then hold **Enter** (or press **Tab** on CardKB)
to see up to eight matches under **Complete:**. Selecting one replaces the
whole word at the cursor and appends a space. Completion is limited to message
text; passwords, names and configuration fields are never suggested or
modified. The dictionary contains 4,000 frequency-ranked conversational words,
based on the SUBTLEX-US spoken-English corpus with a 130-entry Australian
localisation layer covering spellings and everyday vocabulary. Proper names,
corpus fragments, profanity and explicit adult or violent terms are excluded.

With the full on-screen **T9** layout, pressing keys 2–9 builds a predictive
digit sequence. Characters covered by the entered digits are shown normally and
the remainder of a longer completion is shown after the cursor as a ghost
suffix. Exact-length words are preferred over longer completions. Press
**Back** while a prediction is active to accept it and insert a space without
navigating away from the number grid. Hold **Enter** to choose from up to eight
alternatives; choosing one also inserts a space. The hint line shows the next
alternatives that fit.

Backspace removes the latest digit. Space, Done, cursor movement or a page
change commits the provisional word. An unmatched sequence opens **Spell word**
and **Cancel word** recovery choices instead of silently rejecting its final
key. **Spell word** enters the existing literal multi-tap mode. The page key
cycles **T9 → #@ → abc → T9**, and key 1 and the symbols page retain multi-tap.
Common contractions are predicted without requiring an apostrophe key, and up
to eight words selected during the current session receive ranking priority;
this temporary history is never written to flash. Predictive entry is limited
to message text; names, passwords and other literal fields always use multi-tap
in T9 layout. CardKB remains direct QWERTY input in either setting.

The smiley special key opens the emoji picker for 👍, 👎, 🙂 and 🙁.

See [CardKB](../cardkb/cardkb.md) for connection, controls and polling behaviour.

---

### Contacts

| Setting  | Options          | Notes                                                |
| -------- | ---------------- | ---------------------------------------------------- |
| DMs      | All / Favourites | **All** shows every eligible direct-message contact; **Favourites** shows only upstream-starred eligible DM contacts. Rooms and repeaters remain in their own lists |
| Channels | All / Favourites | Show all channels or only favourited ones            |
| Rooms    | All / Favourites | Show all room servers or only favourited ones        |

---

### Child Mode

| Setting    | Options  | Notes                                                                    |
| ---------- | -------- | ------------------------------------------------------------------------ |
| Enabled    | On / Off | Enables the restricted interface; requires a saved PIN and confirmation  |
| Set PIN    | 000000–999999 | Enter the six-digit parent PIN twice; the saved value is not displayed |
| Channels   | On / Off | Shows favourited private channels in Messages; Public and # channels remain hidden; default Off |
| Favourites | On / Off | Allows the Favourites Dial; default On                                   |

While Child Mode is locked, selecting Settings opens the parent PIN prompt. A successful PIN entry temporarily restores the full Settings screen and companion access. Leaving Settings or allowing the display to sleep ends the parent session.

See [Child Mode](../child_mode/child_mode.md) for preparation, restrictions, GPS/advert behaviour, and PIN recovery details.

---

### Messages

Up to 10 quick reply templates (Q1–Q10). Press **Enter** on a slot to open the keyboard editor. Supports the same placeholders as the main keyboard (`{time}`, `{loc}`, and sensor placeholders when connected).

On-device direct-message delivery uses a fixed policy. When a known path exists,
Zen tries it twice in total, clears it after both attempts fail, then tries
three times by flood. With no known path, it tries three times by flood in total.
