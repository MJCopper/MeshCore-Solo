#pragma once

#include <stddef.h>

// Stable byte counts for the historical Solo preference tails. These describe
// the serialized file format, not sizeof(NodePrefs), so compiler padding cannot
// silently change migration decisions.
namespace soloprefs {

static const size_t SENTINEL_BYTES = 4;
static const size_t LEGACY_CHILD_TAIL_BYTES = 1 + 4 + 2 + SENTINEL_BYTES;
static const size_t CHILD_TAIL_BYTES = 1 + 4 + 2 + 1 + SENTINEL_BYTES;
static const size_t QUIET_TAIL_BYTES = 1 + 2 + 2 + SENTINEL_BYTES;
static const size_t NEWER_UPSTREAM_TAIL_BYTES = 8;

// Historical combined layout through keyboard_main_alphabet, followed by the
// Child Mode and Quiet Time payloads and sentinel. Keep this explicit: it must
// continue describing files already deployed even if NodePrefs fields change.
static const size_t LEGACY_COMBINED_TAIL_BYTES =
    1 + 1 + 1 + 1 + 6 + 64 + 140 + 1 + 1 + 1 +
    1 + 4 + 2 + 1 + 1 + 2 + 2 + SENTINEL_BYTES;

enum class InitialTail {
  CURRENT_OR_UPSTREAM,
  LEGACY_CHILD,
  LEGACY_COMBINED
};

static inline InitialTail classifyInitialTail(size_t remaining) {
  if (remaining == LEGACY_CHILD_TAIL_BYTES) return InitialTail::LEGACY_CHILD;
  if (remaining == LEGACY_COMBINED_TAIL_BYTES) return InitialTail::LEGACY_COMBINED;
  return InitialTail::CURRENT_OR_UPSTREAM;
}

static inline bool isStandaloneQuietTail(size_t remaining) {
  return remaining == QUIET_TAIL_BYTES;
}

static inline bool hasCompleteTail(size_t remaining, size_t tail_bytes) {
  return remaining >= tail_bytes;
}

} // namespace soloprefs
