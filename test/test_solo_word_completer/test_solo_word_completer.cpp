#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>

#include "../../examples/companion_radio/solo/WordCompleter.h"
#include "../../examples/companion_radio/solo/T9Predictor.h"

TEST(SoloWordCompleter, HonoursRequestedResultLimit) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::WordCompleter::suggest("th", 2, matches, 3);
  ASSERT_EQ(3u, count);
  EXPECT_STREQ("the", matches[0]);
  EXPECT_STREQ("that", matches[1]);
  EXPECT_STREQ("this", matches[2]);
}

TEST(SoloWordCompleter, MatchesCaseInsensitivelyAndPreservesInitialCapital) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::WordCompleter::suggest("Plea", 4, matches, 3), 0u);
  EXPECT_STREQ("Please", matches[0]);
}

TEST(SoloWordCompleter, SkipsExactWordAndReturnsLongerCompletions) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::WordCompleter::suggest("he", 2, matches,
                                                solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  for (uint8_t i = 0; i < count; i++) EXPECT_STRNE("he", matches[i]);
}

TEST(SoloWordCompleter, DictionaryHasExactlyTwoThousandFiveHundredUniqueBoundedWords) {
  ASSERT_EQ(2500u, solo::WordCompleter::dictionarySize());
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

TEST(SoloWordCompleter, UsesAustralianSpellingsAndEverydayTerms) {
  std::set<std::string> words;
  for (size_t i = 0; i < solo::WordCompleter::dictionarySize(); i++)
    words.insert(solo::WordCompleter::wordAt(i));

  const char* expected[] = {
    "mum", "colour", "favourite", "centre", "theatre", "neighbourhood",
    "realise", "apologise", "defence", "licence", "arvo", "brekkie",
    "servo", "mozzie", "footy", "bushwalk"
  };
  for (const char* word : expected) EXPECT_EQ(1u, words.count(word)) << word;

  const char* replaced[] = {
    "mom", "color", "favorite", "center", "theater", "neighborhood",
    "realize", "apologize", "defense"
  };
  for (const char* word : replaced) EXPECT_EQ(0u, words.count(word)) << word;
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

TEST(SoloT9Predictor, MapsClassicPhoneKeypadLetters) {
  EXPECT_EQ('2', solo::T9Predictor::digitFor('a'));
  EXPECT_EQ('7', solo::T9Predictor::digitFor('s'));
  EXPECT_EQ('9', solo::T9Predictor::digitFor('z'));
  EXPECT_EQ(0, solo::T9Predictor::digitFor('\''));
}

TEST(SoloT9Predictor, RanksHelloForItsCompleteSequence) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::T9Predictor::suggest("43556", 5, matches,
                                              solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  EXPECT_STREQ("hello", matches[0]);
}

TEST(SoloT9Predictor, ReturnsFrequencyRankedAlternatives) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::T9Predictor::suggest("4663", 4, matches,
                                              solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GE(count, 2u);
  EXPECT_STREQ("good", matches[0]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
