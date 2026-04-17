//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_CONDITION_VARIABLE_H_
#define DP_OSAL_POSIX_CONDITION_VARIABLE_H_

#if DP_OSAL_ENABLE_CONDITION_VAR

#include <pthread.h>

#include <atomic>
#include <chrono>

#include "interface_condition_variable.h"
#include "dp_osal_debug.h"
#include "dp_osal_mutex.h"

namespace dp::osal {

class ConditionVariable : public ConditionVariableBase<ConditionVariable> {
    friend class ConditionVariableBase<ConditionVariable>;

public:
    ConditionVariable() {
        if (pthread_cond_init(&cond_, nullptr) != 0) {
            DP_OSAL_LOGE("Failed to initialize condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable initialized\n");
        }
    }

    ~ConditionVariable() {
        if (pthread_cond_destroy(&cond_) != 0) {
            DP_OSAL_LOGE("Failed to destroy condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable destroyed\n");
        }
    }

private:
    void doWait(Mutex &mutex) {
        waitCount++;
        if (pthread_cond_wait(&cond_, &mutex.getNativeHandle()) != 0) {
            DP_OSAL_LOGE("Failed to wait on condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable wait succeeded\n");
        }
        waitCount--;
    }

    bool doWaitFor(Mutex &mutex, uint32_t timeout) {
        waitCount++;

        // 获取当前时间
        auto now = std::chrono::system_clock::now();

        // 计算超时时间
        auto timeoutTime = now + std::chrono::milliseconds(timeout);

        // 将超时时间转换为 timespec 结构体
        timespec ts;
        ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeoutTime.time_since_epoch()).count();
        ts.tv_nsec =
            std::chrono::duration_cast<std::chrono::nanoseconds>(timeoutTime.time_since_epoch()).count() % 1000000000;

        // 等待条件变量
        int result = pthread_cond_timedwait(&cond_, &mutex.getNativeHandle(), &ts);
        waitCount--;

        if (result == 0) {
            DP_OSAL_LOGD("Condition variable waitFor succeeded\n");
            return true;
        } else if (result == ETIMEDOUT) {
            DP_OSAL_LOGD("Condition variable waitFor timed out\n");
            return false;
        } else {
            DP_OSAL_LOGE("Failed to wait on condition variable with timeout\n");
            return false;
        }
    }

    void doNotifyOne() {
        if (pthread_cond_signal(&cond_) != 0) {
            DP_OSAL_LOGE("Failed to notify one on condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable notifyOne succeeded\n");
        }
    }

    void doNotifyAll() {
        if (pthread_cond_broadcast(&cond_) != 0) {
            DP_OSAL_LOGE("Failed to notify all on condition variable\n");
        } else {
            DP_OSAL_LOGD("Condition variable notifyAll succeeded\n");
        }
    }

    int doGetWaitCount() const { return waitCount.load(); }

    pthread_cond_t cond_;
    mutable std::atomic<int> waitCount{0};  // 当前等待线程数（原子，防止多线程竞争）
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CONDITION_VAR */

#endif  // DP_OSAL_POSIX_CONDITION_VARIABLE_H_
