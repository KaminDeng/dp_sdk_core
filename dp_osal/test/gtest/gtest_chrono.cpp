#include "gtest/gtest.h"
#include "dp_osal_system.h"

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal_chrono.h"

using namespace dp::osal;

// Test: now() is monotonically increasing and advances by roughly the sleep duration.
// Lower bound (>=10ms) verifies the clock advances; upper bound (<50ms) gives generous
// headroom for CI/scheduler jitter while still catching a broken clock.
TEST(OSALChronoTest, TestOSALChronoNow) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto timePoint1 = Chrono::getInstance().now();
    // If the system just started, tick count may be near 0; wait for it to stabilise.
    if (timePoint1 < 1000) {
        System::getInstance().sleep_ms(1000);
        timePoint1 = Chrono::getInstance().now();
    }
    System::getInstance().sleep_ms(15);
    auto timePoint2 = Chrono::getInstance().now();
    auto interval = timePoint2 - timePoint1;
    EXPECT_GE(interval, 10u);  // must advance at least 10 ms
    EXPECT_LT(interval, 50u);  // must not advance more than 50 ms (sanity cap)
#else
    GTEST_SKIP();
#endif
}

// Test: elapsed() converts a tick-count difference into seconds accurately.
// 15 ms sleep -> elapsed should be in [0.010, 0.050] seconds.
TEST(OSALChronoTest, TestOSALChronoElapsed) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto timePoint1 = Chrono::getInstance().now();
    System::getInstance().sleep_ms(15);
    auto timePoint2 = Chrono::getInstance().now();
    auto duration = static_cast<float>(Chrono::getInstance().elapsed(timePoint1, timePoint2));
    EXPECT_GE(duration, 0.010f);
    EXPECT_LT(duration, 0.050f);  // generous upper bound for CI headroom
#else
    GTEST_SKIP();
#endif
}

// Test: to_time_t() returns a positive wall-clock time_t value.
TEST(OSALChronoTest, TestOSALChronoToTimeT) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto timePoint = Chrono::getInstance().now();
    std::time_t time = Chrono::getInstance().to_time_t(timePoint);
    EXPECT_GT(time, 0);
#else
    GTEST_SKIP();
#endif
}

// Test: from_time_t() converts a time_t back to a non-zero tick-count TimePoint.
TEST(OSALChronoTest, TestOSALChronoFromTimeT) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    std::time_t time = Chrono::getInstance().to_time_t(Chrono::getInstance().now());
    auto timePoint = Chrono::getInstance().from_time_t(time);
    EXPECT_GT(timePoint, 0u);
#else
    GTEST_SKIP();
#endif
}

// Test: to_string() returns a non-empty string representation of a time point.
TEST(OSALChronoTest, TestOSALChronoToString) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    auto timePoint = Chrono::getInstance().now();
    std::string timeString = Chrono::getInstance().to_string(timePoint);
    EXPECT_FALSE(timeString.empty());
#else
    GTEST_SKIP();
#endif
}

#endif /* DP_OSAL_ENABLE_CHRONO */
