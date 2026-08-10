#include <gtest/gtest.h>
#include <cstring>

#define SOLO_FEAT_CHILD_MODE 1
#include "../../examples/companion_radio/solo/SoloRuntime.h"

TEST(SoloRuntime, OwnsParentSessionAndLockTransitions) {
  NodePrefs prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  prefs.child_mode_enabled = 1;

  solo::Runtime runtime;
  runtime.begin(&prefs);
  EXPECT_TRUE(runtime.childLocked(&prefs));
  EXPECT_TRUE(runtime.recordChildLockState(&prefs));
  EXPECT_FALSE(runtime.recordChildLockState(&prefs));

  runtime.setParentUnlocked(true);
  EXPECT_FALSE(runtime.childLocked(&prefs));
  EXPECT_FALSE(runtime.recordChildLockState(&prefs));

  runtime.setParentUnlocked(false);
  EXPECT_TRUE(runtime.recordChildLockState(&prefs));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
