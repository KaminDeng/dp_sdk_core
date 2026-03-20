#include "gtest/gtest.h"
#include "osal_chrono.h"
#include "osal_system.h"

using namespace osal;

TEST(OSALChronoTest, TestOSALChronoNow) {
#if (OSAL_TEST_CHRONO_ENABLED || OSAL_TEST_ALL)
    auto timePoint1 = OSALChrono::getInstance().now();
    // 如果系统启动时时间为0, 可能会测试失败
    if (timePoint1 < 1000) {
        OSALSystem::getInstance().sleep_ms(1000);
        timePoint1 = OSALChrono::getInstance().now();
    }
    OSALSystem::getInstance().sleep_ms(15);
    auto timePoint2 = OSALChrono::getInstance().now();
    auto interval = timePoint2 - timePoint1;
    EXPECT_TRUE(interval >= 10 && interval < 20);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoTest, TestOSALChronoElapsed) {
#if (OSAL_TEST_CHRONO_ENABLED || OSAL_TEST_ALL)
    auto timePoint1 = OSALChrono::getInstance().now();
    OSALSystem::getInstance().sleep_ms(15);
    auto timePoint2 = OSALChrono::getInstance().now();
    auto duration = static_cast<float>(OSALChrono::getInstance().elapsed(timePoint1, timePoint2));
    EXPECT_TRUE(duration >= 0.01f && duration < 0.02f);  // 允许一些误差
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoTest, TestOSALChronoToTimeT) {
#if (OSAL_TEST_CHRONO_ENABLED || OSAL_TEST_ALL)
    auto timePoint = OSALChrono::getInstance().now();
    std::time_t time = OSALChrono::getInstance().to_time_t(timePoint);
    EXPECT_TRUE(time > 0);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoTest, TestOSALChronoFromTimeT) {
#if (OSAL_TEST_CHRONO_ENABLED || OSAL_TEST_ALL)
    std::time_t time = OSALChrono::getInstance().to_time_t(OSALChrono::getInstance().now());
    auto timePoint = OSALChrono::getInstance().from_time_t(time);
    EXPECT_TRUE(timePoint > 0);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALChronoTest, TestOSALChronoToString) {
#if (OSAL_TEST_CHRONO_ENABLED || OSAL_TEST_ALL)
    auto timePoint = OSALChrono::getInstance().now();
    std::string timeString = OSALChrono::getInstance().to_string(timePoint);
    EXPECT_TRUE(!timeString.empty());
#else
    GTEST_SKIP();
#endif
}