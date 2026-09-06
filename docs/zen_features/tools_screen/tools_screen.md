# Tools

[Back to README](../../../README.md)

| Tool | Purpose |
| ---- | ------- |
| Discover Repeaters | Scan for zero-hop repeaters and add results as contacts |
| Node List | Browse, filter, inspect, ping and manage known nodes |
| Repeater Mode | Toggle the companion repeater backend |
| Ringtone Editor | Edit and preview notification melodies |
| Diagnostics | View device, radio, mesh and runtime information |

Node List opens on **All**. Use Left/Right for the Fav, All, Comp, Rpt, Room and
Snsr filters; hold Enter for actions available to the selected node.

## Node administration

Select a saved repeater or room in Node List, hold Enter and choose **Admin**.
Zen first tries ACL login with an empty password, then offers password entry.
The remote node must grant admin rights. Credentials stay in RAM; Back returns
to the same Node List filter and position.

Menus provide Status, Settings, Radio, Repeater or Room options, Console and
Actions. Open a setting to fetch its current value. Finish editing with Enter
or Back, then choose Apply, Discard or Cancel. Unchanged values send nothing.
Radio changes warn that the node may become unreachable; Zen's radio stays unchanged.

Console uses predictive T9 with commands and keywords from the
[MeshCore CLI reference](https://docs.meshcore.io/cli_commands/) ranked ahead of
chat words. Dotted names such as `flood.advert.interval` are single candidates;
T9 supplies their dots automatically. Hold Enter (CardKB: Tab) opens alternatives.
ABC and CardKB use the same command-first completion list. Completing a word
adds a trailing space. Password and structured setting fields remain literal.

Actions and console commands require confirmation. **Confirmed** means the
remote node returned an OK response. **No reply** means the result is unknown
(including reboot); changes are never automatically retried. The console shows
other replies verbatim.

Admin is unavailable while Child Mode is locked. Leaving Admin clears its
session. Commands use tagged replies and current contact paths, without background
polling. Phone/USB commands retain priority; after overlap, cancellation or timeout,
wait at least one minute before sending another local command. App traffic may
extend this wait by its response timeout. Nodes must support MeshCore's echoed
CLI prefix; untagged replies cannot populate an editor.

Discover Repeaters starts a repeater-only scan immediately. Hold Enter on a
result to manage it or rescan.

Repeater Mode uses the current **Settings › Radio** configuration. Its fixed
backend values are RX delay `10`, flood/direct airtime factors `0.5`/`0.3`,
Yield `x2`, and duplicate suppression On. It continues in the background after
leaving Tools.
