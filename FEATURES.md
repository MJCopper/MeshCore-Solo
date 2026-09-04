# MeshCore Zen features

MeshCore Zen is a Wio Tracker L1 companion firmware based on upstream MeshCore.
It keeps the normal mesh protocol and companion-radio behaviour while adding a
standalone, joystick-friendly interface for messaging without a phone.

Current Zen release: **v2.26**, based on **MeshCore v1.17.1**.

## Included

| Area | Zen functionality |
| ---- | ----------------- |
| Home | Clock is permanently first; Messages and Settings are always available; other supported cards can be enabled and reordered |
| Messages | Direct, channel and room transcripts; complete wrapped messages; one-line scrolling; unread tracking; quick replies; targeted `@[name]` replies in rooms and channels |
| Delivery | Fixed path/flood retry policy, live route/transmission markers, channel echo counts and manual resend of the latest failed message |
| Text entry | EN-US ABC and predictive T9, 2,500-word Australianised completion dictionary, UTF-8 cursor editing, Latin accents and four insertable chat emoji |
| Emoji display | Monochrome rendering for common emoji with a diamond fallback for unsupported glyphs |
| CardKB | Boot detection on Grove I2C, direct text input, compact editor layout and polling only while the display is awake |
| Favourites | Four-entry single-column dial with unread badges and long-press Add/Change/Remove actions |
| Clock and GPS | Boot-time time synchronisation, hourly bounded retries for 48 hours, persistent SYNC indication until time is set, timed GPS modes, timezone handling and unread-message shortcut |
| Notifications | Per-contact/channel controls, custom melodies, five-second notification wake, Quiet Time sound suppression and Child Mode filtering |
| Child Mode | Parent PIN, favourite-only contacts and rooms, optional favourited private channels, optional Favourites page and disabled BLE/USB companion access while locked |
| Advert | Off/1 h/3 h/6 h automatic interval, shared GPS privacy policy and a steady five-second sent-advert indicator |
| Radio | Presets, manual LoRa parameters, TX power and Adaptive Power Control; base MeshCore RX behaviour is retained |
| Repeater | Companion repeater using the current radio settings with fixed standard-backend timing, Yield x2 and duplicate suppression enabled |
| Tools | Nearby Nodes, Repeater, Ringtone Editor and Diagnostics |
| Display | OLED and e-ink layouts, rotation controls where supported and power-aware refresh scheduling |
| Persistence | Save-on-exit settings with writes only when values changed; Zen preferences are isolated from the upstream MeshCore preference layout |

## Documentation

- [Feature overview and installation](./README.md)
- [Messages](./docs/zen_features/message_screen/message_screen.md)
- [Settings](./docs/zen_features/settings_screen/settings_screen.md)
- [Child Mode](./docs/zen_features/child_mode/child_mode.md)
- [Quiet Time](./docs/zen_features/quiet_time/quiet_time.md)
- [CardKB](./docs/zen_features/cardkb/cardkb.md)
- [Clock](./docs/zen_features/clock_screen/clock_screen.md)
- [Favourites Dial](./docs/zen_features/favourites_dial/favourites_dial.md)
- [Tools and repeater](./docs/zen_features/tools_screen/tools_screen.md)
- [Zen UI developer guide](./docs/design/zen_ui_framework.md)

Historical implementation notes belong in the release history and Git history;
this file describes only the current Zen feature set.
