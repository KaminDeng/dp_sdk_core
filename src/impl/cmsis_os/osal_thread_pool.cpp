//
// Created by kamin.deng on 2024/8/22.
//
#include "osal_thread_pool.h"

#include <algorithm>

namespace osal {

OSALThreadPool::OSALThreadPool()
    : isstarted_(false),
      suspended_(false),
      priority_(0),
      stack_size_(0),
      activeThreads_(0),
      maxThreads_(0),
      minThreads_(0) {}

OSALThreadPool::~OSALThreadPool() { stop(); }

void OSALThreadPool::start(uint32_t numThreads, int priority, int stack_size) {
    stop();  // 停止任何现有的线程池
    isstarted_ = true;
    minThreads_ = numThreads;
    maxThreads_ = numThreads;
    priority_ = priority;
    stack_size_ = stack_size;
    for (uint32_t i = 0; i < numThreads; ++i) {
        OSALThreadPool::OSALAddTread();
    }
    OSAL_LOGD("Thread pool started with %u threads\n", static_cast<unsigned int>(numThreads));
}

bool OSALThreadPool::OSALAddTread() {
    auto thread = std::make_unique<OSALThread>();
    if (thread == nullptr) {
        OSAL_LOGE("Failed to create thread\n");
        return false;
    } else {
        thread->start("ThreadPool", threadEntry, this, priority_, stack_size_);
        threads_.push_back(std::move(thread));
        return true;
    }
}

bool OSALThreadPool::OSALDelTread() {
    auto it = std::find_if(threads_.begin(), threads_.end(),
                           [](const std::unique_ptr<OSALThread> &t) { return !t->isRunning(); });
    if (it != threads_.end()) {
        (*it)->stop();
        threads_.erase(it);
        return true;
    }
    return false;
}

void OSALThreadPool::stop() {
    isstarted_ = false;

    /* Wake threads blocked in condition_.wait() so they can re-check
     * isstarted_ and exit threadLoop() cooperatively.  Threads executing a
     * submitted task receive the stop signal via OSALThread::stop() below,
     * which uses osThreadFlagsSet(0x1) to interrupt sleep_ms/osThreadFlagsWait
     * without pthread_cancel — avoiding the GCC 11 ASan CHECK failure. */
    condition_.notifyAll();

    for (auto &thread : threads_) {
        thread->stop();
    }
    threads_.clear();
    OSAL_LOGD("Thread pool stopped\n");
}

int OSALThreadPool::suspend() {
    suspended_ = true;
    OSAL_LOGD("Thread pool suspended\n");
    return 0;
}

int OSALThreadPool::resume() {
    suspended_ = false;
    condition_.notifyAll();
    OSAL_LOGD("Thread pool resumed\n");
    return 0;
}

bool OSALThreadPool::isStarted() const { return isstarted_; }

bool OSALThreadPool::isSuspended() const { return suspended_; }

void OSALThreadPool::submit(std::function<void(void *)> taskFunction, void *taskArgument, int priority) {
    {
        OSALLockGuard lockGuard(queueMutex_);
        taskQueue_.emplace(Task{taskFunction, taskArgument, priority});
    }
    condition_.notifyOne();
    // 如果当前仍有任务堆积, 且已有线程池跑满，且未达到最大线程值
    if (activeThreads_ == std::size(threads_) && activeThreads_ < maxThreads_) {
        OSALAddTread();
    }
    OSAL_LOGD("Task submitted\n");
}

void OSALThreadPool::setPriority(int priority) {
    priority_ = priority;
    for (auto &thread : threads_) {
        thread->setPriority(priority);
    }
    OSAL_LOGD("Thread pool priority set to %d\n", priority);
}

int OSALThreadPool::getPriority() const { return priority_; }

size_t OSALThreadPool::getTaskQueueSize() {
    OSALLockGuard lockGuard(queueMutex_);
    return taskQueue_.size();
}

uint32_t OSALThreadPool::getActiveThreadCount() const { return activeThreads_; }

bool OSALThreadPool::cancelTask(std::function<void(void *)> &taskFunction) {
    OSALLockGuard lockGuard(queueMutex_);
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
    OSAL_LOGD("Task %s\n", found ? "cancelled" : "not found");
    return found;
}

void OSALThreadPool::setTaskFailureCallback(std::function<void(void *)> callback) {
    taskFailureCallback_ = callback;
    OSAL_LOGD("Task failure callback set\n");
}

void OSALThreadPool::setMaxThreads(uint32_t maxThreads) {
    maxThreads_ = maxThreads;
    OSAL_LOGD("Max threads set to %u\n", static_cast<unsigned int>(maxThreads));
}

uint32_t OSALThreadPool::getMaxThreads() const { return maxThreads_; }

void OSALThreadPool::setMinThreads(uint32_t minThreads) {
    minThreads_ = minThreads;
    OSAL_LOGD("Min threads set to %u\n", static_cast<unsigned int>(minThreads));
}

uint32_t OSALThreadPool::getMinThreads() const { return minThreads_; }

void OSALThreadPool::threadEntry(void *arg) {
    auto *pool = static_cast<OSALThreadPool *>(arg);
    pool->threadLoop();
}

void OSALThreadPool::threadLoop() {
    while (isstarted_) {
        Task task;
        {
            OSALLockGuard lockGuard(queueMutex_);
            // Loop to handle spurious wakeups: condition_.wait has no predicate
            // support in the CMSIS-OS semaphore implementation (C3).
            while (isstarted_ && !suspended_ && taskQueue_.empty()) {
                condition_.wait(queueMutex_);
            }
            if (!isstarted_) break;
            if (suspended_ || taskQueue_.empty()) continue;
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

}  // namespace osal