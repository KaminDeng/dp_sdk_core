#include "dp_osal.h"
#if DP_OSAL_ENABLE_LEGACY_COMPAT && defined(DP_OSAL_BACKEND_POSIX)
/**
 * @file gtest_realtime_thread.cpp
 * @brief 实时门面的raw转发、真实线程调用者栈以及单次生命周期回归。
 * @details RawEntryRecordingThread是同步替身，不创建OS线程；另两路径使用真实所选Thread。
 * 外部栈使用64KiB、4096对齐，实际帧与Linux pthread属性只在真实后端用例检查。
 * 这些断言不提供调度期限、全链路无分配或MCU端口栈资格证明。
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#if defined(__linux__)
#include <pthread.h>
#endif

#include "dp_osal_realtime.h"

namespace {

/**
 * @brief 任务写回的栈区间与结果，main在线程join后读取。
 */
struct StackObservation {
    uintptr_t begin; ///< 调用者栈起始地址，含此地址。
    uintptr_t end; ///< 调用者栈结束地址，不含此地址。
    uint32_t value; ///< 任务写入的业务观察值，期望7。
    bool used_caller_stack; ///< 真实帧地址是否落入[begin,end)，不使用ASAN fake-stack局部地址。
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

/**
 * @brief 同步替身保存的raw启动参数与join标记，全部借用，不取得栈/上下文所有权。
 */
struct RawEntryStartObservation {
    dp::osal::RealtimeEntry entry = nullptr; ///< 记录到的raw入口，非拥有。
    void* context = nullptr; ///< 记录的借用上下文。
    void* stack = nullptr; ///< 记录的外部栈地址，替身不实际使用。
    int priority = 0; ///< 原样记录的后端优先级编码。
    int stack_size = 0; ///< 原样记录的栈字节数。
    bool joined = false; ///< 替身join是否被调用。
};

/**
 * @brief 同线程内直接调用entry的记录替身，仅证明门面选用raw接口并正确传参。
 */
class RawEntryRecordingThread {
public:
    /**
     * @brief 保存raw参数后同步entry(context)，返回0，不切换线程或执行栈。
     * @param entry 待记录并调用的raw入口。
     * @param context 借用上下文。
     * @param priority 要记录的优先级值。
     * @param stack_size 要记录的栈大小，字节。
     * @param stack 只记录地址，不用于实际栈切换。
     * @return 固定0模拟后端启动成功；未命名线程名参数被忽略。
     */
    int startRaw(const char*, dp::osal::RealtimeEntry entry, void* context, int priority, int stack_size, void* stack) {
        observation->entry = entry;
        observation->context = context;
        observation->priority = priority;
        observation->stack_size = stack_size;
        observation->stack = stack;
        entry(context);
        return 0;
    }

    /**
     * @brief 只把当前观察对象joined设true，没有真实线程可等待。
     */
    void join() { observation->joined = true; }

    inline static RawEntryStartObservation* observation = nullptr; ///< 当前同步用例借用的观察对象，只在其存活期调用替身。
};

/**
 * @brief 给同步raw转发用例的uint32_t上下文加一。
 * @param context 有效uint32_t指针。
 */
void increment_context(void* context) {
    auto* value = static_cast<uint32_t*>(context);
    ++*value;
}

/**
 * @brief 通过记录替身核入口、上下文、优先级、栈地址/字节数、一次调用及join转发。
 * @details 同步替身不使用所提供的栈，不能据此证明真实任务已在调用者栈运行。
 */
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

/**
 * @brief 使用真实旧Thread接口启动空std::function并join，验证空可调用对象路径不被执行。
 * @details 属于legacy接口兼容回归，不是raw实时门面无分配测试。
 */
TEST(OSALRealtimeThread, LegacyThreadSkipsEmptyCallable) {
    alignas(4096) uint8_t stack[64U * 1024U]{};
    dp::osal::Thread thread;
    std::function<void(void*)> empty;

    ASSERT_EQ(thread.start("empty", empty, nullptr, 0, sizeof(stack), stack), 0);
    thread.join();
}

/**
 * @brief 真实后端入口写7且实际帧地址落入调用者栈，运行中拒绝重启，join后仍拒绝复用门面。
 * @note 需要所选后端支持外部栈和普通join路径，不能推广到持久端口。
 */
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

/**
 * @brief 检查空入口、空栈/零大小和未启动join返回对应错误，不启动有效任务。
 */
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

#endif // 显式兼容的POSIX Host套件
