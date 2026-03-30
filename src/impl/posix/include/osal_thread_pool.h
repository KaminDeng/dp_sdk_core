//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_THREADPOOL_H__
#define __OSAL_THREADPOOL_H__

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

class OSALThreadPool : public IThreadPool {
public:
    OSALThreadPool();

    ~OSALThreadPool();

    void start(uint32_t numThreads, int priority = 0, int stack_size = 0) override;

    void stop() override;

    int suspend() override;

    int resume() override;

    bool isStarted() const override;

    bool isSuspended() const override;

    uint32_t submit(std::function<void(void *)> taskFunction, void *taskArgument, int priority) override;

    void setPriority(int priority) override;

    int getPriority() const override;

    size_t getTaskQueueSize() override;

    uint32_t getActiveThreadCount() const override;

    bool cancelTask(uint32_t taskId) override;

    bool cancelTask(std::function<void(void *)> &taskFunction) override;

    void setTaskFailureCallback(std::function<void(void *)> callback) override;

    void setMaxThreads(uint32_t maxThreads) override;

    uint32_t getMaxThreads() const override;

    void setMinThreads(uint32_t minThreads) override;

    uint32_t getMinThreads() const override;

private:
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

#endif  // __OSAL_THREADPOOL_H__