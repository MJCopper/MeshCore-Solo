#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/BootTimeSync.h"

TEST(BootTimeSync, StartsTemporaryGpsAndStopsAfterAnyLiveSync) {
  solo::BootTimeSync sync;
  sync.begin(4, true, false, 1000);
  EXPECT_TRUE(sync.pending());
  EXPECT_TRUE(sync.shouldStartGps());
  EXPECT_EQ(sync.tick(5, false, true, 2000),
            solo::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, DoesNotStopGpsThatWasConfiguredOrManuallyEnabled) {
  solo::BootTimeSync configured;
  configured.begin(1, true, true, 0);
  EXPECT_FALSE(configured.shouldStartGps());
  EXPECT_EQ(configured.tick(2, true, true, 10), solo::BootTimeSync::Action::NONE);

  solo::BootTimeSync manual;
  manual.begin(1, true, false, 0);
  EXPECT_EQ(manual.tick(1, true, true, 10), solo::BootTimeSync::Action::NONE);
  EXPECT_EQ(manual.tick(2, true, true, 20), solo::BootTimeSync::Action::NONE);
}

TEST(BootTimeSync, TimesOutTemporaryGpsButKeepsWaitingForAnotherSource) {
  solo::BootTimeSync sync;
  sync.begin(7, true, false, 100);
  EXPECT_EQ(sync.tick(7, false, true, 100 + solo::BootTimeSync::GPS_TIMEOUT_MS),
            solo::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
  EXPECT_EQ(sync.tick(8, false, false, 100 + solo::BootTimeSync::GPS_TIMEOUT_MS + 1),
            solo::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, RetriesTemporaryGpsHourlyUntilTimeIsSet) {
  solo::BootTimeSync sync;
  sync.begin(4, true, false, 100);

  EXPECT_EQ(sync.tick(4, false, true, 100 + solo::BootTimeSync::GPS_TIMEOUT_MS),
            solo::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_EQ(sync.tick(4, false, false,
                      100 + solo::BootTimeSync::RETRY_INTERVAL_MS - 1),
            solo::BootTimeSync::Action::NONE);
  EXPECT_EQ(sync.tick(4, false, false,
                      100 + solo::BootTimeSync::RETRY_INTERVAL_MS),
            solo::BootTimeSync::Action::START_TEMP_GPS);
  EXPECT_EQ(sync.tick(4, false, true,
                      100 + solo::BootTimeSync::RETRY_INTERVAL_MS +
                      solo::BootTimeSync::GPS_RETRY_TIMEOUT_MS),
            solo::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
}

TEST(BootTimeSync, StopsRetryingAfterTwentyFourHours) {
  solo::BootTimeSync sync;
  sync.begin(4, true, false, 100);
  sync.tick(4, false, true, 100 + solo::BootTimeSync::GPS_TIMEOUT_MS);

  EXPECT_EQ(sync.tick(4, false, false,
                      100 + solo::BootTimeSync::RETRY_WINDOW_MS - 1),
            solo::BootTimeSync::Action::START_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
  EXPECT_EQ(sync.tick(4, false, true,
                      100 + solo::BootTimeSync::RETRY_WINDOW_MS),
            solo::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, HourlyRetryStopsAfterExternalSync) {
  solo::BootTimeSync sync;
  sync.begin(12, true, false, 0);
  sync.tick(12, false, true, solo::BootTimeSync::GPS_TIMEOUT_MS);
  EXPECT_EQ(sync.tick(13, false, false, solo::BootTimeSync::RETRY_INTERVAL_MS),
            solo::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
