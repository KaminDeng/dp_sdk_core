//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_CONDITION_VARIABLE_H_
#define DP_OSAL_CMSIS_CONDITION_VARIABLE_H_

#if DP_OSAL_ENABLE_CONDITION_VAR

#include <atomic>

#include "interface_condition_variable.h"
#include "osal.h"
#include "dp_osal_debug.h"
#include "dp_osal_mutex.h"

namespace dp::osal {

class ConditionVariable : public ConditionVariableBase<ConditionVariable> {
    friend class ConditionVariableBase<ConditionVariable>;

public:
    ConditionVariable() {
        cond_ = osSemaphoreNew(16, 0, nullptr);
        if (cond_ == nullptr) {
            DP_OSAL_LOGE("Failed to initialize condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable initialized\n");
        }
    }

    ~ConditionVariable() {
        if (osSemaphoreDelete(cond_) != osOK) {
            DP_OSAL_LOGE("Failed to destroy condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable destroyed\n");
        }
    }

private:
    void doWait(Mutex &mutex) {
        waitCount++;
        mutex.unlock();  // Release the mutex while waiting
        if (osSemaphoreAcquire(cond_, osWaitForever) != osOK) {
            DP_OSAL_LOGE("Failed to wait on condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable wait succeeded\n");
        }
        mutex.lock();  // Reacquire the mutex after waiting
        waitCount--;
    }

    bool doWaitFor(Mutex &mutex, uint32_t timeout) {
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
            DP_OSAL_LOGD("Condition variable waitFor succeeded\n");
            return true;
        } else if (result == osErrorTimeout) {
            DP_OSAL_LOGD("Condition variable waitFor timed out\n");
            return false;
        } else {
            DP_OSAL_LOGE("Failed to wait on condition variable with timeout\n");
            return false;
        }
    }

    void doNotifyOne() {
        if (osSemaphoreRelease(cond_) != osOK) {
            DP_OSAL_LOGE("Failed to notify one on condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable notifyOne succeeded\n");
        }
    }

    void doNotifyAll() {
        // Snapshot waitCount to avoid TOCTOU: a timed-out waiter may decrement
        // waitCount mid-loop, causing extra semaphore tokens (I8).
        int count = waitCount.load();
        for (int i = 0; i < count; ++i) {
            if (osSemaphoreRelease(cond_) != osOK) {
                DP_OSAL_LOGE("Failed to notify all on condition variable\n");
                break;
            }
        }
        DP_OSAL_LOGD("Condition variable notifyAll succeeded\n");
    }

    int doGetWaitCount() const { return waitCount.load(); }

    osSemaphoreId_t cond_;          // Condition variable semaphore
    std::atomic<int> waitCount{0};  // Number of waiting threads
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CONDITION_VAR */

#endif  // DP_OSAL_CMSIS_CONDITION_VARIABLE_H_
