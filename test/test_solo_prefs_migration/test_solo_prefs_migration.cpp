#include <gtest/gtest.h>

#include "../../examples/companion_radio/SoloPrefsMigration.h"

TEST(SoloPrefsMigration, RecognisesLegacyChildTailExactly) {
  EXPECT_EQ(soloprefs::InitialTail::LEGACY_CHILD,
            soloprefs::classifyInitialTail(soloprefs::LEGACY_CHILD_TAIL_BYTES));
  EXPECT_EQ(soloprefs::InitialTail::CURRENT_OR_UPSTREAM,
            soloprefs::classifyInitialTail(soloprefs::LEGACY_CHILD_TAIL_BYTES - 1));
  EXPECT_EQ(soloprefs::InitialTail::CURRENT_OR_UPSTREAM,
            soloprefs::classifyInitialTail(soloprefs::LEGACY_CHILD_TAIL_BYTES + 1));
}

TEST(SoloPrefsMigration, RecognisesLegacyCombinedTailExactly) {
  EXPECT_EQ(234u, soloprefs::LEGACY_COMBINED_TAIL_BYTES);
  EXPECT_EQ(soloprefs::InitialTail::LEGACY_COMBINED,
            soloprefs::classifyInitialTail(soloprefs::LEGACY_COMBINED_TAIL_BYTES));
}

TEST(SoloPrefsMigration, DistinguishesStandaloneQuietTail) {
  EXPECT_TRUE(soloprefs::isStandaloneQuietTail(soloprefs::QUIET_TAIL_BYTES));
  EXPECT_FALSE(soloprefs::isStandaloneQuietTail(soloprefs::CHILD_TAIL_BYTES));
  EXPECT_FALSE(soloprefs::isStandaloneQuietTail(soloprefs::QUIET_TAIL_BYTES - 1));
}

TEST(SoloPrefsMigration, RejectsTruncatedFeatureTails) {
  EXPECT_FALSE(soloprefs::hasCompleteTail(soloprefs::CHILD_TAIL_BYTES - 1,
                                          soloprefs::CHILD_TAIL_BYTES));
  EXPECT_FALSE(soloprefs::hasCompleteTail(soloprefs::QUIET_TAIL_BYTES - 1,
                                          soloprefs::QUIET_TAIL_BYTES));
  EXPECT_TRUE(soloprefs::hasCompleteTail(soloprefs::CHILD_TAIL_BYTES,
                                         soloprefs::CHILD_TAIL_BYTES));
}
