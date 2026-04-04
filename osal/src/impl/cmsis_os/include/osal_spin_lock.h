//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_SPINLOCK_H__
#define __OSAL_SPINLOCK_H__

#if OSAL_ENABLE_SPIN_LOCK

#include <atomic>

#include "interface_spin_lock.h"
#include "osal.h"
#include "osal_debug.h"

namespace osal {

class OSALSpinLock : public SpinLockBase<OSALSpinLock> {
    friend class SpinLockBase<OSALSpinLock>;

public:
    OSALSpinLock() : locked_(false) {
        osMutexAttr_t mutexAttr = {};
        mutexAttr.name = "OSALSpinLock";
        mutexAttr.attr_bits = osMutexRecursive | osMutexPrioInherit;
        mutex_ = osMutexNew(&mutexAttr);
        if (mutex_ == nullptr) {
            OSAL_LOGE("Failed to create mutex\n");
        } else {
            OSAL_LOGD("SpinLock initialized\n");
        }
    }

    ~OSALSpinLock() {
        if (mutex_ != nullptr) {
            osMutexDelete(mutex_);
            OSAL_LOGD("SpinLock destroyed\n");
        }
    }

private:
    void doLock() {
        if (osMutexAcquire(mutex_, osWaitForever) == osOK) {
            locked_.store(true, std::memory_order_release);
            OSAL_LOGD("Lock acquired\n");
        } else {
            OSAL_LOGE("Lock acquisition failed\n");
        }
    }

    bool doTryLock() {
        if (osMutexAcquire(mutex_, 0) == osOK) {
            locked_.store(true, std::memory_order_release);
            OSAL_LOGD("Try lock succeeded\n");
            return true;
        }
        OSAL_LOGD("Try lock failed\n");
        return false;
    }

    bool doLockFor(uint32_t timeout) {
        if (osMutexAcquire(mutex_, timeout) == osOK) {
            locked_.store(true, std::memory_order_release);
            OSAL_LOGD("Lock with timeout succeeded\n");
            return true;
        }
        OSAL_LOGD("Lock with timeout failed\n");
        return false;
    }

    void doUnlock() {
        locked_.store(false, std::memory_order_release);
        if (osMutexRelease(mutex_) == osOK) {
            OSAL_LOGD("Lock released\n");
        } else {
            OSAL_LOGE("Lock release failed\n");
        }
    }

    /* Use a dedicated atomic flag instead of osMutexGetOwner().
     * osMutexGetOwner() can block on FreeRTOS POSIX simulation due to
     * internal scheduler interactions, causing TestOSALSpinLock.Lock
     * to deadlock. The atomic flag is updated under the mutex on
     * acquire/release, giving correct visibility without OS calls. */
    [[nodiscard]] bool doIsLocked() const {
        bool result = locked_.load(std::memory_order_acquire);
        OSAL_LOGD("Requested lock status: %s\n", result ? "locked" : "unlocked");
        return result;
    }

    osMutexId_t mutex_;
    std::atomic<bool> locked_;
};

}  // namespace osal

#endif /* OSAL_ENABLE_SPIN_LOCK */

#endif  // __OSAL_SPINLOCK_H__
