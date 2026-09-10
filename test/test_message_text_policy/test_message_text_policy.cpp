#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/MessageTextPolicy.h"
#include "../../examples/companion_radio/solo/QuickReplies.h"

TEST(MessageTextPolicy, ReservesRetryBytesAndChannelSenderPrefix) {
  EXPECT_EQ(solo::MessageTextPolicy::limit(160), 158u);
  EXPECT_EQ(solo::MessageTextPolicy::limit(160, "Matt"), 154u);
  EXPECT_EQ(solo::MessageTextPolicy::limit(160, "\xF0\x9F\x98\x80"), 154u);
  EXPECT_EQ(solo::MessageTextPolicy::limit(3, "Matt"), 0u);
}

TEST(MessageTextPolicy, ExpandedTextNeverEndsInsideEmoji) {
  char text[] = "Hi \xF0\x9F\x91\x8D!";
  solo::MessageTextPolicy::trim(text, 6);
  EXPECT_STREQ(text, "Hi ");
  char complete[] = "Hi \xF0\x9F\x91\x8D!";
  solo::MessageTextPolicy::trim(complete, 7);
  EXPECT_STREQ(complete, "Hi \xF0\x9F\x91\x8D");
}

TEST(MessageTextPolicy, QuickMessageUsesRetrySafeBudget) {
  char text[161];
  memset(text, 'a', 160);
  text[160] = 0;
  solo::MessageTextPolicy::trim(text, solo::MessageTextPolicy::limit(160));
  EXPECT_EQ(strlen(text), 158u);
}

TEST(QuickReplies, HasStableBalancedBuiltinCatalogue) {
  static const char* const expected[] = {
    "Yes", "No", "Okay", "Thanks", "Sounds good", "Not right now",
    "I can't", "On my way", "Please wait", "Maybe"
  };
  EXPECT_EQ(solo::QuickReplies::BUILTIN_COUNT, 10);
  EXPECT_EQ(solo::QuickReplies::CUSTOM_COUNT, 5);
  EXPECT_EQ(solo::QuickReplies::MAX_VISIBLE_COUNT, 15);
  for (uint8_t i = 0; i < solo::QuickReplies::BUILTIN_COUNT; i++)
    EXPECT_STREQ(solo::QuickReplies::builtin(i), expected[i]);
  EXPECT_STREQ(solo::QuickReplies::builtin(solo::QuickReplies::BUILTIN_COUNT), "");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
