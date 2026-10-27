/** @file gtest_thread_start.cpp
 * @brief 正式线程的调用者栈与任务生命周期回归。
 */
#include <gtest/gtest.h>
#include <cstdint>
#include "dp_osal_thread.h"
#if defined(DP_OSAL_BACKEND_POSIX)
#if defined(__linux__)
#include <pthread.h>
#endif
namespace formal_thread_stack_test {
struct StackObservation {
    uintptr_t begin;         ///< 调用者栈起始地址，含此地址。
    uintptr_t end;           ///< 调用者栈结束地址，不含此地址。
    uint32_t value;          ///< 任务写入的业务观察值，期望7。
    bool used_caller_stack;  ///< 真实帧地址是否落入[begin,end)，不使用ASAN fake-stack局部地址。
    uintptr_t actual_begin = 0U;
    size_t actual_size = 0U;
    int attributes_status = -1;
};

/**
 * @brief 读取真实帧地址及Linux线程栈属性，写结果7并核对调用者栈区间。
 * @param context 有效StackObservation，跨任务生命周期存活。
 */
void write_context(void* context) {
    auto* observation = static_cast<StackObservation*>(context);
    const uintptr_t frame_address = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#if defined(__linux__)
    pthread_attr_t attributes;
    observation->attributes_status = pthread_getattr_np(pthread_self(), &attributes);
    if (observation->attributes_status == 0) {
        void* actual_stack = nullptr;
        observation->attributes_status = pthread_attr_getstack(&attributes, &actual_stack, &observation->actual_size);
        observation->actual_begin = reinterpret_cast<uintptr_t>(actual_stack);
        (void)pthread_attr_destroy(&attributes);
    }
#endif
    observation->value = 7U;
    observation->used_caller_stack = frame_address >= observation->begin && frame_address < observation->end;
}

TEST(OSALThreadStart, UsesAndReusesCallerStackAfterJoin) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    StackObservation observed{reinterpret_cast<uintptr_t>(stack), reinterpret_cast<uintptr_t>(stack + sizeof(stack)),
                              0U, false};
    dp::osal::Thread thread;
    for (unsigned iteration = 0; iteration < 2U; ++iteration) {
        ASSERT_EQ(thread.start("caller-stack", write_context, &observed, 0, sizeof(stack), stack), 0);
        ASSERT_EQ(thread.join(), 0);
        EXPECT_EQ(observed.value, 7U);
        EXPECT_TRUE(observed.used_caller_stack);
#if defined(__linux__)
        EXPECT_EQ(observed.attributes_status, 0);
        EXPECT_EQ(observed.actual_begin, observed.begin);
        EXPECT_EQ(observed.actual_size, sizeof(stack));
#endif
    }
}
}  // namespace formal_thread_stack_test
#endif
