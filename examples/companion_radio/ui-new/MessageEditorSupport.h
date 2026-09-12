#pragma once

// Shared setup for fields that contain message text. KeyboardWidget owns the
// full/compact layout decision; this module adds the same word completion and
// live message placeholders to every message editor without affecting literal
// fields such as names, passwords, or preset labels.

#include "KeyboardWidget.h"
#include "SensorPlaceholders.h"
#include "../Features.h"
#include "../solo/MessageTextPolicy.h"
#if SOLO_FEAT_AUTOCOMPLETE
#include "../solo/WordCompleter.h"
#include "../solo/ContextPredictor.h"
#endif

namespace messageeditor {

// Attempts 4+ append an extended-attempt byte after the message terminator.
// Reserve those two bytes up front so every message accepted by the editor can
// be sent by every stage of the fixed retry policy.
static const int SEND_TEXT_LIMIT = MAX_TEXT_LEN - 2;

#if SOLO_FEAT_AUTOCOMPLETE
inline uint8_t suggest(const char* text, size_t word_start,
                       const char* prefix, size_t prefix_len,
                       char matches[][solo::WordCompleter::MAX_WORD_LEN]) {
  uint8_t count = solo::ContextPredictor::suggestPrefix(
      text, word_start, prefix, prefix_len, matches,
      solo::WordCompleter::MAX_SUGGESTIONS);
  char fallback[solo::WordCompleter::MAX_SUGGESTIONS]
               [solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t fallback_count = solo::WordCompleter::suggest(
      prefix, prefix_len, fallback, solo::WordCompleter::MAX_SUGGESTIONS);
  for (uint8_t i = 0; i < fallback_count &&
                      count < solo::WordCompleter::MAX_SUGGESTIONS; i++) {
    bool duplicate = false;
    for (uint8_t j = 0; j < count; j++)
      if (strcmp(matches[j], fallback[i]) == 0) { duplicate = true; break; }
    if (!duplicate)
      memcpy(matches[count++], fallback[i], solo::WordCompleter::MAX_WORD_LEN);
  }
  return count;
}

inline void refreshCompletions(KeyboardWidget& kb, void* ctx) {
  SensorManager* sensors = static_cast<SensorManager*>(ctx);
  solo::WordCompleter::WordRange word = solo::WordCompleter::currentWord(
      kb.buf, (size_t)kb.len, (size_t)kb.cursor_pos);
  kb.clearPlaceholders();
  kb.setCompletionRange((int)word.start, (int)word.end);
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN];
  uint8_t count = suggest(kb.buf, word.start, kb.buf + word.start,
                          (size_t)kb.cursor_pos - word.start, matches);
  for (uint8_t i = 0; i < count; i++) kb.addPlaceholder(matches[i]);
  kbAddSensorPlaceholders(kb, sensors);
}

inline bool previewCompletion(const KeyboardWidget& kb, void*,
                              char* word, size_t word_size,
                              char* suffix, size_t suffix_size,
                              char* candidates, size_t candidates_size) {
  if (!word || !suffix || !candidates || word_size == 0 ||
      suffix_size == 0 || candidates_size == 0) return false;
  word[0] = suffix[0] = candidates[0] = '\0';
  // Keep the live completion unambiguous: only suggest while appending at the
  // end. The popup still supports replacing a word around a moved cursor.
  if (kb.cursor_pos != kb.len) return false;
  solo::WordCompleter::WordRange range = solo::WordCompleter::currentWord(
      kb.buf, (size_t)kb.len, (size_t)kb.cursor_pos);
  size_t prefix_len = (size_t)kb.cursor_pos - range.start;
  if (prefix_len == 0) return false;
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN];
  uint8_t count = suggest(kb.buf, range.start, kb.buf + range.start,
                          prefix_len, matches);
  if (count == 0) return false;
  size_t match_len = strlen(matches[0]);
  if (match_len <= prefix_len) return false;
  snprintf(word, word_size, "%s", matches[0]);
  snprintf(suffix, suffix_size, "%s", matches[0] + prefix_len);
  size_t used = 0;
  for (uint8_t i = 0; i < count && used + 1 < candidates_size; i++) {
    int written = snprintf(candidates + used, candidates_size - used,
                           "%s%s", i ? ", " : "", matches[i]);
    if (written < 0) break;
    if ((size_t)written >= candidates_size - used) {
      used = candidates_size - 1;
      break;
    }
    used += (size_t)written;
  }
  return true;
}

inline void enableCompletion(KeyboardWidget& kb, SensorManager* sensors) {
  kb.setPlaceholderRefresh(refreshCompletions, sensors, "Complete:", true);
  kb.setCompletionPreview(previewCompletion, nullptr);
  kb.setPredictiveT9(true);
  kb.setContextPrediction(true);
}
#else
inline void enableCompletion(KeyboardWidget& kb, SensorManager*) { kb.setPredictiveT9(false); }
#endif

inline void begin(KeyboardWidget& kb, const char* initial, int max_len,
                  SensorManager* sensors) {
  kb.begin(initial, max_len);
  kb.setSentenceCase(true);
  kb.setEmojiEnabled(true);
  kbAddSensorPlaceholders(kb, sensors);
  enableCompletion(kb, sensors);
}

inline void begin(KeyboardWidget& kb, const char* initial,
                  SensorManager* sensors) {
  begin(kb, initial, SEND_TEXT_LIMIT, sensors);
}

} // namespace messageeditor
