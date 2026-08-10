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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
