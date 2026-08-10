#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>

#include "../../examples/companion_radio/solo/WordCompleter.h"

TEST(SoloWordCompleter, FindsAtMostThreePrefixMatches) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::WordCompleter::suggest("th", 2, matches,
                                                solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_EQ(3u, count);
  EXPECT_STREQ("the", matches[0]);
  EXPECT_STREQ("that", matches[1]);
  EXPECT_STREQ("this", matches[2]);
}

TEST(SoloWordCompleter, MatchesCaseInsensitivelyAndPreservesInitialCapital) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_EQ(1u, solo::WordCompleter::suggest("Plea", 4, matches, 3));
  EXPECT_STREQ("Please", matches[0]);
}

TEST(SoloWordCompleter, DictionaryHasExactlySixHundredUniqueBoundedWords) {
  ASSERT_EQ(600u, solo::WordCompleter::dictionarySize());
  std::set<std::string> unique;
  for (size_t i = 0; i < solo::WordCompleter::dictionarySize(); i++) {
    const char* word = solo::WordCompleter::wordAt(i);
    ASSERT_NE(nullptr, word);
    ASSERT_GT(std::strlen(word), 1u);
    ASSERT_LT(std::strlen(word), solo::WordCompleter::MAX_WORD_LEN);
    for (const char* p = word; *p; p++)
      EXPECT_TRUE((*p >= 'a' && *p <= 'z') || *p == '\'');
    EXPECT_TRUE(unique.insert(word).second) << word;
  }
}

TEST(SoloWordCompleter, FindsWholeWordAroundCursorAtPunctuation) {
  const char* text = "hello, schoo!";
  solo::WordCompleter::WordRange range = solo::WordCompleter::currentWord(
      text, std::strlen(text), 11);
  EXPECT_EQ(7u, range.start);
  EXPECT_EQ(12u, range.end);
}

TEST(SoloWordCompleter, EmptyPrefixDoesNotDumpDictionary) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  EXPECT_EQ(0u, solo::WordCompleter::suggest("", 0, matches, 3));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
