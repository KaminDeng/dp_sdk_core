//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_THREADPOOL_H__
#define __OSAL_THREADPOOL_H__

#if OSAL_ENABLE_THREAD_POOL

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

#include "interface_thread_pool.h"
#include "osal_debug.h"
#include "osal_thread.h"

namespace osal {

class OSALThreadPool : public ThreadPoolBase<OSALThreadPool> {
    friend class ThreadPoolBase<OSALThreadPool>;

public:
    OSALThreadPool();

    ~OSALThreadPool();

private:
    void doStart(uint32_t numThreads, int priority = 0, int stack_size = 0);

    void doStop();

    int doSuspend();

    int doResume();

    bool doIsStarted() const;

    bool doIsSuspended() const;

    uint32_t doSubmit(std::function<void(void *)> taskFunction, void *taskArgument, int priority);

    void doSetPriority(int priority);

    int doGetPriority() const;

    size_t doGetTaskQueueSize();

    uint32_t doGetActiveThreadCount() const;

    bool doCancelTask(uint32_t taskId);

    bool doCancelTask(std::function<void(void *)> &taskFunction);

    void doSetTaskFailureCallback(std::function<void(void *)> callback);

    void doSetMaxThreads(uint32_t maxThreads);

    uint32_t doGetMaxThreads() const;

    void doSetMinThreads(uint32_t minThreads);

    uint32_t doGetMinThreads() const;
    struct Task {
        std::function<void(void *)> function;
        void *argument;
        int priority;
        uint32_t id;
    };

    bool OSALAddTread();

    bool OSALDelTread();

    static void threadEntry(void *arg);

    void threadLoop();

    std::vector<std::shared_ptr<OSALThread>> threads_;
    std::queue<Task> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> isstarted_;
    std::atomic<bool> suspended_;
    std::atomic<int> priority_;
    std::atomic<int> stack_size_;
    std::atomic<uint32_t> activeThreads_;
    std::atomic<uint32_t> maxThreads_;
    std::atomic<uint32_t> minThreads_;
    std::atomic<uint32_t> nextTaskId_{1};
    std::function<void(void *)> taskFailureCallback_;
};

}  // namespace osal

#endif /* OSAL_ENABLE_THREAD_POOL */

#endif  // __OSAL_THREADPOOL_H__
