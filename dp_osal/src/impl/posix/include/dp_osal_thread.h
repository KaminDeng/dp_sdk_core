//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_THREAD_H_
#define DP_OSAL_POSIX_THREAD_H_

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

#include "interface_thread.h"
#include "dp_osal_port.h"  // provides DP_OSAL_PORT_THREAD_MIN_STACK_SIZE
#include "dp_osal_debug.h"

namespace dp::osal {

// Exception thrown by osal_sleep_ms_interruptible() when the owning Thread
// is stopped. Caught by taskRunner to cleanly abort the user function without
// using pthread_cancel (which triggers a GCC 11 ASan CHECK failure via
// AsanThread::Destroy → UnsetAlternateSignalStack → sigaltstack interceptor).
struct OSALThreadStopException : public std::exception {
    const char *what() const noexcept override { return "Thread: stop requested"; }
};

// Per-thread stop context: owned by Thread, pointed to by the thread-local
// tl_stop_ctx while taskRunner is executing.
struct OSALThreadStopCtx {
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};

// Thread-local pointer to the current thread's stop context.
// Set by taskRunner before calling the user function; cleared on exit.
// Used by osal_sleep_ms_interruptible() so System::sleep_ms() is interruptible.
inline thread_local OSALThreadStopCtx *tl_stop_ctx = nullptr;

// Interruptible sleep used by System::sleep_ms in the POSIX backend.
// - If called from inside an Thread and stop() is requested before the
//   timeout expires, throws OSALThreadStopException to abort the user function.
// - Falls back to plain sleep_for when not inside an Thread (e.g. main thread).
inline void osal_sleep_ms_interruptible(uint32_t ms) {
    OSALThreadStopCtx *ctx = tl_stop_ctx;
    if (ctx) {
        std::unique_lock<std::mutex> lk(ctx->mtx);
        bool stopped = ctx->cv.wait_for(lk, std::chrono::milliseconds(ms), [ctx] { return ctx->stop.load(); });
        if (stopped) {
#if defined(__cpp_exceptions)
            throw OSALThreadStopException{};
#else
            // 禁异常raw入口协作返回，调用方循环检查自身停止状态。
            return;
#endif
        }
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
    Thread() : threadHandle_(0), running(false), suspended(false), priority_(0) {
        DP_OSAL_LOGD("Thread default constructor called\n");
    }

    Thread(const char *name, std::function<void(void *)> fn, void *arg = nullptr, int priority = 0,
               int stack_size = 0, void *pstack = nullptr)
        : threadHandle_(0), running(false), suspended(false), priority_(priority) {
        DP_OSAL_LOGD("Thread parameterized constructor called\n");
        start(name, fn, arg, priority, stack_size, pstack);
    }

    ~Thread() { doStop(); }

private:
    int doStart(const char *name, std::function<void(void *)> fn, void *arg = nullptr, int priority = 0,
                int stack_size = 0, void *pstack = nullptr) {
        if (isRunning()) {
            return 0;
        }
        taskFunction.emplace(std::move(fn));
        taskArgument = arg;
        entryKind_ = EntryKind::kLegacy;
        const int result = doStartNative(name, priority, stack_size, pstack);
        if (result != 0) {
            clearEntry();
        }
        return result;
    }

    int doStartRaw(const char *name, ThreadEntry fn, void *arg = nullptr, int priority = 0, int stack_size = 0,
                   void *pstack = nullptr) {
        return doStartRawWithCleanup(name, fn, arg, nullptr, priority, stack_size, pstack);
    }

    int doStartRawOwned(const char* name, ThreadEntry fn, void* arg, ThreadCleanup cleanup,
                        int priority, int stack_size, void* pstack) noexcept {
        if (cleanup == nullptr) return -1;
        return doStartRawWithCleanup(name, fn, arg, cleanup, priority, stack_size, pstack);
    }

    int doStartRawWithCleanup(const char* name, ThreadEntry fn, void* arg, ThreadCleanup cleanup,
                             int priority, int stack_size, void* pstack) noexcept {
        if (isRunning()) {
            return -1;
        }
        if (fn == nullptr) {
            clearEntry();
            return -1;
        }
        taskFunction.reset();
        rawTaskFunction_ = fn;
        rawCleanup_ = cleanup;
        taskArgument = arg;
        entryKind_ = EntryKind::kRaw;
        const int result = doStartNative(name, priority, stack_size, pstack);
        if (result != 0) {
            clearEntry();
        }
        return result;
    }

    int doStartNative(const char *name, int priority, int stack_size, void *pstack) {
        (void)name;
        int result = 0;

        // Reset ready_ so the new thread waits until the constructor/start() finishes.
        ready_.store(false, std::memory_order_relaxed);

        // Reset the stop context for a fresh start
        stopCtx_.stop.store(false);

        pthread_attr_t attr;
        result = pthread_attr_init(&attr);
        if (result != 0) {
            return result;
        }

        const size_t requested = stack_size > 0 ? static_cast<size_t>(stack_size) : 0U;
        const size_t minimum = static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE);
        const size_t actual = requested > minimum ? requested : minimum;
        if (pstack != nullptr) {
            result = pthread_attr_setstack(&attr, pstack, actual);
        } else if (stack_size > 0) {
            result = pthread_attr_setstacksize(&attr, actual);
        }
        if (result != 0) {
            pthread_attr_destroy(&attr);
            return result;
        }

        // 设置线程优先级
        if (priority > 0) {
            sched_param schedParam;
            pthread_attr_getschedparam(&attr, &schedParam);
            schedParam.sched_priority = priority;
            pthread_attr_setschedparam(&attr, &schedParam);
        }

        // 设置线程分离状态
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        pthread_t new_handle = 0;
        result = pthread_create(&new_handle, &attr, taskRunner, this);
        if (result == 0) {
            threadHandle_.store(new_handle, std::memory_order_relaxed);
            running = true;
            DP_OSAL_LOGD("Thread started successfully\n");
        } else {
            DP_OSAL_LOGE("Failed to create thread: %s\n", strerror(result));
        }

        pthread_attr_destroy(&attr);

        // Signal the new thread that all members are fully initialized.
        // Must be the last write in start() so the thread sees a consistent object.
        ready_.store(true, std::memory_order_release);
        return result;
    }

    void doStop() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            // 1. Wake waitIfSuspended
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.notify_all();
            }

            // 2. Signal the stop context: wakes any interruptible sleep inside the user
            //    function and causes osal_sleep_ms_interruptible() to throw, aborting
            //    the user function without pthread_cancel (avoids GCC 11 ASan crash).
            {
                std::unique_lock<std::mutex> lk(stopCtx_.mtx);
                stopCtx_.stop.store(true);
                stopCtx_.cv.notify_all();
            }

            // 3. Wait for the thread to exit naturally — no pthread_cancel needed.
            pthread_join(h, nullptr);
            DP_OSAL_LOGD("Thread stopped and joined\n");
        }
    }

    int doSuspend() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = true;
        DP_OSAL_LOGD("Thread suspended\n");
        return 0;  // 成功
    }

    int doResume() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = false;
        cv_.notify_all();
        DP_OSAL_LOGD("Thread resumed\n");
        return 0;  // 成功
    }

    void doJoin() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            pthread_join(h, nullptr);
            DP_OSAL_LOGD("Thread joined\n");
        }
    }

    void doDetach() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            pthread_detach(h);
            DP_OSAL_LOGD("Thread detached\n");
        }
    }

    bool doIsRunning() const { return running; }

    void doSetPriority(int priority) {
        priority_ = priority;  // always store, so getPriority() works after thread ends
        pthread_t h = threadHandle_.load();
        if (h) {
            struct sched_param schedParam;
            schedParam.sched_priority = priority;
            // Note: SCHED_FIFO requires root on macOS/Linux. Failure is non-fatal.
            pthread_setschedparam(h, SCHED_FIFO, &schedParam);
            DP_OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    int doGetPriority() const {
        // Return the stored priority so callers get a consistent value
        // even when the thread has already finished (threadHandle == 0)
        // or when the OS call would fail (e.g. no root for SCHED_FIFO).
        DP_OSAL_LOGD("Thread priority retrieved: %d\n", priority_);
        return priority_;
    }

    static void *taskRunner(void *parameters) {
        Thread *thread = static_cast<Thread *>(parameters);

        // Wait until the constructor / start() has finished initializing all
        // members. Without this barrier, TSAN reports data races on taskFunction,
        // mutex_, cv_, stopCtx_, etc. because pthread_create() can schedule the
        // new thread before start() returns.
        while (!thread->ready_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // owned context可以在cleanup中销毁this；所需参数先保存在runner自身局部。
        const ThreadCleanup cleanup = thread->rawCleanup_;
        void* const cleanupContext = thread->taskArgument;
        // Install this thread's stop context so osal_sleep_ms_interruptible() works.
        tl_stop_ctx = &thread->stopCtx_;

        if (thread->entryKind_ != EntryKind::kNone) {
            thread->waitIfSuspended();
#if defined(__cpp_exceptions)
            try {
#endif
                if (thread->entryKind_ == EntryKind::kRaw) {
                    ThreadEntry entry = thread->rawTaskFunction_;
                    if (entry != nullptr) {
                        entry(thread->taskArgument);
                    }
                } else if (thread->taskFunction.has_value() && static_cast<bool>(*thread->taskFunction)) {
                    (*thread->taskFunction)(thread->taskArgument);
                }
#if defined(__cpp_exceptions)
            } catch (const OSALThreadStopException &) {
                // stop() was called while the user function was sleeping.
                // Exit cleanly without running any more user code.
                DP_OSAL_LOGD("Thread: user function aborted by stop()\n");
            }
#endif
        }
        thread->clearEntry();
        tl_stop_ctx = nullptr;
        if (cleanup != nullptr) {
            const pthread_t handle = thread->threadHandle_.exchange(0, std::memory_order_acq_rel);
            thread->running.store(false, std::memory_order_release); // 最后一次this访问。
            if (handle != 0) (void)pthread_detach(handle);
            cleanup(cleanupContext);
            return nullptr;
        }
        thread->running = false;

        // Return normally — do NOT call pthread_exit() here.
        // pthread_exit() is noreturn and triggers __asan_handle_no_return →
        // PlatformUnpoisonStacks() → sigaltstack interceptor → GCC 11 ASan CHECK failure.
        return nullptr;
    }

    void waitIfSuspended() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (suspended) {
            cv_.wait(lock);
        }
    }

    enum class EntryKind : uint8_t {
        kNone,
        kLegacy,
        kRaw,
    };

    void clearEntry() {
        taskFunction.reset();
        rawTaskFunction_ = nullptr;
        rawCleanup_ = nullptr;
        taskArgument = nullptr;
        entryKind_ = EntryKind::kNone;
    }

    std::atomic<pthread_t> threadHandle_{0};
    std::optional<std::function<void(void *)>> taskFunction;
    ThreadEntry rawTaskFunction_ = nullptr;
    ThreadCleanup rawCleanup_ = nullptr;
    void *taskArgument = nullptr;
    EntryKind entryKind_ = EntryKind::kNone;
    std::atomic<bool> running;
    std::atomic<bool> suspended;
    // Signals the thread that start() has finished writing all members.
    // Prevents TSAN data races when pthread_create() schedules the thread
    // before the constructor/start() returns.
    std::atomic<bool> ready_{false};
    int priority_;  // stored priority, returned by getPriority() regardless of OS call result
    std::mutex mutex_;
    std::condition_variable cv_;
    OSALThreadStopCtx stopCtx_;  // per-instance stop context for cooperative cancellation
};
} // namespace dp::osal

#endif  // DP_OSAL_POSIX_THREAD_H_
