#pragma once

// Solo feature selection is compile-time only. Keep feature intent separate
// from board capability: a feature may be enabled for the firmware while its
// hardware-dependent portion compiles out on a board that cannot provide it.
#if defined(FIRMWARE_SOLO_BUILD)
  #define SOLO_FEATURE_DEFAULT 1
#else
  #define SOLO_FEATURE_DEFAULT 0
#endif

// Each flag remains independently overrideable from a board/environment. This
// lets an upstream integration take only the modules it wants without editing
// shared headers or maintaining another family of near-identical manifests.
#ifndef SOLO_FEAT_CHILD_MODE
  #define SOLO_FEAT_CHILD_MODE SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_QUIET_TIME
  #define SOLO_FEAT_QUIET_TIME SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_CARDKB
  #define SOLO_FEAT_CARDKB SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_NAVIGATION
  #define SOLO_FEAT_NAVIGATION SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_REMOTE_BOT
  #define SOLO_FEAT_REMOTE_BOT SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_REPEATER
  #define SOLO_FEAT_REPEATER SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_GPIO
  #define SOLO_FEAT_GPIO SOLO_FEATURE_DEFAULT
#endif
#ifndef SOLO_FEAT_AUTOCOMPLETE
  #define SOLO_FEAT_AUTOCOMPLETE SOLO_FEATURE_DEFAULT
#endif

#if defined(EINK_DISPLAY_MODEL)
  #define SOLO_CAP_EINK              1
#else
  #define SOLO_CAP_EINK              0
#endif

#if defined(CARDKB_ADDRESS)
  #define SOLO_CAP_CARDKB            1
#else
  #define SOLO_CAP_CARDKB            0
#endif

#if defined(PIN_GPIO1)
  #define SOLO_CAP_USER_GPIO         1
#else
  #define SOLO_CAP_USER_GPIO         0
#endif

#if defined(PIN_BUZZER)
  #define SOLO_CAP_BUZZER            1
#else
  #define SOLO_CAP_BUZZER            0
#endif

namespace solo {

struct Features {
  static constexpr bool CHILD_MODE = SOLO_FEAT_CHILD_MODE;
  static constexpr bool QUIET_TIME = SOLO_FEAT_QUIET_TIME;
  static constexpr bool CARDKB = SOLO_FEAT_CARDKB && SOLO_CAP_CARDKB;
  static constexpr bool NAVIGATION = SOLO_FEAT_NAVIGATION;
  static constexpr bool REMOTE_BOT = SOLO_FEAT_REMOTE_BOT;
  static constexpr bool REPEATER = SOLO_FEAT_REPEATER;
  static constexpr bool GPIO = SOLO_FEAT_GPIO && SOLO_CAP_USER_GPIO;
  static constexpr bool AUTOCOMPLETE = SOLO_FEAT_AUTOCOMPLETE;
};

struct Capabilities {
  static constexpr bool EINK = SOLO_CAP_EINK;
  static constexpr bool CARDKB = SOLO_CAP_CARDKB;
  static constexpr bool USER_GPIO = SOLO_CAP_USER_GPIO;
  static constexpr bool BUZZER = SOLO_CAP_BUZZER;
};

} // namespace solo
