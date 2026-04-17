//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_MUTEX_H_
#define DP_OSAL_CMSIS_MUTEX_H_

#include "interface_mutex.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class Mutex : public MutexBase<Mutex> {
    friend class MutexBase<Mutex>;

public:
    Mutex() {
        osMutexAttr_t mutexAttr = {};
        mutexAttr.name = "Mutex";
        mutexAttr.attr_bits = osMutexRecursive | osMutexPrioInherit;
        mutex_ = osMutexNew(&mutexAttr);
        if (mutex_ == nullptr) {
            DP_OSAL_LOGE("Failed to initialize mutex\n");
        } else {
            DP_OSAL_LOGD("Mutex initialized\n");
        }
    }

    ~Mutex() {
        if (osMutexDelete(mutex_) != osOK) {
            DP_OSAL_LOGE("Failed to destroy mutex\n");
        } else {
            DP_OSAL_LOGD("Mutex destroyed\n");
        }
    }

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    osMutexId_t getNativeHandle() { return mutex_; }

private:
    bool doLock() {
        osStatus_t status = osMutexAcquire(mutex_, osWaitForever);
        if (status != osOK) {
            DP_OSAL_LOGE("Failed to lock mutex, status code %d\n", status);
            return false;
        }
        DP_OSAL_LOGD("Mutex locked successfully\n");
        return true;
    }

    bool doUnlock() {
        osStatus_t status = osMutexRelease(mutex_);
        if (status != osOK) {
            DP_OSAL_LOGE("Failed to unlock mutex, status code %d\n", status);
            return false;
        }
        DP_OSAL_LOGD("Mutex unlocked successfully\n");
        return true;
    }

    bool doTryLock() {
        osStatus_t status = osMutexAcquire(mutex_, 0);
        if (status != osOK) {
            DP_OSAL_LOGD("Failed to try lock mutex, status code %d\n", status);
            return false;
        }
        DP_OSAL_LOGD("Mutex try lock successful\n");
        return true;
    }

    bool doTryLockFor(uint32_t timeout) {
        osStatus_t status = osMutexAcquire(mutex_, timeout);
        if (status != osOK) {
            DP_OSAL_LOGD("Failed to timed lock mutex, status code %d\n", status);
            return false;
        }
        DP_OSAL_LOGD("Mutex timed lock successful\n");
        return true;
    }

    osMutexId_t mutex_;
};

} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_MUTEX_H_
