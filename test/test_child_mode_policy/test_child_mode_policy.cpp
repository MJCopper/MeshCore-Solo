#include <gtest/gtest.h>

#include "../../examples/companion_radio/ui-new/ChildModePolicy.h"

TEST(ChildModePolicy, RequiresFavouriteFlag) {
  EXPECT_FALSE(childmode::favouriteFlagSet(0));
  EXPECT_TRUE(childmode::favouriteFlagSet(0x01));
  EXPECT_TRUE(childmode::favouriteFlagSet(0x81));
}

TEST(ChildModePolicy, RequiresStoredAndReportedContactTypes) {
  const uint8_t chat = 1;
  const uint8_t room = 2;
  EXPECT_TRUE(childmode::contactIdentityMatches(chat, chat, chat));
  EXPECT_FALSE(childmode::contactIdentityMatches(room, chat, chat));
  EXPECT_FALSE(childmode::contactIdentityMatches(chat, room, chat));
}

TEST(ChildModePolicy, BoundsFavouriteChannelMask) {
  const uint64_t favourites = (1ULL << 0) | (1ULL << 63);
  EXPECT_TRUE(childmode::favouriteChannelSet(favourites, 0));
  EXPECT_TRUE(childmode::favouriteChannelSet(favourites, 63));
  EXPECT_FALSE(childmode::favouriteChannelSet(favourites, 1));
  EXPECT_FALSE(childmode::favouriteChannelSet(favourites, 64));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
