#include <gtest/gtest.h>
#include <cstring>
#include <helpers/AdvertDataHelpers.h>

#define SOLO_FEAT_CHILD_MODE 1
#include "../../examples/companion_radio/solo/SoloPolicy.h"

// The native test environment does not link src/Identity.cpp; this policy test
// only needs a zeroed contact identity.
namespace mesh {
Identity::Identity() { std::memset(pub_key, 0, sizeof(pub_key)); }
}

TEST(SoloPolicy, ChildConfigurationFailsClosedWithoutParentSession) {
  NodePrefs prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  prefs.child_mode_enabled = 1;

  EXPECT_TRUE(solo::Policy::childLocked(&prefs, false));
  EXPECT_FALSE(solo::Policy::childLocked(&prefs, true));
}

TEST(SoloPolicy, AllowsOnlyFavouriteContactOfExpectedTypeWhileLocked) {
  NodePrefs prefs;
  ContactInfo contact;
  std::memset(&prefs, 0, sizeof(prefs));
  contact.type = ADV_TYPE_CHAT;
  contact.flags = 0;

  EXPECT_FALSE(solo::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_CHAT));
  contact.flags = 0x01;
  EXPECT_TRUE(solo::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_CHAT));
  EXPECT_FALSE(solo::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_ROOM));
}

TEST(SoloPolicy, MatchesStoredAndReportedContactTypes) {
  EXPECT_TRUE(solo::Policy::contactIdentityMatches(ADV_TYPE_CHAT, ADV_TYPE_CHAT,
                                                   ADV_TYPE_CHAT));
  EXPECT_FALSE(solo::Policy::contactIdentityMatches(ADV_TYPE_ROOM, ADV_TYPE_CHAT,
                                                    ADV_TYPE_CHAT));
  EXPECT_FALSE(solo::Policy::contactIdentityMatches(ADV_TYPE_CHAT, ADV_TYPE_ROOM,
                                                    ADV_TYPE_CHAT));
}

TEST(SoloPolicy, BoundsFavouriteChannelMask) {
  NodePrefs prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  prefs.child_channels_enabled = 1;
  prefs.ch_fav_bitmask = (1ULL << 0) | (1ULL << 63);
  uint8_t private_secret[16] = {1};

  EXPECT_TRUE(solo::Policy::channelAllowed(&prefs, true, 0, "Family", private_secret));
  EXPECT_TRUE(solo::Policy::channelAllowed(&prefs, true, 63, "Family", private_secret));
  EXPECT_FALSE(solo::Policy::channelAllowed(&prefs, true, 1, "Family", private_secret));
  EXPECT_FALSE(solo::Policy::channelAllowed(&prefs, true, 64, "Family", private_secret));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
