# Settings

[Back to README](../../../README.md)

Settings opens a flat section list. Each section has its own screen. Changes
are staged and saved on Back only when the final value differs from the saved
value.

## Display

Brightness, display timeout, battery format, clock seconds and 12/24-hour time.
E-ink builds also provide display rotation, joystick rotation and full-refresh
interval.

Battery percentage estimates usable charge from voltage: 0% at 3.3 V and 100%
at 4.2 V, using a non-linear single-cell Li-ion/LiPo discharge curve. The
device shuts down at or below 3.0 V unless externally powered.
At 20% or below, a Low Battery alert sounds one short beep, then repeats hourly
while on battery power. Quiet Time suppresses the beep; the visual notification
follows the message screen-wake rules. The alert does not add an unread message.
After three consecutive battery samples at 5% or below (about 24 seconds), Low
Power mode turns off GPS, Bluetooth and the radio, stops automatic
message retries and uses minimum OLED brightness with a five-second screen
timeout. CardKB remains available while the screen is on and is suspended with
the screen as usual. Low Power remains active despite voltage rebound and
restores the saved GPS, Bluetooth, radio and brightness settings only after
external power is connected.
Interrupted messages remain available for manual resend; they are not sent
automatically after restoration.

The final carousel card in Low Power is **Low Power Emergency**. Enter opens a
Yes/No confirmation for a volatile ten-minute emergency window. The radio and
messaging operate during that window. GPS and Bluetooth start off but can be
temporarily enabled from their carousel cards without changing their saved
settings. The page shows the remaining time. At expiry, GPS, Bluetooth and the
radio turn off, pending automatic retries stop, and normal Low Power restrictions
resume. The card always shows Radio, GPS and Bluetooth status. While active,
Enter opens a confirmation to end Emergency Mode early. The battery display
reaches 0% at 3.3 V; shutdown occurs at 3.0 V.

## Sound

Buzzer mode, volume, DM/channel/advert melodies, advert sound scope, and Quiet
Time schedule. Custom melodies are edited in **Tools › Ringtone Editor**.

## Advert

| Setting | Options |
| ------- | ------- |
| Auto Advert | Off / 1 hour / 3 hours / 6 hours |
| GPS Details | Hide / Share |

GPS Details applies to manual, automatic and companion-triggered self adverts.
The advert status icon appears steadily for five seconds after an advert is
queued.

## Home Pages

Enter toggles a page and Left/Right changes its order. Clock is fixed first;
Messages and Settings remain available. Position 1 is the first page after
Clock.

Default order: Messages, Favourites, GPS, Advert, Bluetooth, Radio, Sensors, Tools,
Settings.

Sensors lists saved sensor nodes. Open a node to fetch its telemetry; Up/Down
scrolls one value at a time, Enter refreshes and Back returns to the list.
Channels appear in descending order, with values kept in their original order
within each channel. Only telemetry labels and values are shown. Requests use
the node's existing path and access permissions; there is no background polling.
Sensors is enabled by default on fresh settings. Enable it here on existing
devices. It is hidden while Child Mode is locked.

## Radio

TX power, preset, frequency, spreading factor, bandwidth and coding rate. Preset
changes apply the frequency, bandwidth, SF and CR together. Frequency uses a
validated digit editor.

## System

| Setting | Options or action |
| ------- | ----------------- |
| Name | Up to 31 characters |
| Timezone | UTC−12 to UTC+14 |
| GPS | Off / Continuous / 2 min / 5 min / 15 min / 30 min / 1 h / 3 h / 6 h |
| Bluetooth | On / Off |
| Units | Metric / Imperial |
| Reboot | Save pending changes and restart |

Timed GPS modes acquire a stable fix, cache it, power down, then repeat after
the selected interval. GPS and Bluetooth changes are applied when Settings is
closed.

## Keyboard

Choose predictive **T9** (default) or **ABC**. Literal fields use multi-tap even
with T9 selected. **CardKB** is a read-only `Found`/`Missing` status.
To enter `0` in T9, switch to symbols and press the bottom-right key once.

Word completion is limited to messages. Its 4,000-word Australianised
dictionary excludes proper names, fragments and unsuitable terms. The smiley
key opens the four-entry emoji picker.

## Contacts

Direct messages, Channels and Rooms can each show **All** or **Favourites**.

## Child Mode

Set the six-digit PIN, enable the mode, and select whether favourited private
Channels and the Favourites home page remain visible. See
[Child Mode](../child_mode/child_mode.md).

## Messages

Edit quick-message templates Q1–Q10. Templates support the same time, location
and active-sensor placeholders as message composition.
