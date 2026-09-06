#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/BatteryPolicy.h"

TEST(BatteryPolicy, FixedCutoffHonoursExternalPowerAndInvalidReading) {
  EXPECT_TRUE(solo::BatteryPolicy::shouldShutdown(3000, false));
  EXPECT_TRUE(solo::BatteryPolicy::shouldShutdown(2999, false));
  EXPECT_FALSE(solo::BatteryPolicy::shouldShutdown(3001, false));
  EXPECT_FALSE(solo::BatteryPolicy::shouldShutdown(3200, true));
  EXPECT_FALSE(solo::BatteryPolicy::shouldShutdown(0, false));
}

TEST(BatteryPolicy, PercentageEndpointsAndBounds) {
  EXPECT_EQ(solo::BatteryPolicy::percent(3300), 0);
  EXPECT_EQ(solo::BatteryPolicy::percent(4200), 100);
  EXPECT_EQ(solo::BatteryPolicy::percent(0), 0);
  EXPECT_EQ(solo::BatteryPolicy::percent(5000), 100);
  EXPECT_LT(solo::BatteryPolicy::percent(4199), 100);
  EXPECT_LT(solo::BatteryPolicy::percent(4170), 100);
  EXPECT_EQ(solo::BatteryPolicy::percent(3500), 5);
  EXPECT_EQ(solo::BatteryPolicy::percent(3700), 25);
  EXPECT_EQ(solo::BatteryPolicy::percent(3800), 50);
  EXPECT_EQ(solo::BatteryPolicy::percent(4000), 80);
}

TEST(BatteryPolicy, CurveIsMonotonicAndNonlinear) {
  int previous = 0;
  for (int mv = 3000; mv <= 4400; mv++) {
    int percent = solo::BatteryPolicy::percent(mv);
    EXPECT_GE(percent, previous);
    EXPECT_LE(percent, 100);
    previous = percent;
  }
  EXPECT_EQ(solo::BatteryPolicy::percent(3700), 25);
  EXPECT_EQ(solo::BatteryPolicy::percent(3800), 50);
}

TEST(LowBatteryReminder, FirstLowSampleThenHourlyIncludingThresholdChatter) {
  solo::LowBatteryReminder reminder;
  EXPECT_FALSE(reminder.due(0, 3700, false));
  EXPECT_TRUE(reminder.due(8000, 3500, false));
  EXPECT_FALSE(reminder.due(16000, 3700, false));
  EXPECT_FALSE(reminder.due(24000, 3500, false));
  EXPECT_FALSE(reminder.due(3607999, 3500, false));
  EXPECT_TRUE(reminder.due(3608000, 3500, false));
}

TEST(LowBatteryReminder, IgnoresChargingInvalidAndShutdownSamples) {
  solo::LowBatteryReminder reminder;
  EXPECT_FALSE(reminder.due(0, 3500, true));
  EXPECT_FALSE(reminder.due(0, 0, false));
  EXPECT_FALSE(reminder.due(0, 3000, false));
  EXPECT_TRUE(reminder.due(0, 3500, false));
  EXPECT_FALSE(reminder.due(3600000, 3500, true));
  EXPECT_TRUE(reminder.due(3608000, 3500, false));
  EXPECT_FALSE(reminder.due(3609000, 3500, true));
  EXPECT_TRUE(reminder.due(3610000, 3500, false));
}

TEST(LowPowerLatch, LatchesUntilExternalPower) {
  solo::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(3700, false));
  int mv = 3301;
  while (solo::BatteryPolicy::percent(mv) <= 5) mv++;
  EXPECT_FALSE(latch.update(mv - 1, false));
  EXPECT_FALSE(latch.update(mv - 1, false));
  EXPECT_TRUE(latch.update(mv - 1, false));
  EXPECT_TRUE(latch.update(4000, false));
  EXPECT_FALSE(latch.update(4000, true));
  EXPECT_FALSE(latch.update(4000, false));
}

TEST(LowPowerLatch, RequiresThreeConsecutiveLowSamples) {
  solo::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3600, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_TRUE(latch.update(3500, false));
}

TEST(LowPowerLatch, DoesNotReplaceShutdownOrRunWithoutReading) {
  solo::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(0, false));
  EXPECT_FALSE(latch.update(3000, false));
  EXPECT_FALSE(latch.update(2900, false));
  EXPECT_FALSE(latch.update(3500, true));
}

TEST(EmergencyWindow, RunsForTenMinutesAndHandlesRollover) {
  solo::EmergencyWindow window;
  EXPECT_FALSE(window.active());
  window.start(0xFFFFFF00UL);
  EXPECT_TRUE(window.active());
  EXPECT_EQ(window.remainingSeconds(0xFFFFFF00UL), 600u);
  EXPECT_TRUE(window.update((uint32_t)(0xFFFFFF00UL + 599999UL)));
  EXPECT_EQ(window.remainingSeconds((uint32_t)(0xFFFFFF00UL + 599999UL)), 1u);
  EXPECT_FALSE(window.update((uint32_t)(0xFFFFFF00UL + 600000UL)));
  EXPECT_EQ(window.remainingSeconds(0), 0u);
}

TEST(EmergencyWindow, CanBeCancelledWithoutPersistence) {
  solo::EmergencyWindow window;
  window.start(1000);
  window.cancel();
  EXPECT_FALSE(window.active());
  EXPECT_FALSE(window.update(2000));
}

TEST(LowBatteryReminder, HandlesMillisRolloverAndTwentyPercentBoundary) {
  solo::LowBatteryReminder reminder;
  int mv = 3301;
  while (solo::BatteryPolicy::percent(mv + 1) <= 20) mv++;
  EXPECT_FALSE(reminder.due(0xFFFFFF00UL, mv + 1, false));
  EXPECT_TRUE(reminder.due(0xFFFFFF00UL, mv, false));
  EXPECT_FALSE(reminder.due(1000, mv, false));
  EXPECT_TRUE(reminder.due((uint32_t)(0xFFFFFF00UL + 3600000UL), mv, false));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
