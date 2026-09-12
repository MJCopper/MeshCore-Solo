#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>

#include "../../examples/companion_radio/solo/WordCompleter.h"
#include "../../examples/companion_radio/solo/T9Predictor.h"
#include "../../examples/companion_radio/solo/SentenceCase.h"
#include "../../examples/companion_radio/solo/ContextPredictor.h"
#include "../../examples/companion_radio/solo/MessageDraftStore.h"

static_assert(solo::WordCompleter::MAX_SUGGESTIONS == 12,
              "Zen predictive menus expose twelve ranked suggestions");

TEST(SoloMessageDraftStore, KeepsDraftsSeparateByConversation) {
  solo::MessageDraftStore drafts;
  uint8_t alice[] = { 1, 2, 3, 4 };
  uint8_t bob[] = { 5, 6, 7, 8 };
  char text[solo::MessageDraftStore::TEXT_CAPACITY];
  drafts.saveContact(alice, "Hello Alice");
  drafts.saveContact(bob, "Hello Bob");
  drafts.saveChannel(2, "Hello channel");
  ASSERT_TRUE(drafts.loadContact(alice, text, sizeof(text)));
  EXPECT_STREQ("Hello Alice", text);
  ASSERT_TRUE(drafts.loadContact(bob, text, sizeof(text)));
  EXPECT_STREQ("Hello Bob", text);
  ASSERT_TRUE(drafts.loadChannel(2, text, sizeof(text)));
  EXPECT_STREQ("Hello channel", text);
}

TEST(SoloMessageDraftStore, EmptyTextClearsDraftAndReplyStateIsRestored) {
  solo::MessageDraftStore drafts;
  uint8_t alice[] = { 1, 2, 3, 4 };
  char text[solo::MessageDraftStore::TEXT_CAPACITY];
  uint8_t prefix_len = 0;
  drafts.saveContact(alice, "@[Alice] hello", 9);
  ASSERT_TRUE(drafts.loadContact(alice, text, sizeof(text), &prefix_len));
  EXPECT_EQ(9u, prefix_len);
  drafts.saveContact(alice, "");
  EXPECT_FALSE(drafts.loadContact(alice, text, sizeof(text)));
}

TEST(SoloSentenceCase, RecognisesMessageSentenceBoundaries) {
  EXPECT_TRUE(solo::SentenceCase::shouldCapitalize("", 0));
  EXPECT_FALSE(solo::SentenceCase::shouldCapitalize("Hello ", 6));
  EXPECT_TRUE(solo::SentenceCase::shouldCapitalize("Hello. ", 7));
  EXPECT_TRUE(solo::SentenceCase::shouldCapitalize("Really?!  ", 10));
  EXPECT_FALSE(solo::SentenceCase::shouldCapitalize("e.g. text", 9));
}

TEST(SoloSentenceCase, IgnoresReplyPrefixesAndDecoration) {
  const char* reply = "@Marek ";
  EXPECT_TRUE(solo::SentenceCase::shouldCapitalize(reply, strlen(reply)));
  const char* decorated = "\xF0\x9F\x98\x8A \"";
  EXPECT_TRUE(solo::SentenceCase::shouldCapitalize(decorated, strlen(decorated)));
  EXPECT_EQ('H', solo::SentenceCase::apply('h', reply, strlen(reply)));
}

TEST(SoloContextPredictor, PromotesLikelyPrefixCompletions) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "thank y", 6, "y", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(SoloContextPredictor, PromotesLikelyT9Words) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestT9(
                "how ", 4, "273", 3, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("are", matches[0]);
}

TEST(SoloContextPredictor, UsesSentenceRankingAtSentenceAndReplyBoundaries) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "thank. y", 7, "y", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
  memset(matches, 0, sizeof(matches));
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "@Marek y", 7, "y", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(SoloContextPredictor, UsesEightSuccessorsAcrossTwoThousandWords) {
  EXPECT_EQ(2000u, zen_context_data::ENTRY_COUNT);
  EXPECT_EQ(8u, zen_context_data::SUCCESSOR_COUNT);
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "how m", 4, "m", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("much", matches[0]);  // fifth ranked successor of "how"
}

TEST(SoloContextPredictor, TrigramsPrecedeBigramPredictions) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "how are y", 8, "y", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(SoloContextPredictor, UsesContractionsAsContext) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestPrefix(
                "I'm g", 4, "g", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("going", matches[0]);
}

TEST(SoloContextPredictor, RanksSentenceOpenings) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::ContextPredictor::suggestT9(
                "", 0, "4", 1, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("I", matches[0]);
}

TEST(T9Predictor, ChoosesShorterWordWhenContractionCannotFit) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN]{};
  ASSERT_GT(solo::T9Predictor::suggest("46", 2, matches, 8, 2), 0);
  for (const auto& word : matches) EXPECT_LE(strlen(word), 2u);
  EXPECT_EQ(0, solo::T9Predictor::suggest("46", 2, matches, 8, 1));
  EXPECT_GT(solo::T9Predictor::suggest("46", 2, matches, 8), 0);
}

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

TEST(SoloWordCompleter, DictionaryHasExactlyFourThousandUniqueBoundedWords) {
  ASSERT_EQ(4000u, solo::WordCompleter::dictionarySize());
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
    "servo", "mozzie", "footy", "bushwalk", "humour", "judgement",
    "aeroplane", "counsellor", "practising", "maths"
  };
  for (const char* word : expected) EXPECT_EQ(1u, words.count(word)) << word;

  const char* replaced[] = {
    "mom", "color", "favorite", "center", "theater", "neighborhood",
    "realize", "apologize", "defense", "humor", "judgment", "airplane",
    "counselor", "practicing", "math"
  };
  for (const char* word : replaced) EXPECT_EQ(0u, words.count(word)) << word;
}

TEST(SoloWordCompleter, IncludesPinnedLocalCommands) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS]
              [solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::WordCompleter::suggest(
                "hill", 4, matches,
                solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("hillvue", matches[0]);
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

TEST(SoloT9Predictor, ProvidesCommonSingleLetterWords) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::T9Predictor::suggest("2", 1, matches,
                                       solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("a", matches[0]);
  memset(matches, 0, sizeof(matches));
  ASSERT_GT(solo::T9Predictor::suggest("4", 1, matches,
                                       solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("I", matches[0]);
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

TEST(SoloT9Predictor, PrefersExactWordsBeforeLongerCompletions) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::T9Predictor::suggest("43", 2, matches,
                                              solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  EXPECT_EQ(2u, solo::T9Predictor::digitLength(matches[0]));
}

TEST(SoloT9Predictor, MatchesContractionsWithoutAnApostropheKey) {
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = solo::T9Predictor::suggest("3668", 4, matches,
                                              solo::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  bool found = false;
  for (uint8_t i = 0; i < count; i++) found |= std::strcmp(matches[i], "don't") == 0;
  EXPECT_TRUE(found);
}

TEST(SoloT9Predictor, FindsVisiblePrefixBeforeGhostCompletion) {
  EXPECT_EQ(2u, solo::T9Predictor::prefixBytes("hello", 2));
  EXPECT_EQ(3u, solo::T9Predictor::prefixBytes("I'm", 2));
}

TEST(SoloT9Predictor, RecentlyAcceptedWordsLeadTheirSequence) {
  solo::T9Predictor::remember("home");
  char matches[solo::WordCompleter::MAX_SUGGESTIONS][solo::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(solo::T9Predictor::suggest("4663", 4, matches,
                                       solo::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("home", matches[0]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
