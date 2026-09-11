#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/SensorAccessCoordinator.h"

TEST(SensorAccess, DefersTelemetryAfterSuccessfulBlankLogin) {
  solo::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::START_BLANK_LOGIN);
  flow.loginStarted(false);
  flow.loginSucceeded(100);
  EXPECT_TRUE(flow.accessConfirmed());
  EXPECT_EQ(flow.tick(1099), solo::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.tick(1100), solo::SensorAccessCoordinator::SEND_TELEMETRY);
}

TEST(SensorAccess, DoesNotRepeatBlankLoginAfterAccessIsConfirmed) {
  solo::SensorAccessCoordinator flow;
  flow.loginStarted(false);
  flow.loginSucceeded(UINT32_MAX - 500);
  EXPECT_EQ(flow.tick(499), solo::SensorAccessCoordinator::SEND_TELEMETRY);
  flow.telemetryStarted();
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), solo::SensorAccessCoordinator::ERROR);
}

TEST(SensorAccess, IgnoresStaleTelemetryFailureDuringLoginAndRetryDelay) {
  solo::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  ASSERT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::START_BLANK_LOGIN);
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), solo::SensorAccessCoordinator::ACL_WAIT);
  flow.loginSucceeded(100);
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), solo::SensorAccessCoordinator::RETRY_DELAY);
}

TEST(SensorAccess, OffersPasswordOnlyAfterBlankLoginFails) {
  solo::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  ASSERT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::START_BLANK_LOGIN);
  flow.loginStarted(false);
  flow.loginFailed(false);
  EXPECT_EQ(flow.phase(), solo::SensorAccessCoordinator::LOGIN_OFFER);
  flow.offerPassword();
  EXPECT_TRUE(flow.passwordEditing());
}

TEST(SensorAccess, KnownPathRetriesThenFallsBackToThreeFloods) {
  solo::SensorAccessCoordinator flow;
  flow.beginTelemetry(true);
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::SEND_TELEMETRY);
  flow.telemetryStarted();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(flow.telemetryTimedOut(),
              solo::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY);
    flow.telemetryStarted();
  }
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::START_BLANK_LOGIN);
}

TEST(SensorAccess, UnknownPathGetsThreeFloodTriesTotal) {
  solo::SensorAccessCoordinator flow;
  flow.beginTelemetry(false);
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(flow.telemetryTimedOut(),
              solo::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY);
    flow.telemetryStarted();
  }
  EXPECT_EQ(flow.telemetryTimedOut(), solo::SensorAccessCoordinator::START_BLANK_LOGIN);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
