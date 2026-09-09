#include <chrono>
#include <cstdint>
#include <limits>

#include "gtest/gtest.h"

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal.h"
#include "dp_osal_chrono.h"

using namespace dp::osal;

TEST(OSALChronoDeadlineTest, TickAfterHandlesForwardAndWraparound) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    EXPECT_TRUE(Chrono::tickAfter(5U, 4U));
    EXPECT_FALSE(Chrono::tickAfter(4U, 5U));
    EXPECT_FALSE(Chrono::tickAfter(4U, 4U));
    EXPECT_TRUE(Chrono::tickAfter(0U, std::numeric_limits<uint32_t>::max()));
    EXPECT_FALSE(Chrono::tickAfter(std::numeric_limits<uint32_t>::max(), 0U));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoDeadlineTest, TicksPerSecondReportsMilliseconds) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    EXPECT_EQ(Chrono::getInstance().ticksPerSecond(), 1000U);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoDeadlineTest, SleepUntilWaitsForFutureDeadline) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto &chrono = Chrono::getInstance();
    const Chrono::TimePoint start = chrono.now();
    const Chrono::TimePoint deadline = start + 20U;

    chrono.sleepUntil(deadline);

    const Chrono::TimePoint end = chrono.now();
    EXPECT_FALSE(Chrono::tickAfter(deadline, end));
    EXPECT_GE(chrono.elapsed(start, end), 0.010);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoDeadlineTest, SleepUntilReturnsImmediatelyForPastDeadline) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto &chrono = Chrono::getInstance();
    const Chrono::TimePoint start = chrono.now();
    const Chrono::TimePoint deadline = start - 1U;

    chrono.sleepUntil(deadline);

    const Chrono::TimePoint end = chrono.now();
    EXPECT_LT(chrono.elapsed(start, end), 0.050);
#else
    GTEST_SKIP();
#endif
}

#endif  // DP_OSAL_ENABLE_CHRONO
