View and send messages using the on-screen keyboard or predefined quick replies. The keyboard supports placeholders that insert live sensor data — `{time}` and `{loc}` are always available; additional placeholders (`{temp}`, `{hum}`, `{pres}`, `{batt}`, `{alt}`, `{lux}`, `{co2}`) appear automatically for sensors that are connected and returning data.

Each message in the history list shows the sender name and a compact age indicator (`3m`, `2h`, `>1d`) in the top-right corner of the entry.

```
╔══════════════════════════════╗
║          #general            ║
╠══════════════════════════════╣
║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║  ← selected
║ Alice                   3m   ║
║ Hey, let's meet tomorrow     ║
║┌───────────────────────────┐ ║
║│▓▓▓▓▓▓▓▓▓ Bob      1h ▓▓▓▓ │ ║
║│ Sure, what time works?    │ ║
║└───────────────────────────┘ ║
║[+ send]                      ║
╚══════════════════════════════╝
```

Press Enter on a message to open it in fullscreen. Navigate between messages with left (newer) and right (older). If the message is a reply addressed to someone (`@[nick]`), a **To: nick** bar is shown below the sender name and the body is displayed without the address prefix.

```
╔══════════════════════════════╗
║▓▓ Alice ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║  ← sender
║▓▓ To: Bob ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║  ← recipient
╠══════════════════════════════╣
║ Hey Bob, let's meet up       ║
║ tomorrow at 6pm downtown.    ║
║ Will you make it?            ║
║                              ║
║< newer                older >║
╚══════════════════════════════╝
```

Hold Enter on a message to open a context menu. From the list or fullscreen view, select **Reply** to pre-fill the keyboard or a quick message with `@[nick]` so the recipient is clearly addressed. The reply picker title shows the recipient's name.

```
╔══════════════════════════════╗
║           RE:Alice           ║
╠══════════════════════════════╣
║> Custom message...           ║
║  OK, I understand            ║
║  On my way                   ║
║  Be right there              ║
╚══════════════════════════════╝
```

On channel or contact list entries, the context menu also lets you change per-channel notification settings (mute, follow global, or force-on), per-channel melody override (follow global, Melody 1, or Melody 2), or mark messages as read.

**Mark all read** — Hold Enter on the DM / Channels / Rooms mode-select screen to clear all unread counters for the highlighted category at once.