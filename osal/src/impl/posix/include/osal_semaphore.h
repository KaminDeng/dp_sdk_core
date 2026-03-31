//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_SEMAPHORE_H__
#define __OSAL_SEMAPHORE_H__

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "interface_semaphore.h"
#include "osal_debug.h"

namespace osal {

class OSALSemaphore : public ISemaphore {
public:
    OSALSemaphore() : count_(0) { OSAL_LOGD("Semaphore initialized\n"); }

    ~OSALSemaphore() { OSAL_LOGD("Semaphore destroyed\n"); }

    void wait() override {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return count_ > 0; });
        --count_;
        OSAL_LOGD("Semaphore wait succeeded\n");
    }

    void signal() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cond_.notify_one();
        OSAL_LOGD("Semaphore signal succeeded\n");
    }

    bool tryWait() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    bool tryWaitFor(uint32_t timestamp) override {
        std::unique_lock<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        if (cond_.wait_until(lock, now + std::chrono::milliseconds(timestamp), [this] { return count_ > 0; })) {
            --count_;
            OSAL_LOGD("Semaphore tryWaitFor succeeded\n");
            return true;
        }
        OSAL_LOGD("Semaphore tryWaitFor failed\n");
        return false;
    }

    int getValue() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        OSAL_LOGD("Semaphore value: %d\n", count_);
        return count_;
    }

    void init(int initialValue) override {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = initialValue;
        OSAL_LOGD("Semaphore initialized with value %d\n", initialValue);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    int count_;
};

}  // namespace osal

#endif  // __OSAL_SEMAPHORE_H__