## Favourites Dial

[Go back](../../../README.md)

### Overview

|            OLED            |           E-Ink            |
| :------------------------: | :------------------------: |
| ![](./overview_oled.png) | ![](./overview_eink.png) |

A dedicated home page showing a grid of up to 6 pinned contacts for quick access. The layout adapts to the display orientation:

- **Portrait** (OLED, e-ink portrait) — 2 columns × 3 rows
- **Landscape** (e-ink landscape) — 3 columns × 2 rows

---

### Navigation

Navigate tiles with **UP / DOWN / LEFT / RIGHT**. Pressing a directional key at the edge of the grid switches to the adjacent home page instead of wrapping.

**Enter on a filled tile** — opens that contact's DM directly.

**Enter on an empty tile (`+`)** — opens a contact picker to fill the slot.

---

### Unread badge

Filled tiles show an unread message count in the top-right corner when there are unread DMs from that contact. The contact name is ellipsized to make room for the badge.

If a pinned contact has been removed from the contacts list, the tile shows `(gone)` until the slot is reassigned.

---

### Pinning a contact

**From the Favourites Dial** — press **Enter** on an empty tile (`+`). A picker opens showing:

1. Contacts marked as favourites in the upstream app (starred contacts) — listed first
2. Recent DM contacts — listed after
3. All remaining chat contacts — fallback when the first two tiers are empty (e.g. fresh install before any DMs)

|            OLED            |           E-Ink            |
| :------------------------: | :------------------------: |
| ![](./picker_oled.png) | ![](./picker_eink.png) |

<!-- screenshot pending: Pin-to-dial contact picker (favourites first, then recent DMs, then all contacts) -->

Select a contact to pin it to that slot.

**From a DM conversation** — **Hold Enter** › context menu › **Pin to dial**, then choose a slot from the slot picker (Slot 1–6, showing the current occupant name or "empty").

If the selected contact is already pinned in another slot, it is moved to the new slot automatically.

---

### Unpinning a contact

Open the contact's DM, **Hold Enter** › context menu › **Unpin (slot N)**.

---

### Reordering the Favourites page

The position of the Favourites Dial in the home page navigation sequence can be changed in **Settings › Home Pages** — press **LEFT / RIGHT** on the Favourites entry to move it earlier or later.
