#pragma once

#include <stdint.h>

namespace childmode {

static inline bool favouriteFlagSet(uint8_t flags) {
  return (flags & 0x01) != 0;
}

static inline bool contactIdentityMatches(uint8_t stored_type,
                                          uint8_t reported_type,
                                          uint8_t expected_type) {
  return stored_type == expected_type && reported_type == expected_type;
}

static inline bool favouriteChannelSet(uint64_t favourites, uint8_t index) {
  return index < 64 && (favourites & (1ULL << index)) != 0;
}

} // namespace childmode
