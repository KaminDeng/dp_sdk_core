#include "dp_osal.h"
#if DP_OSAL_ENABLE_LEGACY_COMPAT && defined(DP_OSAL_BACKEND_POSIX)
#include <pthread.h>
#include <gtest/gtest.h>

#include <cstdint>

#include "dp_osal_realtime.h"

namespace {

struct StackObservation {
    uintptr_t begin;
    uintptr_t end;
    uint32_t value;
    bool used_caller_stack;
    uintptr_t actual_begin = 0U;
    size_t actual_size = 0U;
    int attributes_status = -1;
};

void write_context(void* context) {
    auto* observation = static_cast<StackObservation*>(context);
    const uintptr_t frame_address = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#if defined(__linux__)
    pthread_attr_t attributes;
    observation->attributes_status = pthread_getattr_np(pthread_self(), &attributes);
    if (observation->attributes_status == 0) {
        void* actual_stack = nullptr;
        observation->attributes_status = pthread_attr_getstack(&attributes, &actual_stack,
                                                              &observation->actual_size);
        observation->actual_begin = reinterpret_cast<uintptr_t>(actual_stack);
        (void)pthread_attr_destroy(&attributes);
    }
#endif
    observation->value = 7U;
    observation->used_caller_stack =
        frame_address >= observation->begin && frame_address < observation->end;
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
#if defined(__linux__)
    EXPECT_EQ(observed.attributes_status, 0);
    EXPECT_EQ(observed.actual_begin, observed.begin);
    EXPECT_EQ(observed.actual_size, sizeof(stack));
#endif
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

#endif
