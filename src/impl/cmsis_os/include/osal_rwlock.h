//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_RWLOCK_H__
#define __OSAL_RWLOCK_H__

#include "osal.h"
#include "interface_rwlock.h"
#include "osal_debug.h"

namespace osal {

class OSALRWLock : public IRWLock {
public:
    OSALRWLock() {
        osMutexAttr_t mutexAttr = {};
        mutexAttr.name = "RWLockMutex";
        mutexAttr.attr_bits = osMutexRecursive | osMutexPrioInherit;
        mutex_ = osMutexNew(&mutexAttr);

        osSemaphoreAttr_t semAttr = {};
        semAttr.name = "RWLockSemaphore";
        readSemaphore_ = osSemaphoreNew(1, 0, &semAttr);
        writeSemaphore_ = osSemaphoreNew(1, 0, &semAttr);

        if (mutex_ == nullptr || readSemaphore_ == nullptr || writeSemaphore_ == nullptr) {
            OSAL_LOGE("Failed to create RWLock\n");
            // 处理创建失败的情况
        } else {
            /* Binary semaphores are created with count=0 by xSemaphoreCreateBinary().
             * Explicitly release both to set the initial count to 1 (unlocked state). */
            osSemaphoreRelease(readSemaphore_);
            osSemaphoreRelease(writeSemaphore_);
        }
    }

    ~OSALRWLock() {
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

    void readLock() override {
        osMutexAcquire(mutex_, osWaitForever);
        osSemaphoreAcquire(readSemaphore_, osWaitForever);
        readCount_++;
        if (readCount_ == 1) {
            osSemaphoreAcquire(writeSemaphore_, osWaitForever);
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        OSAL_LOGD("Read lock acquired\n");
    }

    bool tryReadLock() override {
        if (osMutexAcquire(mutex_, 0) != osOK) {
            OSAL_LOGD("Try read lock failed\n");
            return false;
        }
        if (osSemaphoreAcquire(readSemaphore_, 0) != osOK) {
            osMutexRelease(mutex_);
            OSAL_LOGD("Try read lock failed\n");
            return false;
        }
        readCount_++;
        if (readCount_ == 1) {
            if (osSemaphoreAcquire(writeSemaphore_, 0) != osOK) {
                readCount_--;
                osSemaphoreRelease(readSemaphore_);
                osMutexRelease(mutex_);
                OSAL_LOGD("Try read lock failed\n");
                return false;
            }
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        OSAL_LOGD("Try read lock succeeded\n");
        return true;
    }

    bool readLockFor(uint32_t timeout) override {
        if (osMutexAcquire(mutex_, timeout) != osOK) {
            OSAL_LOGD("Read lock with timeout failed\n");
            return false;
        }
        if (osSemaphoreAcquire(readSemaphore_, timeout) != osOK) {
            osMutexRelease(mutex_);
            OSAL_LOGD("Read lock with timeout failed\n");
            return false;
        }
        readCount_++;
        if (readCount_ == 1) {
            if (osSemaphoreAcquire(writeSemaphore_, timeout) != osOK) {
                readCount_--;
                osSemaphoreRelease(readSemaphore_);
                osMutexRelease(mutex_);
                OSAL_LOGD("Read lock with timeout failed\n");
                return false;
            }
        }
        osSemaphoreRelease(readSemaphore_);
        osMutexRelease(mutex_);
        OSAL_LOGD("Read lock with timeout succeeded\n");
        return true;
    }

    void readUnlock() override {
        osSemaphoreAcquire(readSemaphore_, osWaitForever);
        readCount_--;
        if (readCount_ == 0) {
            osSemaphoreRelease(writeSemaphore_);
        }
        osSemaphoreRelease(readSemaphore_);
        OSAL_LOGD("Read lock released\n");
    }

    void writeLock() override {
        osSemaphoreAcquire(writeSemaphore_, osWaitForever);
        OSAL_LOGD("Write lock acquired\n");
    }

    bool tryWriteLock() override {
        if (osSemaphoreAcquire(writeSemaphore_, 0) != osOK) {
            OSAL_LOGD("Try write lock failed\n");
            return false;
        }
        OSAL_LOGD("Try write lock succeeded\n");
        return true;
    }

    bool writeLockFor(uint32_t timeout) override {
        if (osSemaphoreAcquire(writeSemaphore_, timeout) != osOK) {
            OSAL_LOGD("Write lock with timeout failed\n");
            return false;
        }
        OSAL_LOGD("Write lock with timeout succeeded\n");
        return true;
    }

    void writeUnlock() override {
        osSemaphoreRelease(writeSemaphore_);
        OSAL_LOGD("Write lock released\n");
    }

    size_t getReadLockCount() const override {
        // 这个功能在标准库中没有直接支持，通常需要自定义实现。
        // 这里仅作为示例，返回0。
        OSAL_LOGD("Requested read lock count\n");
        return readCount_;
    }

    bool isWriteLocked() const override {
        // 这个功能在标准库中没有直接支持，通常需要自定义实现。
        // 这里仅作为示例，返回false。
        OSAL_LOGD("Requested write lock status\n");
        return osSemaphoreGetCount(writeSemaphore_) == 0;
    }

private:
    osMutexId_t mutex_;
    osSemaphoreId_t readSemaphore_;
    osSemaphoreId_t writeSemaphore_;
    mutable uint32_t readCount_ = 0;
};

}  // namespace osal

#endif  // __OSAL_RWLOCK_H__