## Quiet Time

[Go back](../../../README.md)

Quiet Time silences incoming notification presentation during a daily local-time interval. Messages continue to be received, stored and counted as unread, but do not show a pop-up alert, wake the display, sound the buzzer or trigger vibration.

Configure it under **Settings › Sound**:

| Setting     | Default | Description                                |
| ----------- | ------- | ------------------------------------------ |
| Quiet Time  | Off     | Enables or disables the daily schedule     |
| Quiet from  | 21:00   | Start of the quiet interval, in local time |
| Quiet until | 07:00   | End of the quiet interval, in local time   |

Press **Enter** on either time, use **LEFT/RIGHT** to select a digit, **UP/DOWN** to change it, then press **Enter** to accept or **Cancel** to discard the edit.

The device RTC stores UTC. Quiet Time applies the timezone configured under **Settings › System › TimeZone** before comparing the schedule. For example, with `UTC+10`, a configured start of `21:00` begins when the RTC reaches `11:00 UTC`.

An interval such as `21:00–07:00` crosses midnight. An interval such as `09:00–15:00` starts and ends on the same local day. Equal start and end times disable the interval even when Quiet Time is set to On.

Quiet Time remains inactive until the RTC has a valid synchronised time. This prevents an unset clock from unexpectedly silencing the device.

### What is silenced

Quiet Time suppresses presentation for incoming direct messages, room messages, channel messages and received adverts. Their normal message history and unread state remain available.

Delivery acknowledgements, key feedback, alarms and countdown timers are not silenced. Message routing, acknowledgements and the companion offline queue are unchanged.
