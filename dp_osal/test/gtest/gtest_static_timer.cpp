#if DP_OSAL_ENABLE_LEGACY_COMPAT

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "dp_osal_static_timer.h"

namespace {


void increment(void* context) {
    ++*static_cast<uint32_t*>(context);
}


struct RearmContext {
    dp::osal::StaticTimer* timer;
    uint32_t calls;
};


void rearm_once(void* context) {
    auto* rearm = static_cast<RearmContext*>(context);
    ++rearm->calls;
    if (rearm->calls == 1U) {
        EXPECT_EQ(rearm->timer->arm_at(20U), dp::osal::StaticTimerStatus::kOk);
    }
}


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

#endif // 显式兼容的轮询工具套件
