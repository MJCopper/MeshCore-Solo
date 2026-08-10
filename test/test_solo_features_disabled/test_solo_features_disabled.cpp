#include <gtest/gtest.h>
#include <cstring>

#define SOLO_FEAT_CHILD_MODE 0
#define SOLO_FEAT_QUIET_TIME 0
#define SOLO_FEAT_CARDKB 0
#define SOLO_FEAT_NAVIGATION 0
#define SOLO_FEAT_REMOTE_BOT 0
#define SOLO_FEAT_REPEATER 0
#define SOLO_FEAT_GPIO 0
#define SOLO_FEAT_AUTOCOMPLETE 0
#include "../../examples/companion_radio/solo/SoloPrefsCodec.h"

TEST(SoloFeaturesDisabled, SidecarDoesNotPersistDisabledFeatureRecords) {
  NodePrefs source;
  std::memset(&source, 0, sizeof(source));
  source.child_mode_enabled = 1;
  source.quiet_time_enabled = 1;

  uint8_t encoded[solo::PrefsCodec::MAX_ENCODED_SIZE] = {};
  size_t size = solo::PrefsCodec::encode(source, encoded, sizeof(encoded));
  EXPECT_EQ(11u, size); // envelope plus checksum, with no feature records

  NodePrefs destination;
  std::memset(&destination, 0, sizeof(destination));
  destination.child_mode_pin_hash = 99;
  EXPECT_TRUE(solo::PrefsCodec::decode(destination, encoded, size));
  EXPECT_EQ(99u, destination.child_mode_pin_hash);
  EXPECT_EQ(0u, destination.child_mode_enabled);
  EXPECT_EQ(0u, destination.quiet_time_enabled);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
