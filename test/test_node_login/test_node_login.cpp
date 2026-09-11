#include <gtest/gtest.h>
#include "../../examples/companion_radio/solo/NodeLoginCoordinator.h"
#include "../../examples/companion_radio/solo/NodeLoginResponse.h"

TEST(NodeLogin, RejectsTruncatedAndUnrelatedResponses) {
  uint8_t data[64]{};
  for (uint8_t len = 0; len < 6; len++)
    EXPECT_FALSE(solo::NodeLoginResponse::valid(data, len));
  EXPECT_TRUE(solo::NodeLoginResponse::valid(data, 13));
  EXPECT_TRUE(solo::NodeLoginResponse::valid(data, 14));
  EXPECT_TRUE(solo::NodeLoginResponse::valid(data, sizeof(data)));
  data[4] = 'O'; data[5] = 'K';
  EXPECT_TRUE(solo::NodeLoginResponse::valid(data, 6));
  data[4] = 42;
  EXPECT_FALSE(solo::NodeLoginResponse::valid(data, 13));
}

TEST(NodeLogin, AppReservationExpiresAcrossMillisRollover) {
  EXPECT_TRUE(solo::NodeLoginResponse::busy(1, 20, UINT32_MAX - 10));
  EXPECT_TRUE(solo::NodeLoginResponse::busy(1, 20, 19));
  EXPECT_FALSE(solo::NodeLoginResponse::busy(1, 20, 20));
  EXPECT_FALSE(solo::NodeLoginResponse::busy(0, 20, 19));
}

TEST(NodeLogin, LegacyPasswordSuccessGrantsAdminButBlankAclDoesNot) {
  EXPECT_TRUE(solo::NodeLoginResponse::grantsAdmin(true, 3, false));
  EXPECT_TRUE(solo::NodeLoginResponse::grantsAdmin(true, 0, true));
  EXPECT_FALSE(solo::NodeLoginResponse::grantsAdmin(true, 0, false));
  EXPECT_FALSE(solo::NodeLoginResponse::grantsAdmin(false, 3, true));
}

TEST(NodeLogin, ExplicitAdminRoleOverridesIncompleteAclRoleBits) {
  EXPECT_EQ(solo::NodeLoginResponse::effectivePermissions(1, 0), 3);
  EXPECT_EQ(solo::NodeLoginResponse::effectivePermissions(1, 0xA2), 0xA3);
  EXPECT_EQ(solo::NodeLoginResponse::effectivePermissions(0, 3), 3);
  EXPECT_EQ(solo::NodeLoginResponse::effectivePermissions(2, 2), 2);
}

TEST(NodeLogin, KeepsOwnerAndRejectsOtherContacts) {
  solo::NodeLoginCoordinator login;
  uint8_t first[4] = {1}, second[4] = {2};
  solo::NodeLoginCoordinator::Attempt result;
  ASSERT_TRUE(login.begin(login.MESSAGES, first, "secret", true, true, 100));
  EXPECT_FALSE(login.begin(login.MESSAGES, second, "", false, false, 100));
  EXPECT_FALSE(login.complete(second, result));
  EXPECT_FALSE(login.takeTimeout(99, result));
  ASSERT_TRUE(login.takeTimeout(100, result));
  EXPECT_STREQ("secret", result.password);
  EXPECT_EQ(result.route_retry.next(), solo::NodeRouteRetry::RETRY_PATH);
  EXPECT_EQ(result.route_retry.next(), solo::NodeRouteRetry::RETRY_FLOOD);
  EXPECT_FALSE(login.complete(first, result));
  ASSERT_TRUE(login.begin(login.MESSAGES, second, "", false, false, 200));
  EXPECT_FALSE(login.cancel(login.MESSAGES, first));
  EXPECT_TRUE(login.complete(second, result));
}

TEST(NodeLogin, TracksSensorCarouselAsAnIndependentOwner) {
  solo::NodeLoginCoordinator login;
  uint8_t sensor[4] = {4, 3, 2, 1};
  solo::NodeLoginCoordinator::Attempt result;
  ASSERT_TRUE(login.begin(login.SENSOR, sensor, "", false, false, 50));
  EXPECT_TRUE(login.ownedBy(login.SENSOR));
  ASSERT_TRUE(login.takeTimeout(50, result));
  EXPECT_EQ(result.owner, login.SENSOR);
  EXPECT_STREQ(result.password, "");
  EXPECT_EQ(result.route_retry.next(), solo::NodeRouteRetry::RETRY_FLOOD);
}

TEST(NodeLogin, RestartPreservesTheRemainingRouteBudget) {
  solo::NodeLoginCoordinator login;
  uint8_t key[4] = {9, 8, 7, 6};
  solo::NodeLoginCoordinator::Attempt attempt;
  ASSERT_TRUE(login.begin(login.ADMIN, key, "pw", false, true, 10));
  ASSERT_TRUE(login.takeTimeout(10, attempt));
  EXPECT_EQ(attempt.route_retry.next(), solo::NodeRouteRetry::RETRY_PATH);
  ASSERT_TRUE(login.restart(attempt, 20));
  ASSERT_TRUE(login.takeTimeout(20, attempt));
  EXPECT_EQ(attempt.route_retry.next(), solo::NodeRouteRetry::RETRY_FLOOD);
  EXPECT_STREQ(attempt.password, "pw");
}

TEST(NodeLogin, MissingPathConsumesTheFloodBudgetWithoutAnExtraTry) {
  solo::NodeRouteRetry retry;
  retry.begin(true);
  EXPECT_EQ(retry.next(false), solo::NodeRouteRetry::RETRY_FLOOD);
  EXPECT_EQ(retry.next(false), solo::NodeRouteRetry::RETRY_FLOOD);
  EXPECT_EQ(retry.next(false), solo::NodeRouteRetry::RETRY_FLOOD);
  EXPECT_EQ(retry.next(false), solo::NodeRouteRetry::EXHAUSTED);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
