#include <atomic>

#include "gtest/gtest.h"
#include "dp_osal_system.h"
#if DP_OSAL_ENABLE_TIMER
#include "dp_osal_timer.h"
#endif

using namespace dp::osal;

#if !DP_OSAL_ENABLE_TIMER
/* Entire test file is disabled when DP_OSAL_ENABLE_TIMER=0. */
#else

TEST(OSALTimerTest, TestOSALTimerRepeat) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;
    std::atomic<int> count(0);

    timer.start(100, true, [&]() { count++; });
    EXPECT_LE(count, 5);
    System::getInstance().sleep_ms(700);
    EXPECT_GT(count, 5);
    timer.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALTimerTest, TestOSALTimerStart) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;
    std::atomic<bool> callbackExecuted(false);

    timer.start(100, false, [&]() { callbackExecuted = true; });

    EXPECT_FALSE(callbackExecuted);
    System::getInstance().sleep_ms(200);
    EXPECT_TRUE(callbackExecuted);
    timer.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALTimerTest, TestOSALTimerStop) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;
    std::atomic<bool> callbackExecuted(false);

    timer.start(100, false, [&]() { callbackExecuted = true; });

    System::getInstance().sleep_ms(50);
    timer.stop();
    System::getInstance().sleep_ms(100);
    EXPECT_FALSE(callbackExecuted);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALTimerTest, TestOSALTimerIsRunning) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;

    EXPECT_FALSE(timer.isRunning());

    timer.start(100, false, []() {});
    EXPECT_TRUE(timer.isRunning());

    timer.stop();
    EXPECT_FALSE(timer.isRunning());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALTimerTest, TestOSALTimerGetRemainingTime) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;

    timer.start(200, false, []() {});
    System::getInstance().sleep_ms(100);
    uint32_t remainingTime = timer.getRemainingTime();
    // After sleeping ~100 ms of a 200 ms timer, at most ~150 ms should remain.
    // The upper bound of 150 (not 100) gives headroom for scheduler jitter.
    EXPECT_GT(remainingTime, 0u);
    EXPECT_LE(remainingTime, 150u);

    timer.stop();
    EXPECT_EQ(timer.getRemainingTime(), 0);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALTimerTest, TestOSALTimerReset) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;
    std::atomic<bool> callbackExecuted(false);

    timer.start(200, false, [&]() { callbackExecuted = true; });

    System::getInstance().sleep_ms(100);
    timer.reset();
    System::getInstance().sleep_ms(150);
    EXPECT_FALSE(callbackExecuted);

    System::getInstance().sleep_ms(100);
    EXPECT_TRUE(callbackExecuted);

    timer.stop();
#else
    GTEST_SKIP();
#endif
}

// Test: timer can be restarted after stop()
TEST(OSALTimerTest, TestOSALTimerRestartAfterStop) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;
    std::atomic<int> count(0);

    timer.start(100, false, [&]() { count++; });
    System::getInstance().sleep_ms(200);
    EXPECT_EQ(count.load(), 1);
    timer.stop();

    // Restart — callback should fire again
    timer.start(100, false, [&]() { count++; });
    System::getInstance().sleep_ms(200);
    EXPECT_EQ(count.load(), 2);
    timer.stop();
#else
    GTEST_SKIP();
#endif
}

// Test: stop() on an already-stopped timer must not crash
TEST(OSALTimerTest, TestOSALTimerStopIdempotent) {
#if (DP_OSAL_TEST_TIMER_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Timer timer;

    timer.stop();  // never started — must be safe
    EXPECT_FALSE(timer.isRunning());

    timer.start(200, false, []() {});
    timer.stop();
    timer.stop();  // double stop — must be safe
    EXPECT_FALSE(timer.isRunning());
#else
    GTEST_SKIP();
#endif
}
#endif /* DP_OSAL_ENABLE_TIMER */
