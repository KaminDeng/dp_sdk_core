//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_SEMAPHORE_H_
#define DP_OSAL_POSIX_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "interface_semaphore.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    Semaphore() : count_(0) { DP_OSAL_LOGD("Semaphore initialized\n"); }

    ~Semaphore() { DP_OSAL_LOGD("Semaphore destroyed\n"); }

private:
    void doWait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return count_ > 0; });
        --count_;
        DP_OSAL_LOGD("Semaphore wait succeeded\n");
    }

    void doSignal() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cond_.notify_one();
        DP_OSAL_LOGD("Semaphore signal succeeded\n");
    }

    bool doTryWait() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            DP_OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    bool doTryWaitFor(uint32_t timestamp) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        if (cond_.wait_until(lock, now + std::chrono::milliseconds(timestamp), [this] { return count_ > 0; })) {
            --count_;
            DP_OSAL_LOGD("Semaphore tryWaitFor succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWaitFor failed\n");
        return false;
    }

    int doGetValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        DP_OSAL_LOGD("Semaphore value: %d\n", count_);
        return count_;
    }

    void doInit(int initialValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = initialValue;
        DP_OSAL_LOGD("Semaphore initialized with value %d\n", initialValue);
    }
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    int count_;
};

} // namespace dp::osal

#endif  // DP_OSAL_POSIX_SEMAPHORE_H_
