//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_POSIX_RWLOCK_H_
#define OSAL_POSIX_RWLOCK_H_

#if OSAL_ENABLE_RW_LOCK

#include <atomic>
#include <chrono>
#include <shared_mutex>

#include "interface_rwlock.h"
#include "osal_debug.h"

namespace osal {

class OSALRWLock : public RWLockBase<OSALRWLock> {
    friend class RWLockBase<OSALRWLock>;

public:
    OSALRWLock() = default;

    ~OSALRWLock() = default;

private:
    void doReadLock() {
        sharedMutex_.lock_shared();
        readCount_.fetch_add(1);
        OSAL_LOGD("Read lock acquired\n");
    }

    bool doTryReadLock() {
        bool result = sharedMutex_.try_lock_shared();
        OSAL_LOGD("Try read lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doReadLockFor(uint32_t timeout) {
        bool result = sharedMutex_.try_lock_shared_for(std::chrono::milliseconds(timeout));
        OSAL_LOGD("Read lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void doReadUnlock() {
        readCount_.fetch_sub(1);
        sharedMutex_.unlock_shared();
        OSAL_LOGD("Read lock released\n");
    }

    void doWriteLock() {
        sharedMutex_.lock();
        writeLocked_.store(true);
        OSAL_LOGD("Write lock acquired\n");
    }

    bool doTryWriteLock() {
        bool result = sharedMutex_.try_lock();
        OSAL_LOGD("Try write lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doWriteLockFor(uint32_t timeout) {
        bool result = sharedMutex_.try_lock_for(std::chrono::milliseconds(timeout));
        OSAL_LOGD("Write lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void doWriteUnlock() {
        writeLocked_.store(false);
        sharedMutex_.unlock();
        OSAL_LOGD("Write lock released\n");
    }

    size_t doGetReadLockCount() const { return readCount_.load(); }

    bool doIsWriteLocked() const { return writeLocked_.load(); }
    std::atomic<size_t> readCount_{0};
    std::atomic<bool> writeLocked_{false};
    mutable std::shared_timed_mutex sharedMutex_;
};

}  // namespace osal

#endif /* OSAL_ENABLE_RW_LOCK */

#endif  // OSAL_POSIX_RWLOCK_H_
