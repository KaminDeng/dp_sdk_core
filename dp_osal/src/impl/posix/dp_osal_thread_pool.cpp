//
// Created by kamin.deng on 2024/8/23.
//
#include "dp_osal_thread_pool.h"

#if DP_OSAL_ENABLE_THREAD_POOL

#include <algorithm>
#include <functional>

#include "dp_osal_thread.h"

namespace dp::osal {

ThreadPool::ThreadPool()
    : isstarted_(false), suspended_(false), priority_(0), activeThreads_(0), maxThreads_(0), minThreads_(0) {}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::doStart(uint32_t numThreads, int priority, int stack_size) {
    stop();  // 停止任何现有的线程池
    isstarted_ = true;
    minThreads_ = numThreads;
    maxThreads_ = numThreads;
    priority_ = priority;
    stack_size_ = stack_size;
    for (uint32_t i = 0; i < numThreads; ++i) {
        ThreadPool::OSALAddTread();
    }
    DP_OSAL_LOGD("Thread pool started with %u threads\n", numThreads);
}

bool ThreadPool::OSALAddTread() {
    auto thread = std::make_unique<Thread>();
    if (thread == nullptr) {
        DP_OSAL_LOGE("Failed to create thread\n");
        return false;
    } else {
        thread->start("ThreadPool", threadEntry, this, priority_, stack_size_);
        threads_.push_back(std::move(thread));
        return true;
    }
}

bool ThreadPool::OSALDelTread() {
    auto it = std::find_if(threads_.begin(), threads_.end(),
                           [](const std::shared_ptr<Thread> &t) { return !t->isRunning(); });
    if (it != threads_.end()) {
        (*it)->stop();
        threads_.erase(it);
        return true;
    }
    return false;
}

void ThreadPool::doStop() {
    isstarted_ = false;
    condition_.notify_all();
    for (std::shared_ptr<Thread> thread : threads_) {
        thread->stop();
    }
    threads_.clear();
    DP_OSAL_LOGD("Thread pool stopped\n");
}

int ThreadPool::doSuspend() {
    suspended_ = true;
    DP_OSAL_LOGD("Thread pool suspended\n");
    return 0;
}

int ThreadPool::doResume() {
    suspended_ = false;
    condition_.notify_all();
    DP_OSAL_LOGD("Thread pool resumed\n");
    return 0;
}

bool ThreadPool::doIsStarted() const { return isstarted_; }

bool ThreadPool::doIsSuspended() const { return suspended_; }

uint32_t ThreadPool::doSubmit(std::function<void(void *)> taskFunction, void *taskArgument, int priority) {
    uint32_t id = nextTaskId_.fetch_add(1U, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.emplace(Task{taskFunction, taskArgument, priority, id});
    }
    condition_.notify_one();
    // 如果当前仍有任务堆积, 且已有线程池跑满，且未达到最大线程值
    if (activeThreads_ == std::size(threads_) && activeThreads_ < maxThreads_) {
        OSALAddTread();
    }
    DP_OSAL_LOGD("Task submitted (id=%u)\n", static_cast<unsigned>(id));
    return id;
}

void ThreadPool::doSetPriority(int priority) {
    priority_ = priority;
    for (std::shared_ptr<Thread> thread : threads_) {
        thread->setPriority(priority);
        //            pthread_join(thread, nullptr);
    }
    DP_OSAL_LOGD("Thread pool priority set to %d\n", priority);
}

int ThreadPool::doGetPriority() const { return priority_; }

size_t ThreadPool::doGetTaskQueueSize() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

uint32_t ThreadPool::doGetActiveThreadCount() const { return activeThreads_; }

bool ThreadPool::doCancelTask(uint32_t taskId) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<Task> newQueue;
    bool found = false;
    while (!taskQueue_.empty()) {
        Task t = taskQueue_.front();
        taskQueue_.pop();
        if (t.id == taskId) {
            found = true;
        } else {
            newQueue.push(t);
        }
    }
    taskQueue_ = std::move(newQueue);
    DP_OSAL_LOGD("cancelTask(id=%u): %s\n", static_cast<unsigned>(taskId), found ? "cancelled" : "not found");
    return found;
}

bool ThreadPool::doCancelTask(std::function<void(void *)> &taskFunction) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<Task> newQueue;
    bool found = false;
    auto targetPtr = taskFunction.template target<void (*)(void *)>();
    while (!taskQueue_.empty()) {
        Task task = taskQueue_.front();
        taskQueue_.pop();
        auto taskPtr = task.function.template target<void (*)(void *)>();
        // (!taskPtr && !targetPtr) 判断是为了处理两个空的 std::function 对象相等的情况。
        if ((taskPtr && targetPtr && *taskPtr == *targetPtr) || (!taskPtr && !targetPtr)) {
            found = true;
        } else {
            newQueue.push(task);
        }
    }
    taskQueue_ = std::move(newQueue);
    DP_OSAL_LOGD("Task %s\n", found ? "cancelled" : "not found");
    return found;
}

void ThreadPool::doSetTaskFailureCallback(std::function<void(void *)> callback) {
    taskFailureCallback_ = callback;
    DP_OSAL_LOGD("Task failure callback set\n");
}

void ThreadPool::doSetMaxThreads(uint32_t maxThreads) {
    maxThreads_ = maxThreads;
    DP_OSAL_LOGD("Max threads set to %u\n", maxThreads);
}

uint32_t ThreadPool::doGetMaxThreads() const { return maxThreads_; }

void ThreadPool::doSetMinThreads(uint32_t minThreads) {
    minThreads_ = minThreads;
    DP_OSAL_LOGD("Min threads set to %u\n", minThreads);
}

uint32_t ThreadPool::doGetMinThreads() const { return minThreads_; }

void ThreadPool::threadEntry(void *arg) {
    ThreadPool *pool = static_cast<ThreadPool *>(arg);
    pool->threadLoop();
}

void ThreadPool::threadLoop() {
    while (isstarted_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] { return !taskQueue_.empty() || !isstarted_; });
            if (!isstarted_) break;
            if (suspended_) continue;
            task = taskQueue_.front();
            taskQueue_.pop();
        }
        if (task.function != nullptr) {
            ++activeThreads_;
            task.function(task.argument);
            --activeThreads_;
        } else {
            if (taskFailureCallback_ != nullptr) {
                taskFailureCallback_(task.argument);
            }
        }
    }
}

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_THREAD_POOL */
