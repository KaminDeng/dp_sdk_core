#include <gtest/gtest.h>

#include <cstdint>

#include "dp_osal_realtime.h"

namespace {

struct StackObservation {
    uintptr_t begin;
    uintptr_t end;
    uint32_t value;
    bool used_caller_stack;
};

void write_context(void* context) {
    uint8_t local = 0U;
    auto* observation = static_cast<StackObservation*>(context);
    const uintptr_t local_address = reinterpret_cast<uintptr_t>(&local);
    observation->value = 7U;
    observation->used_caller_stack =
        local_address >= observation->begin && local_address < observation->end;
}

TEST(OSALRealtimeThread, ForwardsCallerContextAndJoins) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    StackObservation observed{reinterpret_cast<uintptr_t>(stack),
                              reinterpret_cast<uintptr_t>(stack + sizeof(stack)), 0U, false};
    dp::osal::RealtimeThread thread;

    ASSERT_EQ(thread.start(write_context, &observed, 0, stack, sizeof(stack)),
              dp::osal::RealtimeStatus::kOk);
    EXPECT_EQ(thread.start(write_context, &observed, 0, stack, sizeof(stack)),
              dp::osal::RealtimeStatus::kAlreadyStarted);
    EXPECT_EQ(thread.join(), dp::osal::RealtimeStatus::kOk);
    EXPECT_EQ(observed.value, 7U);
    EXPECT_TRUE(observed.used_caller_stack);
    EXPECT_EQ(thread.start(write_context, &observed, 0, stack, sizeof(stack)),
              dp::osal::RealtimeStatus::kInvalidState);
}

TEST(OSALRealtimeThread, RejectsMissingEntryOrStack) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    dp::osal::RealtimeThread thread;

    EXPECT_EQ(thread.start(nullptr, nullptr, 0, stack, sizeof(stack)),
              dp::osal::RealtimeStatus::kInvalidArgument);
    EXPECT_EQ(thread.start(write_context, nullptr, 0, nullptr, 0U),
              dp::osal::RealtimeStatus::kInvalidArgument);
    EXPECT_EQ(thread.join(), dp::osal::RealtimeStatus::kInvalidState);
}

}  // namespace
