#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/DiagnosticLog.h"

TEST(DiagnosticLog, KeepsNewestSixteenEntries) {
  solo::DiagnosticLog log;
  for (uint32_t i = 0; i < 20; i++) {
    char reason[8];
    snprintf(reason, sizeof(reason), "%lu", (unsigned long)i);
    log.add(i, "Test", reason);
  }
  ASSERT_EQ(log.size(), 16);
  EXPECT_STREQ(log.newest(0)->reason, "19");
  EXPECT_STREQ(log.newest(15)->reason, "4");
}

TEST(DiagnosticLog, CoalescesConsecutiveDuplicates) {
  solo::DiagnosticLog log;
  log.add(1, "Login", "No reply");
  log.add(2, "Login", "No reply");
  ASSERT_EQ(log.size(), 1);
  EXPECT_EQ(log.newest(0)->count, 2);
  EXPECT_EQ(log.newest(0)->timestamp, 2u);
}

TEST(DiagnosticLog, KeepsSeverityDistinct) {
  solo::DiagnosticLog log;
  log.add(1, solo::DiagnosticLog::WARNING, "GPS", "No fix");
  log.add(2, solo::DiagnosticLog::ERROR, "GPS", "No fix");
  ASSERT_EQ(log.size(), 2);
  EXPECT_EQ(log.newest(0)->severity, solo::DiagnosticLog::ERROR);
  EXPECT_EQ(log.newest(1)->severity, solo::DiagnosticLog::WARNING);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
