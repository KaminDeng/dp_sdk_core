//
// Created by kamin.deng on 2024/8/22.
//
#ifndef _OSAL_MUTEX_H__
#define _OSAL_MUTEX_H__

#include "interface_mutex.h"
#include "osal.h"
#include "osal_debug.h"

namespace osal {

class OSALMutex : public IMutex {
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

    bool lock() override {
        osStatus_t status = osMutexAcquire(mutex_, osWaitForever);
        if (status != osOK) {
            OSAL_LOGE("Failed to lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex locked successfully\n");
        return true;
    }

    bool unlock() override {
        osStatus_t status = osMutexRelease(mutex_);
        if (status != osOK) {
            OSAL_LOGE("Failed to unlock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex unlocked successfully\n");
        return true;
    }

    bool tryLock() override {
        osStatus_t status = osMutexAcquire(mutex_, 0);
        if (status != osOK) {
            OSAL_LOGD("Failed to try lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex try lock successful\n");
        return true;
    }

    bool tryLockFor(const uint32_t timeout) override {
        osStatus_t status = osMutexAcquire(mutex_, timeout);
        if (status != osOK) {
            OSAL_LOGD("Failed to timed lock mutex, status code %d\n", status);
            return false;
        }
        OSAL_LOGD("Mutex timed lock successful\n");
        return true;
    }

    osMutexId_t getNativeHandle() { return mutex_; }

private:
    osMutexId_t mutex_;
};

}  // namespace osal

#endif  // _OSAL_MUTEX_H__