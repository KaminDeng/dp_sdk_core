//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_CONDITION_VARIABLE_H__
#define __OSAL_CONDITION_VARIABLE_H__

#include <atomic>

#include "interface_condition_variable.h"
#include "osal.h"
#include "osal_debug.h"
#include "osal_mutex.h"

namespace osal {

class OSALConditionVariable : public IConditionVariable {
public:
    OSALConditionVariable() {
        cond_ = osSemaphoreNew(16, 0, nullptr);
        if (cond_ == nullptr) {
            OSAL_LOGE("Failed to initialize condition variable\n");
        } else {
            OSAL_LOGD("Condition variable initialized\n");
        }
    }

    ~OSALConditionVariable() override {
        if (osSemaphoreDelete(cond_) != osOK) {
            OSAL_LOGE("Failed to destroy condition variable\n");
        } else {
            OSAL_LOGD("Condition variable destroyed\n");
        }
    }

    void wait(OSALMutex &mutex) override {
        waitCount++;
        mutex.unlock();  // Release the mutex while waiting
        if (osSemaphoreAcquire(cond_, osWaitForever) != osOK) {
            OSAL_LOGE("Failed to wait on condition variable\n");
        } else {
            OSAL_LOGD("Condition variable wait succeeded\n");
        }
        mutex.lock();  // Reacquire the mutex after waiting
        waitCount--;
    }

    bool waitFor(OSALMutex &mutex, uint32_t timeout) override {
        waitCount++;
        mutex.unlock();  // Release the mutex while waiting

        osStatus_t result = osSemaphoreAcquire(cond_, timeout);

        mutex.lock();  // Reacquire the mutex BEFORE decrementing waitCount.
                       // Decrementing before lock() creates a race: notifyAll()
                       // reads waitCount between the semaphore return and the
                       // mutex reacquisition and may under-count waiting threads,
                       // causing it to send too few signals to other waiters.
        waitCount--;

        if (result == osOK) {
            OSAL_LOGD("Condition variable waitFor succeeded\n");
            return true;
        } else if (result == osErrorTimeout) {
            OSAL_LOGD("Condition variable waitFor timed out\n");
            return false;
        } else {
            OSAL_LOGE("Failed to wait on condition variable with timeout\n");
            return false;
        }
    }

    void notifyOne() override {
        if (osSemaphoreRelease(cond_) != osOK) {
            OSAL_LOGE("Failed to notify one on condition variable\n");
        } else {
            OSAL_LOGD("Condition variable notifyOne succeeded\n");
        }
    }

    void notifyAll() override {
        // Snapshot waitCount to avoid TOCTOU: a timed-out waiter may decrement
        // waitCount mid-loop, causing extra semaphore tokens (I8).
        int count = waitCount.load();
        for (int i = 0; i < count; ++i) {
            if (osSemaphoreRelease(cond_) != osOK) {
                OSAL_LOGE("Failed to notify all on condition variable\n");
                break;
            }
        }
        OSAL_LOGD("Condition variable notifyAll succeeded\n");
    }

    int getWaitCount() const override { return waitCount.load(); }

private:
    osSemaphoreId_t cond_;          // Condition variable semaphore
    std::atomic<int> waitCount{0};  // Number of waiting threads
};

}  // namespace osal

#endif  // __OSAL_CONDITION_VARIABLE_H__
