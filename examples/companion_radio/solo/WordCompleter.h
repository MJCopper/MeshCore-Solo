#pragma once

#include <stddef.h>
#include <stdint.h>

namespace solo {

// Small, allocation-free prefix completer for conversational text. The word
// list lives in flash and is deliberately independent of KeyboardWidget so it
// can be reused by another UI without pulling in display/input code.
class WordCompleter {
public:
  enum : uint8_t { MAX_SUGGESTIONS = 3, MAX_WORD_LEN = 16 };

  static size_t dictionarySize() { return WORD_COUNT; }
  static const char* wordAt(size_t index) {
    return index < WORD_COUNT ? words()[index] : nullptr;
  }

  struct WordRange {
    size_t start;
    size_t end;
  };

  static WordRange currentWord(const char* text, size_t len, size_t cursor) {
    if (!text) return { 0, 0 };
    if (cursor > len) cursor = len;
    size_t start = cursor;
    while (start > 0 && isWordChar(text[start - 1])) start--;
    size_t end = cursor;
    while (end < len && isWordChar(text[end])) end++;
    return { start, end };
  }

  // Writes up to max_results NUL-terminated matches and returns their count.
  // Empty prefixes intentionally return no words: the picker can still show
  // message placeholders without dumping the whole dictionary.
  static uint8_t suggest(const char* prefix, size_t prefix_len,
                         char results[][MAX_WORD_LEN], uint8_t max_results) {
    if (!prefix || prefix_len == 0 || max_results == 0) return 0;
    if (max_results > MAX_SUGGESTIONS) max_results = MAX_SUGGESTIONS;
    uint8_t count = 0;
    const char* const* dictionary = words();
    for (size_t i = 0; i < WORD_COUNT && count < max_results; i++) {
      if (!startsWith(dictionary[i], prefix, prefix_len)) continue;
      size_t n = strLength(dictionary[i]);
      if (n >= MAX_WORD_LEN) n = MAX_WORD_LEN - 1;
      for (size_t j = 0; j < n; j++) results[count][j] = dictionary[i][j];
      results[count][n] = '\0';
      if (isUpper(prefix[0])) results[count][0] = toUpper(results[count][0]);
      count++;
    }
    return count;
  }

private:
  static bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
  static char toLower(char c) { return isUpper(c) ? (char)(c + ('a' - 'A')) : c; }
  static char toUpper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - ('a' - 'A')) : c; }
  static bool isWordChar(char c) {
    c = toLower(c);
    return (c >= 'a' && c <= 'z') || c == '\'';
  }
  static size_t strLength(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
  }
  static bool startsWith(const char* word, const char* prefix, size_t prefix_len) {
    for (size_t i = 0; i < prefix_len; i++) {
      if (!word[i] || toLower(word[i]) != toLower(prefix[i])) return false;
    }
    return true;
  }

  static const size_t WORD_COUNT = 600;
  static const char* const* words() {
    // Frequency-ranked conversational English derived from the ISC-licensed
    // SUBTLEX-US spoken corpus. Fragments, proper names, profanity and explicit
    // adult/violent terms are omitted for this child-friendly device.
    static const char* const WORDS[WORD_COUNT] = {
      "you", "the", "to", "it", "that", "and", "of", "what",
      "in", "me", "is", "we", "this", "he", "on", "for",
      "my", "your", "have", "do", "no", "be", "know", "was",
      "not", "can", "are", "all", "with", "just", "get", "here",
      "but", "there", "so", "they", "right", "like", "out", "go",
      "she", "up", "about", "if", "him", "got", "oh", "at",
      "now", "come", "one", "how", "well", "yeah", "her", "want",
      "think", "good", "see", "let", "did", "why", "who", "as",
      "his", "will", "going", "from", "when", "back", "okay", "yes",
      "time", "look", "take", "an", "man", "where", "them", "would",
      "been", "some", "hey", "tell", "or", "us", "had", "were",
      "say", "could", "something", "really", "down", "then", "little", "way",
      "our", "make", "too", "never", "by", "over", "more", "need",
      "mean", "very", "off", "sorry", "give", "has", "thank", "love",
      "said", "am", "people", "please", "sure", "any", "thing", "only",
      "because", "two", "should", "doing", "much", "sir", "maybe", "help",
      "anything", "these", "even", "night", "call", "talk", "nothing", "into",
      "first", "find", "wait", "put", "great", "thought", "day", "work",
      "life", "before", "better", "again", "still", "home", "guy", "those",
      "than", "around", "other", "away", "new", "last", "ever", "stop",
      "keep", "told", "must", "things", "big", "after", "long", "does",
      "always", "their", "everything", "nice", "name", "money", "guys", "feel",
      "believe", "thanks", "old", "place", "fine", "kind", "hello", "lot",
      "years", "made", "leave", "hi", "girl", "hear", "father", "through",
      "every", "bad", "listen", "remember", "three", "boy", "coming", "wrong",
      "might", "stay", "house", "may", "baby", "another", "ok", "dad",
      "wanted", "enough", "talking", "happened", "show", "course", "being", "care",
      "done", "getting", "mind", "left", "ask", "car", "understand", "mother",
      "which", "try", "came", "own", "world", "guess", "next", "else",
      "trying", "someone", "real", "room", "morning", "hold", "woman", "yourself",
      "today", "looking", "mom", "friend", "move", "same", "job", "tonight",
      "went", "son", "best", "saw", "found", "pretty", "ready", "heard",
      "whole", "seen", "together", "minute", "men", "head", "matter", "knew",
      "excuse", "many", "idea", "without", "play", "family", "meet", "most",
      "run", "while", "wife", "once", "live", "somebody", "everybody", "used",
      "use", "myself", "took", "yet", "start", "called", "kid", "tomorrow",
      "happy", "school", "problem", "watch", "bring", "actually", "business", "says",
      "hope", "open", "already", "since", "looks", "sit", "cause", "alone",
      "hard", "wants", "stuff", "turn", "days", "friends", "until", "few",
      "kids", "honey", "gone", "both", "door", "later", "saying", "such",
      "having", "face", "worry", "ago", "five", "second", "brother", "case",
      "thinking", "probably", "beautiful", "hand", "check", "year", "forget", "hit",
      "lost", "minutes", "crazy", "late", "phone", "nobody", "end", "easy",
      "doctor", "shut", "under", "part", "deal", "soon", "four", "anyone",
      "pay", "happen", "true", "each", "supposed", "eat", "mine", "working",
      "town", "afraid", "drink", "exactly", "whatever", "hurt", "knows", "heart",
      "gave", "young", "everyone", "chance", "read", "makes", "number", "taking",
      "change", "anyway", "week", "married", "point", "hands", "police", "word",
      "fun", "wish", "bit", "game", "party", "set", "cut", "comes",
      "sleep", "anybody", "stand", "water", "boys", "trouble", "dear", "couple",
      "gets", "making", "eyes", "break", "story", "far", "times", "close",
      "means", "funny", "goes", "lady", "asked", "walk", "fire", "hours",
      "hate", "rest", "person", "inside", "waiting", "different", "girls", "least",
      "important", "also", "line", "yours", "office", "dinner", "quite", "against",
      "fight", "side", "six", "half", "pick", "question", "ahead", "cool",
      "women", "body", "high", "husband", "reason", "almost", "dog", "buy",
      "truth", "met", "telling", "hot", "anymore", "behind", "started", "speak",
      "bed", "moment", "tried", "shall", "along", "either", "though", "front",
      "sister", "bye", "send", "welcome", "sometimes", "trust", "free", "book",
      "answer", "between", "children", "hurry", "fact", "brought", "clear", "bet",
      "its", "white", "glad", "daughter", "outside", "city", "feeling", "black",
      "seems", "full", "till", "sick", "light", "news", "lose", "wonderful",
      "months", "save", "hour", "country", "needs", "wow", "able", "perfect",
      "running", "child", "order", "living", "sounds", "alive", "food", "gentlemen",
      "luck", "hair", "drive", "promise", "music", "power", "sort", "special",
      "serious", "street", "red", "dance", "hang", "touch", "team", "playing",
      "company", "pull", "plan", "sweet", "ten", "coffee", "lucky", "sound",
      "safe", "date", "leaving", "parents", "himself", "seem", "lives", "air",
      "taken", "picture", "ladies", "sent", "fast", "happens", "perhaps", "catch",
      "ride", "win", "kidding", "top", "scared", "dream", "sign", "meeting",
      "sense", "beat", "control", "drop", "cold", "weeks", "darling", "figure",
      "king", "poor", "throw", "asking", "write", "cannot", "suppose", "small",
      "human", "piece", "boss", "hospital", "past", "calling", "known", "follow",
      "movie", "straight", "christmas", "words", "clean", "kiss", "looked", "feet",
      "evening", "million", "lie", "felt", "moving", "certainly", "step", "learn"
    };
    return WORDS;
  }
};

} // namespace solo
