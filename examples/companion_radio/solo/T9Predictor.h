#pragma once

#include <stddef.h>
#include <stdint.h>
#include "WordCompleter.h"

namespace solo {

// Allocation-free predictive T9 lookup over the shared conversational
// dictionary. Results retain dictionary order, so the most useful candidate is
// always first. A partial digit sequence matches the start of longer words,
// allowing the editor to display a useful provisional word after every key.
class T9Predictor {
public:
  static char digitFor(char letter) {
    if (letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
    if (letter >= 'a' && letter <= 'c') return '2';
    if (letter >= 'd' && letter <= 'f') return '3';
    if (letter >= 'g' && letter <= 'i') return '4';
    if (letter >= 'j' && letter <= 'l') return '5';
    if (letter >= 'm' && letter <= 'o') return '6';
    if (letter >= 'p' && letter <= 's') return '7';
    if (letter >= 't' && letter <= 'v') return '8';
    if (letter >= 'w' && letter <= 'z') return '9';
    return 0;
  }

  static bool matches(const char* word, const char* digits, size_t digit_count) {
    if (!word || !digits || digit_count == 0) return false;
    for (size_t i = 0; i < digit_count; i++) {
      if (!word[i] || digitFor(word[i]) != digits[i]) return false;
    }
    return true;
  }

  static uint8_t suggest(const char* digits, size_t digit_count,
                         char results[][WordCompleter::MAX_WORD_LEN],
                         uint8_t max_results) {
    if (!digits || digit_count == 0 || !results || max_results == 0) return 0;
    if (max_results > WordCompleter::MAX_SUGGESTIONS)
      max_results = WordCompleter::MAX_SUGGESTIONS;
    uint8_t count = 0;
    for (size_t i = 0; i < WordCompleter::dictionarySize() && count < max_results; i++) {
      const char* word = WordCompleter::wordAt(i);
      if (!matches(word, digits, digit_count)) continue;
      size_t n = 0;
      while (word[n] && n + 1 < WordCompleter::MAX_WORD_LEN) {
        results[count][n] = word[n];
        n++;
      }
      results[count][n] = '\0';
      count++;
    }
    return count;
  }
};

} // namespace solo
