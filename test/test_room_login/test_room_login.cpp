#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/RoomLoginCoordinator.h"
#include "../../examples/companion_radio/solo/RoomLoginResponse.h"

TEST(RoomLogin, RejectsTruncatedAndUnrelatedResponses) {
  uint8_t data[64]{};
  for (uint8_t len = 0; len < 6; len++)
    EXPECT_FALSE(solo::RoomLoginResponse::valid(data, len));
  EXPECT_FALSE(solo::RoomLoginResponse::valid(data, sizeof(data)));
  EXPECT_TRUE(solo::RoomLoginResponse::valid(data, 13));
  data[4] = 'O'; data[5] = 'K';
  EXPECT_TRUE(solo::RoomLoginResponse::valid(data, 6));
  data[4] = 42;
  EXPECT_FALSE(solo::RoomLoginResponse::valid(data, 13));
}

TEST(RoomLogin, AppReservationExpiresAcrossMillisRollover) {
  EXPECT_TRUE(solo::RoomLoginResponse::busy(1, 20, UINT32_MAX - 10));
  EXPECT_TRUE(solo::RoomLoginResponse::busy(1, 20, 19));
  EXPECT_FALSE(solo::RoomLoginResponse::busy(1, 20, 20));
  EXPECT_FALSE(solo::RoomLoginResponse::busy(0, 20, 19));
}

TEST(RoomLogin, KeepsOwnerAndRejectsOtherContacts) {
  solo::RoomLoginCoordinator login;
  uint8_t first[4] = {1}, second[4] = {2};
  solo::RoomLoginCoordinator::Attempt result;
  ASSERT_TRUE(login.begin(login.MESSAGES, first, "secret", true, 100));
  EXPECT_FALSE(login.begin(login.MESSAGES, second, "", false, 100));
  EXPECT_FALSE(login.complete(second, result));
  EXPECT_FALSE(login.takeTimeout(99, result));
  ASSERT_TRUE(login.takeTimeout(100, result));
  EXPECT_STREQ("secret", result.password);
  EXPECT_FALSE(login.complete(first, result));
  ASSERT_TRUE(login.begin(login.MESSAGES, second, "", false, 200));
  EXPECT_FALSE(login.cancel(login.MESSAGES, first));
  EXPECT_TRUE(login.complete(second, result));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
