/**
 * @file gtest_chrono_deadline.cpp
 * @brief tick先后、频率及绝对期限等待的GTest回归。
 * @details CHRONO关闭时整个源不注册，测试开关关闭时相应用例GTEST_SKIP。
 * 本组频率预期固定1000Hz，适用于相应装配，不能推广为所有CMSIS内核都使用毫秒tick。
 * 主机休眠断言依赖实际调度，不构成产品硬实时上界证明。
 */
#include <chrono>
#include <cstdint>
#include <limits>

#include "gtest/gtest.h"

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal.h"
#include "dp_osal_chrono.h"

using namespace dp::osal;

/**
 * @brief 检查普通先后、相等及MAX→0邻近回绕的严格晚于关系。
 * @details 没有证明超过半圈或多个完整回绕时的先后判断。
 */
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

/**
 * @brief 当前测试装配固定要求频率1000Hz，即1tick=1ms。
 * @note 这不是跨后端通用频率契约，非1000Hz内核需另按实际装配解释结果。
 */
TEST(OSALChronoDeadlineTest, TicksPerSecondReportsMilliseconds) {
#if (DP_OSAL_TEST_CHRONO_ENABLED || DP_OSAL_TEST_ALL)
    EXPECT_EQ(Chrono::getInstance().ticksPerSecond(), 1000U);
#else
    GTEST_SKIP();
#endif
}

/**
 * @brief 等待当前tick+20后，确认已到期限且观测秒数至少0.010。
 * @details 不要求精确20ms恢复，也未给未来期限路径设置最大调度延迟断言。
 */
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

/**
 * @brief 对已过去一个tick的期限调用等待，要求本次观测耗时小于50ms。
 * @note 属于当前运行环境的回归阈值，宿主高负载也可能影响观测，不是硬件实时资格证明。
 */
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
