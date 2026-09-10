# Messages

[Back to README](../../../README.md)

The Messages home page opens directly to **Direct Message**, **Channel** and
**Room Servers**, with unread badges. Hold Enter on a category to mark it read.

## Conversations

Select a recipient to open its full transcript. Sender names are inverted,
messages are separated by a rule, and Up/Down scroll one wrapped line at a time.
Messages sent from a connected companion app are added to the same direct,
channel and room histories and use the same delivery markers.

- **Enter** composes a message.
- **Hold Enter** opens quick replies and transcript actions.
- In channels and rooms, **Reply to…** selects one of the six latest senders and
  inserts `@[name] `.
- A failed latest message offers **Resend failed**; channels offer
  **Resend anyway** when no echo was heard.

Unread messages are cleared only when their transcript is visible while the
display is awake. A notification that wakes the display turns it off again
after five seconds unless a Tracker button is pressed.

## Delivery markers

Direct and room messages show `D`, `P` or `F` for direct, stored path or flood.
The number is the transmission count; a tick confirms delivery and `✗` marks an
exhausted send. Channels show the number of matching repeater echoes, such as
`2✓`, or `✗` when none are heard during the response window.

With a known path, Zen tries it twice, clears it, then makes up to three flood
attempts. Without a path it makes up to three flood attempts. An ACK stops the
sequence immediately.

## Text entry

The default on-screen layout is predictive T9; ABC is selectable under
**Settings › Keyboard**. Message fields provide:

- a 4,000-word Australianised completion dictionary;
- cursor-safe UTF-8 editing and encoded-length enforcement;
- up to eight completion choices, accepted with a trailing space;
- placeholders for time, GPS and active sensors;
- an emoji picker for 👍, 👎, 🙂 and 🙁.

Hold Enter opens word alternatives. In predictive T9, Back accepts the current
word and inserts a space. CardKB uses direct input, Tab for completion and Fn+M
for emoji.

## Rooms

Opening a room starts its login when required. Enter a password, or submit an
empty field for an open or ACL-controlled room. Successful passwords are saved
and reused after restart. A rejection discards the rejected password; a timeout
keeps it because the server may simply be unreachable.

For an ACL-only blank login with no reply, Zen opens the transcript after the
normal response window. The room server remains authoritative when a message is
sent. Hold Enter on a room to log in again or log out.

## Lists and context actions

Contact and channel menus provide read state, notification and melody controls.
Contacts, rooms and channels can be starred and are sorted first in their lists.
Contacts can separately be pinned to the four-slot Favourites Dial. Channels can
also be edited or deleted, and the Channels list can add Public, hashtag or
private channels.

Child Mode limits these lists to permitted favourites and blocks editing.
