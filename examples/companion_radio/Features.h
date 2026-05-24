#pragma once

// Build-time feature flags derived from board defines.
// Use these instead of `#ifdef EINK_DISPLAY_MODEL` for runtime-shape decisions
// (booleans, timings, defaults). Compiler dead-branch-eliminates on the
// constexpr, so cost is identical to a preprocessor branch but the code
// remains readable and reachable to tooling. Conditional struct members
// (enum entries, NodePrefs fields) still need a real `#if defined(...)`.

namespace Features {

#if defined(EINK_DISPLAY_MODEL)
  static constexpr bool     IS_EINK              = true;
  static constexpr bool     BLINK_INDICATORS     = false;   // e-ink avoids high-rate redraws
  static constexpr bool     CLOCK_HIDE_SECONDS_DEFAULT = true;  // pref starting value
  static constexpr unsigned HOME_REFRESH_MS      = 30000;   // slow display polls less
  static constexpr unsigned LOCKSCREEN_REFRESH_MS = 30000;
#else
  static constexpr bool     IS_EINK              = false;
  static constexpr bool     BLINK_INDICATORS     = true;
  static constexpr bool     CLOCK_HIDE_SECONDS_DEFAULT = false;
  static constexpr unsigned HOME_REFRESH_MS      = 1000;
  static constexpr unsigned LOCKSCREEN_REFRESH_MS = 1000;
#endif

} // namespace Features
