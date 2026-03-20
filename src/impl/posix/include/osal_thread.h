//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_THREAD_H__
#define __OSAL_THREAD_H__

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>

#include "interface_thread.h"
#include "osal_debug.h"

namespace osal {
class OSALThread : public IThread {
public:
    OSALThread() : threadHandle(0), running(false), suspended(false), priority_(0) {
        OSAL_LOGD("OSALThread default constructor called\n");
    }

    OSALThread(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
               int priority = 0, int stack_size = 0, void *pstack = nullptr)
        : threadHandle(0), running(false), suspended(false), priority_(priority) {
        OSAL_LOGD("OSALThread parameterized constructor called\n");
        start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    virtual ~OSALThread() { stop(); }

    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
              int priority = 0, int stack_size = 0, void *pstack = nullptr) override {
        (void)name;
        int result = 0;
        if (!isRunning()) {
            this->taskFunction = taskFunction;
            this->taskArgument = taskArgument;

            pthread_attr_t attr;
            pthread_attr_init(&attr);

            // 设置线程堆栈大小
            if (stack_size > 0) {
                int size = (stack_size > OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE) ? stack_size
                                                                                : OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE;
                pthread_attr_setstacksize(&attr, size);
            }

            // 设置线程堆栈内存
            if (pstack != nullptr) {
                pthread_attr_setstack(&attr, pstack, stack_size);
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

            result = pthread_create(&threadHandle, &attr, taskRunner, this);
            if (result == 0) {
                running = true;
                OSAL_LOGD("Thread started successfully\n");
            } else {
                OSAL_LOGE("Failed to create thread: %s\n", strerror(result));
            }

            pthread_attr_destroy(&attr);
        }
        return result;
    }

    void stop() override {
        if (threadHandle) {
            pthread_t h = threadHandle;
            threadHandle = 0;  // Clear first to prevent double-join on reentrant calls
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.notify_all();  // Wake thread if blocked in waitIfSuspended
            }
            pthread_cancel(h);  // Deferred cancel: fires at next cancellation point
            pthread_join(h, nullptr);
            OSAL_LOGD("OSALThread stopped and joined\n");
        }
    }

    int suspend() override {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = true;
        OSAL_LOGD("Thread suspended\n");
        return 0;  // 成功
    }

    int resume() override {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = false;
        cv_.notify_all();
        OSAL_LOGD("Thread resumed\n");
        return 0;  // 成功
    }

    void join() override {
        if (threadHandle) {
            pthread_join(threadHandle, nullptr);
            OSAL_LOGD("Thread joined\n");
        }
    }

    void detach() override {
        if (threadHandle) {
            pthread_detach(threadHandle);
            OSAL_LOGD("Thread detached\n");
        }
    }

    bool isRunning() const override { return running; }

    void setPriority(int priority) override {
        priority_ = priority;  // always store, so getPriority() works after thread ends
        if (threadHandle) {
            struct sched_param schedParam;
            schedParam.sched_priority = priority;
            // Note: SCHED_FIFO requires root on macOS/Linux. Failure is non-fatal.
            pthread_setschedparam(threadHandle, SCHED_FIFO, &schedParam);
            OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    int getPriority() const override {
        // Return the stored priority so callers get a consistent value
        // even when the thread has already finished (threadHandle == 0)
        // or when the OS call would fail (e.g. no root for SCHED_FIFO).
        OSAL_LOGD("Thread priority retrieved: %d\n", priority_);
        return priority_;
    }

private:
    static void *taskRunner(void *parameters) {
        // Use deferred cancellation (the default) so the thread is only
        // cancelled at explicit cancellation points, not inside malloc/mutex.
        // PTHREAD_CANCEL_ASYNCHRONOUS is unsafe for general use.
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

        OSALThread *thread = static_cast<OSALThread *>(parameters);
        if (thread->taskFunction) {
            thread->waitIfSuspended();
            thread->taskFunction(thread->taskArgument);
        }
        thread->running = false;
        // Clear threadHandle to prevent double-join in stop() / destructor
        thread->threadHandle = 0;
        pthread_exit(nullptr);
        return nullptr;
    }

    void waitIfSuspended() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (suspended) {
            cv_.wait(lock);
        }
    }

    pthread_t threadHandle;
    std::function<void(void *)> taskFunction;
    void *taskArgument;
    std::atomic<bool> running;
    std::atomic<bool> suspended;
    int priority_;  // stored priority, returned by getPriority() regardless of OS call result
    std::mutex mutex_;
    std::condition_variable cv_;
};
}  // namespace osal

#endif  // __OSAL_THREAD_H__