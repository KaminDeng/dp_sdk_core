#include <gtest/gtest.h>

#include <cstdint>
#include <functional>

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

struct RawEntryStartObservation {
    dp::osal::RealtimeEntry entry = nullptr;
    void* context = nullptr;
    void* stack = nullptr;
    int priority = 0;
    int stack_size = 0;
    bool joined = false;
};

class RawEntryRecordingThread {
public:
    int startRaw(const char*, dp::osal::RealtimeEntry entry, void* context, int priority, int stack_size, void* stack) {
        observation->entry = entry;
        observation->context = context;
        observation->priority = priority;
        observation->stack_size = stack_size;
        observation->stack = stack;
        entry(context);
        return 0;
    }

    void join() { observation->joined = true; }

    inline static RawEntryStartObservation* observation = nullptr;
};

void increment_context(void* context) {
    auto* value = static_cast<uint32_t*>(context);
    ++*value;
}

TEST(OSALRealtimeThread, UsesRawThreadEntryOverload) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    RawEntryStartObservation observation;
    uint32_t invocation_count = 0U;
    RawEntryRecordingThread::observation = &observation;
    dp::osal::RealtimeThreadWithBackend<RawEntryRecordingThread> thread;

    ASSERT_EQ(thread.start(increment_context, &invocation_count, 3, stack, sizeof(stack)),
              dp::osal::RealtimeStatus::kOk);
    EXPECT_EQ(observation.entry, increment_context);
    EXPECT_EQ(observation.context, &invocation_count);
    EXPECT_EQ(observation.priority, 3);
    EXPECT_EQ(observation.stack, stack);
    EXPECT_EQ(observation.stack_size, static_cast<int>(sizeof(stack)));
    EXPECT_EQ(invocation_count, 1U);

    EXPECT_EQ(thread.join(), dp::osal::RealtimeStatus::kOk);
    EXPECT_TRUE(observation.joined);
}

TEST(OSALRealtimeThread, LegacyThreadSkipsEmptyCallable) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    dp::osal::Thread thread;
    std::function<void(void*)> empty;

    ASSERT_EQ(thread.start("empty", empty, nullptr, 0, sizeof(stack), stack), 0);
    thread.join();
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
