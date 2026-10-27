/**
 * @file dp_osal_thread.h
 * @brief pthread 线程后端：显式启动、协作停止、真实连接回收。
 * @details 生命周期由单一拥有者串行管理。入口结束仍须 join；失败保留句柄以便重试。
 * 对象与借用上下文覆盖整个 join 过程，不提供借用 this 的后台分离模式。
 */
#ifndef DP_OSAL_POSIX_THREAD_H_
#define DP_OSAL_POSIX_THREAD_H_

#include <pthread.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include "interface_thread.h"
#include "dp_osal_port.h"

namespace dp::osal {
/** @brief 线程自持停止状态；TLS 在执行入口期间借用，不独立分配。 */
struct OSALThreadStopCtx {
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};
inline thread_local OSALThreadStopCtx* tl_stop_ctx = nullptr;

/** @brief 停止时提前结束等待；调用者仍须检查停止状态并自行退出，不抛停止异常。 */
inline void osal_sleep_ms_interruptible(uint32_t ms) {
    if (auto* context = tl_stop_ctx) {
        std::unique_lock<std::mutex> lock(context->mtx);
        context->cv.wait_for(lock, std::chrono::milliseconds(ms),
                             [context] { return context->stop.load(std::memory_order_acquire); });
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
#if defined(__linux__) && defined(__GLIBC__) && defined(__USE_GNU)
#if __GLIBC_PREREQ(2, 31)
    static constexpr bool kSupportsTimedJoin = true;
#else
    static constexpr bool kSupportsTimedJoin = false;
#endif
#else
    static constexpr bool kSupportsTimedJoin = false;
#endif
    static constexpr bool kSupportsControlBlockStorage = false;

    Thread() = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    /** @brief 与 std::thread 一样，析构前必须显式完成回收；不隐藏无限等待或分离。 */
    ~Thread() {
        if (doJoinable()) detail::threadLifetimeViolation();
    }

private:
    ThreadResult doStart(const char* name, detail::ThreadEntryStorage task, void* context, int priority, int stack_size,
                         void* stack, const ThreadResources& resources = {}) noexcept {
        (void)name;
        if (doJoinable()) return {ThreadStatus::kBusy};
        if (!task.valid() || stack_size < 0 || (stack != nullptr && stack_size == 0))
            return {ThreadStatus::kInvalidArgument};
        if (resources.control_block != nullptr || resources.control_block_size != 0U ||
            resources.completion_control_block != nullptr || resources.completion_control_block_size != 0U)
            return {ThreadStatus::kNotSupported};
        pthread_attr_t attributes;
        int error = pthread_attr_init(&attributes);
        if (error != 0) return nativeResult(error);
        if (stack != nullptr) {
            if (static_cast<size_t>(stack_size) < static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE))
                error = EINVAL;
            else
                error = pthread_attr_setstack(&attributes, stack, static_cast<size_t>(stack_size));
        } else if (stack_size > 0) {
            const auto requested = static_cast<size_t>(stack_size);
            const auto minimum = static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE);
            error = pthread_attr_setstacksize(&attributes, requested < minimum ? minimum : requested);
        }
        if (error != 0) {
            (void)pthread_attr_destroy(&attributes);
            return {ThreadStatus::kInvalidArgument, error};
        }
        // 默认继承宿主调度策略；请求优先级只作缓存，不宣称获得实时调度权限。
        priority_ = priority;
        task_ = std::move(task);
        task_argument_ = context;
        ready_.store(false, std::memory_order_relaxed);
        running_.store(false, std::memory_order_relaxed);
        stop_context_.stop.store(false, std::memory_order_relaxed);
        suspended_.store(false, std::memory_order_relaxed);
        error = pthread_create(&thread_handle_, &attributes, taskRunner, this);
        (void)pthread_attr_destroy(&attributes);
        if (error != 0) {
            clearEntry();
            return nativeResult(error);
        }
        joinable_.store(true, std::memory_order_release);
        running_.store(true, std::memory_order_release);
        // 此发布之前 runner 不读取入口或句柄，避免 pthread_create 的即时调度竞态。
        ready_.store(true, std::memory_order_release);
        return {ThreadStatus::kSuccess};
    }

    bool doRequestStop() noexcept {
        bool first;
        {
            std::lock_guard<std::mutex> lock(stop_context_.mtx);
            first = !stop_context_.stop.exchange(true, std::memory_order_acq_rel);
        }
        stop_context_.cv.notify_all();
        {
            std::lock_guard<std::mutex> lock(suspend_mutex_);
            suspended_.store(false, std::memory_order_release);
        }
        suspend_cv_.notify_all();
        return first;
    }
    bool doStopRequested() const noexcept { return stop_context_.stop.load(std::memory_order_acquire); }
    bool doJoinable() const noexcept { return joinable_.load(std::memory_order_acquire); }
    ThreadResult doJoin() {
        if (!doJoinable()) return {ThreadStatus::kInvalidState};
        if (pthread_equal(pthread_self(), thread_handle_) != 0) return {ThreadStatus::kWouldDeadlock};
        const int error = pthread_join(thread_handle_, nullptr);
        if (error != 0) return nativeResult(error);
        completeJoin();
        return {ThreadStatus::kSuccess};
    }
    ThreadResult doTryJoinFor(uint32_t timeout_ms) {
        if (!doJoinable()) return {ThreadStatus::kInvalidState};
        if (pthread_equal(pthread_self(), thread_handle_) != 0) return {ThreadStatus::kWouldDeadlock};
#if defined(__linux__) && defined(__GLIBC__) && defined(__USE_GNU)
#if __GLIBC_PREREQ(2, 31)
        timespec deadline{};
        if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return {ThreadStatus::kNativeError, errno};
        const uint64_t nanos =
            static_cast<uint64_t>(deadline.tv_nsec) + static_cast<uint64_t>(timeout_ms % 1000U) * 1000000ULL;
        const uint64_t seconds = timeout_ms / 1000U + nanos / 1000000000ULL;
        if (seconds > static_cast<uint64_t>(std::numeric_limits<time_t>::max() - deadline.tv_sec))
            return {ThreadStatus::kInvalidArgument, EOVERFLOW};
        deadline.tv_sec += static_cast<time_t>(seconds);
        deadline.tv_nsec = static_cast<long>(nanos % 1000000000ULL);
        const int error = pthread_clockjoin_np(thread_handle_, nullptr, CLOCK_MONOTONIC, &deadline);
        if (error != 0) return nativeResult(error);
        completeJoin();
        return {ThreadStatus::kSuccess};
#endif
#endif
        (void)timeout_ms;
        return {ThreadStatus::kNotSupported};
    }
    ThreadResult doDetach() { return {ThreadStatus::kNotSupported}; }

    int doSuspend() {
        std::lock_guard<std::mutex> lock(suspend_mutex_);
        suspended_.store(true, std::memory_order_release);
        return 0;
    }
    int doResume() {
        {
            std::lock_guard<std::mutex> lock(suspend_mutex_);
            suspended_.store(false, std::memory_order_release);
        }
        suspend_cv_.notify_all();
        return 0;
    }
    bool doIsRunning() const noexcept { return running_.load(std::memory_order_acquire); }
    void doSetPriority(int priority) {
        priority_ = priority;
        if (doJoinable()) {
            sched_param parameters{};
            parameters.sched_priority = priority;
            (void)pthread_setschedparam(thread_handle_, SCHED_FIFO, &parameters);
        }
    }
    int doGetPriority() const noexcept { return priority_; }

    static ThreadResult nativeResult(int error) noexcept {
        if (error == ETIMEDOUT) return {ThreadStatus::kTimeout, error};
        if (error == EDEADLK) return {ThreadStatus::kWouldDeadlock, error};
        if (error == EAGAIN || error == ENOMEM) return {ThreadStatus::kResourceExhausted, error};
        return {ThreadStatus::kNativeError, error};
    }
    static void* taskRunner(void* argument) {
        auto* thread = static_cast<Thread*>(argument);
        while (!thread->ready_.load(std::memory_order_acquire)) std::this_thread::yield();
        tl_stop_ctx = &thread->stop_context_;
        {
            std::unique_lock<std::mutex> lock(thread->suspend_mutex_);
            thread->suspend_cv_.wait(lock, [thread] {
                return !thread->suspended_.load(std::memory_order_acquire) || thread->doStopRequested();
            });
        }
        thread->task_.run(thread->task_argument_);
        tl_stop_ctx = nullptr;
        thread->running_.store(false, std::memory_order_release);
        return nullptr;
    }
    void clearEntry() noexcept {
        task_.reset();
        task_argument_ = nullptr;
    }
    void completeJoin() noexcept {
        // 必须在原生 join 成功后清入口、移交资源复用权；失败路径始终保留句柄。
        clearEntry();
        running_.store(false, std::memory_order_release);
        joinable_.store(false, std::memory_order_release);
    }

    pthread_t thread_handle_{};  ///< 仅在 joinable 为真时有效，不假设 pthread_t 可用零值判断。
    detail::ThreadEntryStorage task_;
    void* task_argument_ = nullptr;
    std::atomic<bool> joinable_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> suspended_{false};
    int priority_ = 0;
    std::mutex suspend_mutex_;
    std::condition_variable suspend_cv_;
    OSALThreadStopCtx stop_context_;
};
}  // namespace dp::osal
#endif  // DP_OSAL_POSIX_THREAD_H_
