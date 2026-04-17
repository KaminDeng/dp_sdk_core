//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_POSIX_THREAD_H_
#define OSAL_POSIX_THREAD_H_

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "interface_thread.h"
#include "osal.h"  // provides OSAL_PORT_THREAD_MIN_STACK_SIZE
#include "osal_debug.h"

namespace osal {

// Exception thrown by osal_sleep_ms_interruptible() when the owning OSALThread
// is stopped. Caught by taskRunner to cleanly abort the user function without
// using pthread_cancel (which triggers a GCC 11 ASan CHECK failure via
// AsanThread::Destroy → UnsetAlternateSignalStack → sigaltstack interceptor).
struct OSALThreadStopException : public std::exception {
    const char *what() const noexcept override { return "OSALThread: stop requested"; }
};

// Per-thread stop context: owned by OSALThread, pointed to by the thread-local
// tl_stop_ctx while taskRunner is executing.
struct OSALThreadStopCtx {
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};

// Thread-local pointer to the current thread's stop context.
// Set by taskRunner before calling the user function; cleared on exit.
// Used by osal_sleep_ms_interruptible() so OSALSystem::sleep_ms() is interruptible.
inline thread_local OSALThreadStopCtx *tl_stop_ctx = nullptr;

// Interruptible sleep used by OSALSystem::sleep_ms in the POSIX backend.
// - If called from inside an OSALThread and stop() is requested before the
//   timeout expires, throws OSALThreadStopException to abort the user function.
// - Falls back to plain sleep_for when not inside an OSALThread (e.g. main thread).
inline void osal_sleep_ms_interruptible(uint32_t ms) {
    OSALThreadStopCtx *ctx = tl_stop_ctx;
    if (ctx) {
        std::unique_lock<std::mutex> lk(ctx->mtx);
        bool stopped = ctx->cv.wait_for(lk, std::chrono::milliseconds(ms), [ctx] { return ctx->stop.load(); });
        if (stopped) {
            throw OSALThreadStopException{};
        }
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

class OSALThread : public ThreadBase<OSALThread> {
    friend class ThreadBase<OSALThread>;

public:
    OSALThread() : threadHandle_(0), running(false), suspended(false), priority_(0) {
        OSAL_LOGD("OSALThread default constructor called\n");
    }

    OSALThread(const char *name, std::function<void(void *)> fn, void *arg = nullptr, int priority = 0,
               int stack_size = 0, void *pstack = nullptr)
        : threadHandle_(0), running(false), suspended(false), priority_(priority) {
        OSAL_LOGD("OSALThread parameterized constructor called\n");
        start(name, fn, arg, priority, stack_size, pstack);
    }

    ~OSALThread() { doStop(); }

private:
    int doStart(const char *name, std::function<void(void *)> fn, void *arg = nullptr, int priority = 0,
                int stack_size = 0, void *pstack = nullptr) {
        (void)name;
        int result = 0;
        if (!isRunning()) {
            this->taskFunction = fn;
            this->taskArgument = arg;

            // Reset ready_ so the new thread waits until the constructor/start() finishes.
            ready_.store(false, std::memory_order_relaxed);

            // Reset the stop context for a fresh start
            stopCtx_.stop.store(false);

            pthread_attr_t attr;
            pthread_attr_init(&attr);

            // 设置线程堆栈大小
            if (stack_size > 0) {
                size_t sz = static_cast<size_t>(stack_size);
                size_t min_sz = static_cast<size_t>(OSAL_PORT_THREAD_MIN_STACK_SIZE);
                size_t actual = (sz > min_sz) ? sz : min_sz;
                pthread_attr_setstacksize(&attr, actual);
            }

            // 设置线程堆栈内存
            if (pstack != nullptr) {
                pthread_attr_setstack(&attr, pstack, static_cast<size_t>(stack_size));
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
                OSAL_LOGD("Thread started successfully\n");
            } else {
                OSAL_LOGE("Failed to create thread: %s\n", strerror(result));
            }

            pthread_attr_destroy(&attr);

            // Signal the new thread that all members are fully initialized.
            // Must be the last write in start() so the thread sees a consistent object.
            ready_.store(true, std::memory_order_release);
        }
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
            OSAL_LOGD("OSALThread stopped and joined\n");
        }
    }

    int doSuspend() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = true;
        OSAL_LOGD("Thread suspended\n");
        return 0;  // 成功
    }

    int doResume() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = false;
        cv_.notify_all();
        OSAL_LOGD("Thread resumed\n");
        return 0;  // 成功
    }

    void doJoin() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            pthread_join(h, nullptr);
            OSAL_LOGD("Thread joined\n");
        }
    }

    void doDetach() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            pthread_detach(h);
            OSAL_LOGD("Thread detached\n");
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
            OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    int doGetPriority() const {
        // Return the stored priority so callers get a consistent value
        // even when the thread has already finished (threadHandle == 0)
        // or when the OS call would fail (e.g. no root for SCHED_FIFO).
        OSAL_LOGD("Thread priority retrieved: %d\n", priority_);
        return priority_;
    }

    static void *taskRunner(void *parameters) {
        OSALThread *thread = static_cast<OSALThread *>(parameters);

        // Wait until the constructor / start() has finished initializing all
        // members. Without this barrier, TSAN reports data races on taskFunction,
        // mutex_, cv_, stopCtx_, etc. because pthread_create() can schedule the
        // new thread before start() returns.
        while (!thread->ready_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // Install this thread's stop context so osal_sleep_ms_interruptible() works.
        tl_stop_ctx = &thread->stopCtx_;

        if (thread->taskFunction) {
            thread->waitIfSuspended();
            try {
                thread->taskFunction(thread->taskArgument);
            } catch (const OSALThreadStopException &) {
                // stop() was called while the user function was sleeping.
                // Exit cleanly without running any more user code.
                OSAL_LOGD("OSALThread: user function aborted by stop()\n");
            }
        }
        thread->running = false;
        // Clear threadHandle to prevent double-join in stop() / destructor
        thread->threadHandle_.store(0, std::memory_order_release);

        // Clear the thread-local stop context before the thread exits.
        tl_stop_ctx = nullptr;

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

    std::atomic<pthread_t> threadHandle_{0};
    std::function<void(void *)> taskFunction;
    void *taskArgument;
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
}  // namespace osal

#endif  // OSAL_POSIX_THREAD_H_
