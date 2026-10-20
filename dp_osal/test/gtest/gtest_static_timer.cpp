/**
 * @file gtest_static_timer.cpp
 * @brief owner派发型静态定时器的期限、回绕、布防/取消、存储独占与回调重布防回归。
 * @details 所有tick由用例直接给定，不启动OS定时器、睡眠线程或设备时钟。
 * 使用局部storage和context，声明顺序保证timer析构时storage仍有效。
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "dp_osal_static_timer.h"

namespace {

/**
 * @brief 到期回调给调用者计数加一。
 * @param context 存活的uint32_t计数对象。
 */
void increment(void* context) {
    ++*static_cast<uint32_t*>(context);
}

/**
 * @brief 回调重布防用的借用timer指针及累计调用次数。
 */
struct RearmContext {
    dp::osal::StaticTimer* timer; ///< 借用同owner的定时器，只在其存活期使用。
    uint32_t calls; ///< 到期回调累计次数，首次重布防后为1。
};

/**
 * @brief 首次回调时将同一个timer重新布防到tick20，第二次只计数。
 * @param context 存活的RearmContext，timer由同一测试owner使用。
 */
void rearm_once(void* context) {
    auto* rearm = static_cast<RearmContext*>(context);
    ++rearm->calls;
    if (rearm->calls == 1U) {
        EXPECT_EQ(rearm->timer->arm_at(20U), dp::osal::StaticTimerStatus::kOk);
    }
}

/**
 * @brief tick99未到、100派发一次、101未布防，确认绝对期限的一次性语义。
 */
TEST(OSALStaticTimer, FiresOnceAtAbsoluteDeadline) {
    dp::osal::StaticTimerStorage storage{};
    dp::osal::StaticTimer timer;
    uint32_t calls = 0U;

    ASSERT_EQ(timer.init(storage, increment, &calls), dp::osal::StaticTimerStatus::kOk);
    ASSERT_EQ(timer.arm_at(100U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(99U), dp::osal::StaticTimerStatus::kNotDue);
    EXPECT_EQ(timer.dispatch_due(100U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(101U), dp::osal::StaticTimerStatus::kNotArmed);
    EXPECT_EQ(calls, 1U);
}

/**
 * @brief 覆盖MAX前后期限、MAX到0以及恰半圈的未到判断，等于期限时仍派发。
 * @details 使用人工tick，不证明实际时钟跨多个完整回绕后仍可无歧义判断。
 */
TEST(OSALStaticTimer, HandlesTickWraparound) {
    dp::osal::StaticTimerStorage storage{};
    dp::osal::StaticTimer timer;
    uint32_t calls = 0U;
    ASSERT_EQ(timer.init(storage, increment, &calls), dp::osal::StaticTimerStatus::kOk);
    ASSERT_EQ(timer.arm_at(1U), dp::osal::StaticTimerStatus::kOk);

    EXPECT_EQ(timer.dispatch_due(std::numeric_limits<uint32_t>::max()),
              dp::osal::StaticTimerStatus::kNotDue);
    EXPECT_EQ(timer.dispatch_due(1U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(calls, 1U);

    ASSERT_EQ(timer.arm_at(std::numeric_limits<uint32_t>::max()),
              dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(std::numeric_limits<uint32_t>::max()),
              dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(calls, 2U);

    ASSERT_EQ(timer.arm_at(std::numeric_limits<uint32_t>::max()),
              dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(0U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(calls, 3U);

    ASSERT_EQ(timer.arm_at(0x80000000U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(0U), dp::osal::StaticTimerStatus::kNotDue);
    EXPECT_EQ(timer.dispatch_due(0x80000000U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(calls, 4U);
}

/**
 * @brief 把期限10覆盖成20，10未到，再cancel后20不触发回调。
 */
TEST(OSALStaticTimer, RearmAndCancelAreDeterministic) {
    dp::osal::StaticTimerStorage storage{};
    dp::osal::StaticTimer timer;
    uint32_t calls = 0U;
    ASSERT_EQ(timer.init(storage, increment, &calls), dp::osal::StaticTimerStatus::kOk);
    ASSERT_EQ(timer.arm_at(10U), dp::osal::StaticTimerStatus::kOk);
    ASSERT_EQ(timer.arm_at(20U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(10U), dp::osal::StaticTimerStatus::kNotDue);
    EXPECT_EQ(timer.cancel(), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(timer.dispatch_due(20U), dp::osal::StaticTimerStatus::kNotArmed);
    EXPECT_EQ(calls, 0U);
}

/**
 * @brief 检查未初始化arm、空回调、同timer重复init及另timer占用同storage均被拒绝。
 */
TEST(OSALStaticTimer, RejectsInvalidLifecycle) {
    dp::osal::StaticTimerStorage storage{};
    dp::osal::StaticTimerStorage other_storage{};
    dp::osal::StaticTimer first;
    dp::osal::StaticTimer second;
    uint32_t calls = 0U;

    EXPECT_EQ(first.arm_at(1U), dp::osal::StaticTimerStatus::kNotInitialized);
    EXPECT_EQ(first.init(storage, nullptr, &calls), dp::osal::StaticTimerStatus::kInvalidArgument);
    ASSERT_EQ(first.init(storage, increment, &calls), dp::osal::StaticTimerStatus::kOk);
    EXPECT_EQ(first.init(other_storage, increment, &calls),
              dp::osal::StaticTimerStatus::kAlreadyInitialized);
    EXPECT_EQ(second.init(storage, increment, &calls), dp::osal::StaticTimerStatus::kAlreadyInitialized);
}

/**
 * @brief tick10回调重布防到20，确认返回后新armed保留，20第二次派发后解除。
 * @details 直接验证先清armed再调回调的顺序，所有操作在单一owner线程同步完成。
 */
TEST(OSALStaticTimer, CallbackCanRearmAfterOneShotIsDisarmed) {
    dp::osal::StaticTimerStorage storage{};
    dp::osal::StaticTimer timer;
    RearmContext context{&timer, 0U};
    ASSERT_EQ(timer.init(storage, rearm_once, &context), dp::osal::StaticTimerStatus::kOk);
    ASSERT_EQ(timer.arm_at(10U), dp::osal::StaticTimerStatus::kOk);

    EXPECT_EQ(timer.dispatch_due(10U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_TRUE(timer.armed());
    EXPECT_EQ(timer.dispatch_due(20U), dp::osal::StaticTimerStatus::kOk);
    EXPECT_FALSE(timer.armed());
    EXPECT_EQ(context.calls, 2U);
}

}  // namespace
