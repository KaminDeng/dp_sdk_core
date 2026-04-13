//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_CMSIS_MUTEX_H_
#define OSAL_CMSIS_MUTEX_H_

#include "interface_mutex.h"
#include "osal.h"
#include "osal_debug.h"

namespace osal {

class OSALMutex : public MutexBase<OSALMutex> {
    friend class MutexBase<OSALMutex>;

public:
    OSALMutex() {
        osMutexAttr_t mutexAttr = {};
        mutexAttr.name = "OSALMutex";
        mutexAttr.attr_bits = osMutexRecursive | osMutexPrioInherit;
        mutex_ = osMutexNew(&mutexAttr);
        if (mutex_ == nullptr) {
            OSAL_LOGE("Failed to initialize mutex\n");
        } else {
            OSAL_LOGD("Mutex initialized\n");
        }
    }

    ~OSALMutex() {
        if (osMutexDelete(mutex_) != osOK) {
            OSAL_LOGE("Failed to destroy mutex\n");
        } else {
            OSAL_LOGD("Mutex destroyed\n");
        }
    }

    OSALMutex(const OSALMutex &) = delete;
    OSALMutex &operator=(const OSALMutex &) = delete;

    osMutexId_t getNativeHandle() { return mutex_; }

private:
    bool doLock() {
        osStatus_t status = osMutexAcquire(mutex_, osWaitForever);
        if (status != osOK) {
            OSAL_LOGE("Failed to lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex locked successfully\n");
        return true;
    }

    bool doUnlock() {
        osStatus_t status = osMutexRelease(mutex_);
        if (status != osOK) {
            OSAL_LOGE("Failed to unlock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex unlocked successfully\n");
        return true;
    }

    bool doTryLock() {
        osStatus_t status = osMutexAcquire(mutex_, 0);
        if (status != osOK) {
            OSAL_LOGD("Failed to try lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex try lock successful\n");
        return true;
    }

    bool doTryLockFor(uint32_t timeout) {
        osStatus_t status = osMutexAcquire(mutex_, timeout);
        if (status != osOK) {
            OSAL_LOGD("Failed to timed lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex timed lock successful\n");
        return true;
    }

    osMutexId_t mutex_;
};

}  // namespace osal

#endif  // OSAL_CMSIS_MUTEX_H_
