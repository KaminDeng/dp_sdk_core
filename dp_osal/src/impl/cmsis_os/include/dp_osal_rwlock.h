//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_RWLOCK_H_
#define DP_OSAL_CMSIS_RWLOCK_H_

#if DP_OSAL_ENABLE_RW_LOCK

#include "interface_rwlock.h"
#include "osal.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class RWLock : public RWLockBase<RWLock> {
    friend class RWLockBase<RWLock>;

public:
    RWLock() {
        osMutexAttr_t mutexAttr = {};
        mutexAttr.name = "RWLockMutex";
        mutexAttr.attr_bits = osMutexRecursive | osMutexPrioInherit;
        mutex_ = osMutexNew(&mutexAttr);

        osSemaphoreAttr_t semAttr = {};
        semAttr.name = "RWLockSemaphore";
        /* Use max_count=2 to create counting semaphores (not binary).
         * Binary semaphores (max=1) created via xSemaphoreCreateBinary() start
         * at count=0 and osSemaphoreRelease may fail in task context on some
         * FreeRTOS POSIX port versions. Counting semaphores with max=2,
         * initial_count=1 give the same 0/1 semantics we need. */
        readSemaphore_ = osSemaphoreNew(2, 1, &semAttr);
        writeSemaphore_ = osSemaphoreNew(2, 1, &semAttr);

        if (mutex_ == nullptr || readSemaphore_ == nullptr || writeSemaphore_ == nullptr) {
            DP_OSAL_LOGE("Failed to create RWLock\n");
        }
    }

    ~RWLock() {
        if (mutex_ != nullptr) {
            osMutexDelete(mutex_);
        }
        if (readSemaphore_ != nullptr) {
            osSemaphoreDelete(readSemaphore_);
        }
        if (writeSemaphore_ != nullptr) {
            osSemaphoreDelete(writeSemaphore_);
        }
    }

private:
    void doReadLock() {
        osMutexAcquire(mutex_, osWaitForever);
        osSemaphoreAcquire(readSemaphore_, osWaitForever);
        readCount_++;
        if (readCount_ == 1) {
            osSemaphoreAcquire(writeSemaphore_, osWaitForever);
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        DP_OSAL_LOGD("Read lock acquired\n");
    }

    bool doTryReadLock() {
        if (osMutexAcquire(mutex_, 0) != osOK) {
            DP_OSAL_LOGD("Try read lock failed\n");
            return false;
        }
        if (osSemaphoreAcquire(readSemaphore_, 0) != osOK) {
            osMutexRelease(mutex_);
            DP_OSAL_LOGD("Try read lock failed\n");
            return false;
        }
        readCount_++;
        if (readCount_ == 1) {
            if (osSemaphoreAcquire(writeSemaphore_, 0) != osOK) {
                readCount_--;
                osSemaphoreRelease(readSemaphore_);
                osMutexRelease(mutex_);
                DP_OSAL_LOGD("Try read lock failed\n");
                return false;
            }
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        DP_OSAL_LOGD("Try read lock succeeded\n");
        return true;
    }

    bool doReadLockFor(uint32_t timeout) {
        if (osMutexAcquire(mutex_, timeout) != osOK) {
            DP_OSAL_LOGD("Read lock with timeout failed\n");
            return false;
        }
        if (osSemaphoreAcquire(readSemaphore_, timeout) != osOK) {
            osMutexRelease(mutex_);
            DP_OSAL_LOGD("Read lock with timeout failed\n");
            return false;
        }
        readCount_++;
        if (readCount_ == 1) {
            if (osSemaphoreAcquire(writeSemaphore_, timeout) != osOK) {
                readCount_--;
                osSemaphoreRelease(readSemaphore_);
                osMutexRelease(mutex_);
                DP_OSAL_LOGD("Read lock with timeout failed\n");
                return false;
            }
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        DP_OSAL_LOGD("Read lock with timeout succeeded\n");
        return true;
    }

    void doReadUnlock() {
        osSemaphoreAcquire(readSemaphore_, osWaitForever);
        readCount_--;
        if (readCount_ == 0) {
            osSemaphoreRelease(writeSemaphore_);
        }
        osSemaphoreRelease(readSemaphore_);
        DP_OSAL_LOGD("Read lock released\n");
    }

    void doWriteLock() {
        osSemaphoreAcquire(writeSemaphore_, osWaitForever);
        writeLocked_ = true;
        DP_OSAL_LOGD("Write lock acquired\n");
    }

    bool doTryWriteLock() {
        if (osSemaphoreAcquire(writeSemaphore_, 0) != osOK) {
            DP_OSAL_LOGD("Try write lock failed\n");
            return false;
        }
        writeLocked_ = true;
        DP_OSAL_LOGD("Try write lock succeeded\n");
        return true;
    }

    bool doWriteLockFor(uint32_t timeout) {
        if (osSemaphoreAcquire(writeSemaphore_, timeout) != osOK) {
            DP_OSAL_LOGD("Write lock with timeout failed\n");
            return false;
        }
        writeLocked_ = true;
        DP_OSAL_LOGD("Write lock with timeout succeeded\n");
        return true;
    }

    void doWriteUnlock() {
        writeLocked_ = false;
        osSemaphoreRelease(writeSemaphore_);
        DP_OSAL_LOGD("Write lock released\n");
    }

    size_t doGetReadLockCount() const {
        DP_OSAL_LOGD("Requested read lock count\n");
        return readCount_;
    }

    bool doIsWriteLocked() const {
        /* True only when a writer holds the lock — not when readers do.
         * Readers also acquire writeSemaphore_ (to block writers), but
         * isWriteLocked() must distinguish the two cases. */
        DP_OSAL_LOGD("Requested write lock status\n");
        return writeLocked_;
    }

    osMutexId_t mutex_;
    osSemaphoreId_t readSemaphore_;
    osSemaphoreId_t writeSemaphore_;
    mutable uint32_t readCount_ = 0;
    mutable bool writeLocked_ = false;
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_RW_LOCK */

#endif  // DP_OSAL_CMSIS_RWLOCK_H_
