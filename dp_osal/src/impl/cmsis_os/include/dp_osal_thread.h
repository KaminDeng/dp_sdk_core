/**
 * @file dp_osal_thread.h
 * @brief CMSIS 线程后端：显式协作停止，连接成功后才交还资源。
 * @details 完成通知只代表最后一次对象访问；端口 reap 必须同步确认原生任务结束。
 * 生命周期由单一拥有者串行操作。超时或失败保留回收责任，析构不隐式停止或连接。
 */
#ifndef DP_OSAL_CMSIS_THREAD_H_
#define DP_OSAL_CMSIS_THREAD_H_

#include <atomic>
#include <cstdint>
#include <utility>
#include "interface_thread.h"
#include "dp_osal_port.h"
#include "dp_osal_thread_stop.h"
#if defined(DP_OSAL_PORT_FREERTOS_THREAD_LIFECYCLE)
#include "dp_osal_freertos_thread_port.h"
#endif

namespace dp::osal {
/**
 * @brief 持有CMSIS线程句柄、退出信号量及任务入口的CRTP后端。
 * @note 生命周期字段不构成并发join/stop协议，不允许靠局部句柄副本推断并发删除安全。
 */
class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
    static constexpr bool kSupportsTimedJoin = true;
#else
    static constexpr bool kSupportsTimedJoin = false;
#endif
#ifdef DP_OSAL_PORT_THREAD_STATIC_RESOURCES_UNSUPPORTED
    static constexpr bool kSupportsControlBlockStorage = false;
#else
    static constexpr bool kSupportsControlBlockStorage = true;
#endif
#if defined(DP_OSAL_PORT_FREERTOS_THREAD_LIFECYCLE)
    using ControlBlockStorage = StaticTask_t;
#endif
    Thread() noexcept = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    ~Thread() {
        if (threadHandle != nullptr || exitSemaphore != nullptr) detail::threadLifetimeViolation();
    }

private:
    ThreadResult doStart(const char* name, detail::ThreadEntryStorage task, void* context, int priority, int stack_size,
                         void* stack, const ThreadResources& resources = {}) noexcept {
        if (threadHandle != nullptr || exitSemaphore != nullptr) return {ThreadStatus::kBusy};
        if (!task.valid() || stack_size < 0 ||
            ((resources.control_block == nullptr) != (resources.control_block_size == 0U)) ||
            ((resources.completion_control_block == nullptr) != (resources.completion_control_block_size == 0U)))
            return {ThreadStatus::kInvalidArgument};
#ifdef DP_OSAL_PORT_THREAD_EXTERNAL_STACK_UNSUPPORTED
        if (stack != nullptr) return {ThreadStatus::kNotSupported};
#endif
#ifdef DP_OSAL_PORT_THREAD_STATIC_RESOURCES_UNSUPPORTED
        if (resources.control_block != nullptr || resources.completion_control_block != nullptr)
            return {ThreadStatus::kNotSupported};
#endif
        osThreadAttr_t attr{};
        attr.name = name;
        attr.priority = priority == 0 ? static_cast<osPriority_t>(DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY)
                                      : static_cast<osPriority_t>(priority);
        attr.stack_size = stack_size > 0 ? static_cast<uint32_t>(stack_size) : DP_OSAL_PORT_THREAD_MIN_STACK_SIZE;
        if (attr.stack_size < DP_OSAL_PORT_THREAD_MIN_STACK_SIZE) {
            if (stack != nullptr) return {ThreadStatus::kInvalidArgument};
            attr.stack_size = DP_OSAL_PORT_THREAD_MIN_STACK_SIZE;
        }
        attr.stack_mem = stack;
        attr.cb_mem = resources.control_block;
        attr.cb_size = resources.control_block_size;
#ifdef DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES
        if (!DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES(attr)) return {ThreadStatus::kInvalidArgument};
#endif
#ifdef DP_OSAL_PORT_THREAD_JOINABLE_ATTR
        attr.attr_bits = DP_OSAL_PORT_THREAD_JOINABLE_ATTR;
#else
        attr.attr_bits = osThreadJoinable;
#endif
        osSemaphoreAttr_t completion_attr{};
        completion_attr.cb_mem = resources.completion_control_block;
        completion_attr.cb_size = resources.completion_control_block_size;
        exitSemaphore = osSemaphoreNew(1U, 0U, &completion_attr);
        if (exitSemaphore == nullptr) return {ThreadStatus::kResourceExhausted};
        task_ = std::move(task);
        task_argument_ = context;
        completion_observed_ = false;
        suspended = false;
        stop_requested_.store(false, std::memory_order_release);
        // 创建可能立即运行；全部 runner 借用字段必须先初始化。
        running.store(true, std::memory_order_release);
        threadHandle = osThreadNew(taskRunner, this, &attr);
        if (threadHandle != nullptr) return {ThreadStatus::kSuccess};
        running.store(false, std::memory_order_release);
        clearEntry();
        (void)releaseCompletion();
        return {ThreadStatus::kResourceExhausted};
    }

    bool doRequestStop() noexcept { return !stop_requested_.exchange(true, std::memory_order_acq_rel); }
    bool doStopRequested() const noexcept { return stop_requested_.load(std::memory_order_acquire); }
    bool doJoinable() const noexcept { return threadHandle != nullptr; }
    int doSuspend() {
        if (threadHandle == nullptr || !running.load(std::memory_order_acquire)) return -1;
        const auto status = osThreadSuspend(threadHandle);
        if (status == osOK) suspended = true;
        return status == osOK ? 0 : -1;
    }
    int doResume() {
        if (threadHandle == nullptr || !suspended.load(std::memory_order_acquire)) return -1;
        const auto status = osThreadResume(threadHandle);
        if (status == osOK) suspended = false;
        return status == osOK ? 0 : -1;
    }
    ThreadResult releaseCompletion() noexcept {
        if (exitSemaphore == nullptr) return {ThreadStatus::kSuccess};
        const auto status = osSemaphoreDelete(exitSemaphore);
        if (status != osOK) return {ThreadStatus::kNativeError, static_cast<int>(status)};
        exitSemaphore = nullptr;
        return {ThreadStatus::kSuccess};
    }
    ThreadResult doJoin() { return joinImpl(false, 0U); }
    ThreadResult doTryJoinFor(uint32_t milliseconds) {
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
        return joinImpl(true, milliseconds);
#else
        (void)milliseconds;
        if (osal_cmsis_stop_flag_get() == &stop_requested_) return {ThreadStatus::kWouldDeadlock};
        if (threadHandle == nullptr && exitSemaphore == nullptr) return {ThreadStatus::kInvalidState};
        // 原生线程已回收而完成资源删除失败时，仍允许有限 join 重试清理。
        if (threadHandle == nullptr) return releaseCompletion();
        return {ThreadStatus::kNotSupported};
#endif
    }
    ThreadResult joinImpl(bool bounded, uint32_t milliseconds) {
        if (osal_cmsis_stop_flag_get() == &stop_requested_) return {ThreadStatus::kWouldDeadlock};
        if (threadHandle == nullptr && exitSemaphore == nullptr) return {ThreadStatus::kInvalidState};
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
        const uint32_t started = osKernelGetTickCount();
        const uint64_t wide_ticks = detail::cmsisMillisecondsToTicks(milliseconds, osKernelGetTickFreq());
        if (bounded && wide_ticks >= osWaitForever) return {ThreadStatus::kInvalidArgument};
        const uint32_t budget = static_cast<uint32_t>(wide_ticks);
        const auto remaining = [=]() {
            const uint32_t elapsed = osKernelGetTickCount() - started;
            return elapsed >= budget ? 0U : budget - elapsed;
        };
#else
        (void)milliseconds;
#endif
        if (threadHandle != nullptr) {
            if (exitSemaphore == nullptr) return {ThreadStatus::kInvalidState};
            if (!completion_observed_) {
                uint32_t wait = osWaitForever;
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
                if (bounded) wait = remaining();
#endif
                const auto status = osSemaphoreAcquire(exitSemaphore, wait);
                if (status != osOK)
                    return {(bounded && (status == osErrorTimeout || status == osErrorResource))
                                ? ThreadStatus::kTimeout
                                : ThreadStatus::kNativeError,
                            static_cast<int>(status)};
                // 一次消费完成通知；reap 失败/超时后的重试不得重新 acquire。
                completion_observed_ = true;
            }
            osStatus_t status;
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
            status = bounded ? DP_OSAL_PORT_THREAD_REAP_FOR(threadHandle, remaining())
                             : DP_OSAL_PORT_THREAD_REAP_FOR(threadHandle, osWaitForever);
#elif defined(DP_OSAL_PORT_THREAD_REAP)
            status = DP_OSAL_PORT_THREAD_REAP(threadHandle);
#else
            status = osThreadJoin(threadHandle);
#endif
            if (status != osOK)
                return {(bounded && (status == osErrorTimeout || status == osErrorResource))
                            ? ThreadStatus::kTimeout
                            : ThreadStatus::kNativeError,
                        static_cast<int>(status)};
            threadHandle = nullptr;
        }
        // 原生连接成功后，清理失败也保留同步资源；下一次 join 只重试这里。
        return releaseCompletion();
    }
    ThreadResult doDetach() { return {ThreadStatus::kNotSupported}; }
    bool doIsRunning() const { return running.load(std::memory_order_acquire); }
    void doSetPriority(int priority) {
        if (threadHandle != nullptr) (void)osThreadSetPriority(threadHandle, static_cast<osPriority_t>(priority));
    }
    int doGetPriority() const {
        return threadHandle != nullptr ? static_cast<int>(osThreadGetPriority(threadHandle)) : -1;
    }
    static void taskRunner(void* parameters) {
        auto* thread = static_cast<Thread*>(parameters);
        osal_cmsis_stop_flag_set(&thread->stop_requested_);
        thread->task_.run(thread->task_argument_);
        const osSemaphoreId_t completion = thread->exitSemaphore;
        thread->clearEntry();
        osal_cmsis_stop_flag_set(nullptr);
        thread->running.store(false, std::memory_order_release);  // 最后一次对象访问。
        (void)osSemaphoreRelease(completion);
#ifdef DP_OSAL_PORT_THREAD_FINISH
        DP_OSAL_PORT_THREAD_FINISH();
#else
        osThreadExit();
#endif
    }
    void clearEntry() noexcept {
        task_.reset();
        task_argument_ = nullptr;
    }
    std::atomic<osThreadId_t> threadHandle{nullptr};
    detail::ThreadEntryStorage task_;
    void* task_argument_{};
    std::atomic<bool> running{false};
    std::atomic<bool> suspended{false};
    std::atomic<bool> stop_requested_{false};
    bool completion_observed_ = false;
    osSemaphoreId_t exitSemaphore{};
};
}  // namespace dp::osal
#endif
