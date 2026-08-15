#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/GpsMode.h"

TEST(GpsMode, ExposesRequestedModesAndIntervals) {
  EXPECT_EQ(solo::GpsMode::COUNT, 9);
  EXPECT_STREQ(solo::GpsMode::label(0), "Off");
  EXPECT_STREQ(solo::GpsMode::label(1), "Continuous");
  EXPECT_EQ(solo::GpsMode::interval(2), 120U);
  EXPECT_EQ(solo::GpsMode::interval(3), 300U);
  EXPECT_EQ(solo::GpsMode::interval(4), 900U);
  EXPECT_EQ(solo::GpsMode::interval(5), 1800U);
  EXPECT_EQ(solo::GpsMode::interval(6), 3600U);
  EXPECT_EQ(solo::GpsMode::interval(7), 10800U);
  EXPECT_EQ(solo::GpsMode::interval(8), 21600U);
}

TEST(GpsMode, MapsStoredPreferencesBackToMenuModes) {
  EXPECT_EQ(solo::GpsMode::fromPrefs(false, 3600), 0);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 0), 1);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 900), 4);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 1000), 4);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
