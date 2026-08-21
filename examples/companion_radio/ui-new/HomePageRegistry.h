#pragma once

#include "../NodePrefs.h"
#include <stdint.h>
#include <string.h>

// Shared metadata and saved-order maintenance for the home carousel. Screen
// enums remain local to their UI classes; persisted bit IDs and policy live
// here so the settings editor and runtime carousel use the same rules.
namespace homepage {

static inline bool alwaysVisible(uint8_t bit) {
  return bit == NodePrefs::HPB_CLOCK || bit == NodePrefs::HPB_SETTINGS ||
         bit == NodePrefs::HPB_QUICK_MSG;
}

static inline bool childOptional(uint8_t bit) {
  return bit == NodePrefs::HPB_RECENT || bit == NodePrefs::HPB_FAVOURITES;
}

static inline bool childHidden(uint8_t bit) {
  return bit == NodePrefs::HPB_RADIO || bit == NodePrefs::HPB_BLUETOOTH ||
         bit == NodePrefs::HPB_ADVERT || bit == NodePrefs::HPB_GPS ||
         bit == NodePrefs::HPB_TOOLS;
}

static inline bool visible(const NodePrefs* prefs, uint8_t bit, bool child_locked) {
  if (alwaysVisible(bit)) return true;
  uint16_t mask = (prefs && prefs->home_pages_mask) ? prefs->home_pages_mask
                                                     : NodePrefs::HP_ALL;
  if (child_locked) {
    if (childOptional(bit))
      return prefs && (prefs->child_visible_pages & (uint16_t)(1U << bit));
    if (childHidden(bit)) return false;
  }
  return (mask & (uint16_t)(1U << bit)) != 0;
}

static inline int defaultOrder(uint8_t* order, int capacity) {
  if (!order || capacity <= 0) return 0;
  static const uint8_t BITS[] = {
    NodePrefs::HPB_CLOCK, NodePrefs::HPB_QUICK_MSG, NodePrefs::HPB_FAVOURITES,
#if ENV_INCLUDE_GPS == 1
    NodePrefs::HPB_GPS,
#endif
    NodePrefs::HPB_ADVERT, NodePrefs::HPB_BLUETOOTH, NodePrefs::HPB_RADIO,
    NodePrefs::HPB_TOOLS, NodePrefs::HPB_SETTINGS,
  };
  int count = 0;
  for (int i = 0; i < (int)(sizeof(BITS) / sizeof(BITS[0])) && count < capacity; i++)
    order[count++] = BITS[i];
  return count;
}

static inline void ensureOrder(NodePrefs* prefs) {
  if (!prefs) return;

  uint8_t required[NodePrefs::PAGE_ORDER_LEN];
  int required_count = defaultOrder(required, NodePrefs::PAGE_ORDER_LEN);
  bool valid = prefs->page_order_set == NodePrefs::PAGE_ORDER_MAGIC;
  uint16_t present = 0;
  int length = 0;
  if (valid) {
    for (; length < NodePrefs::PAGE_ORDER_LEN; length++) {
      uint8_t stored = prefs->page_order[length];
      if (stored < 1 || stored > NodePrefs::HPB_COUNT) break;
      uint16_t mask = (uint16_t)(1U << (stored - 1));
      if (present & mask) { valid = false; break; }
      present |= mask;
    }
    valid = valid && (present & (uint16_t)(1U << NodePrefs::HPB_CLOCK));
  }

  if (!valid) {
    memset(prefs->page_order, 0, sizeof(prefs->page_order));
    length = 0;
    present = 0;
  } else {
    // Clock is the fixed home anchor. Repair older custom orders that moved it
    // while preserving the relative order of every other page.
    int clock = -1;
    for (int i = 0; i < length; i++)
      if (prefs->page_order[i] == NodePrefs::HPB_CLOCK + 1) { clock = i; break; }
    if (clock > 0) {
      uint8_t stored_clock = prefs->page_order[clock];
      for (int i = clock; i > 0; i--) prefs->page_order[i] = prefs->page_order[i - 1];
      prefs->page_order[0] = stored_clock;
    }
  }

  if (valid && !(present & (uint16_t)(1U << NodePrefs::HPB_FAVOURITES))) {
    // Preserve the established migration rule: Favourites was introduced
    // immediately after Clock, rather than appearing at the end of an older
    // custom order.
    int clock = -1;
    for (int i = 0; i < length; i++)
      if (prefs->page_order[i] == NodePrefs::HPB_CLOCK + 1) { clock = i; break; }
    int insert = clock + 1;
    if (clock >= 0 && insert < NodePrefs::PAGE_ORDER_LEN) {
      int tail = length < NodePrefs::PAGE_ORDER_LEN ? length : NodePrefs::PAGE_ORDER_LEN - 1;
      for (int i = tail; i > insert; i--) prefs->page_order[i] = prefs->page_order[i - 1];
      prefs->page_order[insert] = NodePrefs::HPB_FAVOURITES + 1;
      if (length < NodePrefs::PAGE_ORDER_LEN) length++;
      present |= (uint16_t)(1U << NodePrefs::HPB_FAVOURITES);
    }
  }

  // Preserve a valid user order and append pages introduced by newer builds.
  for (int i = 0; i < required_count && length < NodePrefs::PAGE_ORDER_LEN; i++) {
    uint16_t mask = (uint16_t)(1U << required[i]);
    if (!(present & mask)) {
      prefs->page_order[length++] = required[i] + 1;
      present |= mask;
    }
  }
  while (length < NodePrefs::PAGE_ORDER_LEN) prefs->page_order[length++] = 0;
  prefs->page_order_set = NodePrefs::PAGE_ORDER_MAGIC;
}

static inline int position(const NodePrefs* prefs, uint8_t bit) {
  if (!prefs || prefs->page_order_set != NodePrefs::PAGE_ORDER_MAGIC) return 0;
  for (int i = 0; i < NodePrefs::PAGE_ORDER_LEN; i++) {
    uint8_t stored = prefs->page_order[i];
    if (stored < 1 || stored > NodePrefs::HPB_COUNT) break;
    if (stored - 1 == bit) return i + 1;
  }
  return 0;
}

static inline void move(NodePrefs* prefs, uint8_t bit, int delta) {
  ensureOrder(prefs);
  if (!prefs || bit == NodePrefs::HPB_CLOCK) return;
  int current = -1, count = 0;
  for (int i = 0; i < NodePrefs::PAGE_ORDER_LEN; i++) {
    uint8_t stored = prefs->page_order[i];
    if (stored < 1 || stored > NodePrefs::HPB_COUNT) break;
    if (stored - 1 == bit) current = i;
    count++;
  }
  int next = current + delta;
  if (current < 0 || next < 1 || next >= count) return;
  uint8_t temp = prefs->page_order[current];
  prefs->page_order[current] = prefs->page_order[next];
  prefs->page_order[next] = temp;
}

} // namespace homepage
