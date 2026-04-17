//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_RWLOCK_H_
#define DP_OSAL_POSIX_RWLOCK_H_

#if DP_OSAL_ENABLE_RW_LOCK

#include <atomic>
#include <chrono>
#include <shared_mutex>

#include "interface_rwlock.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class RWLock : public RWLockBase<RWLock> {
    friend class RWLockBase<RWLock>;

public:
    RWLock() = default;

    ~RWLock() = default;

private:
    void doReadLock() {
        sharedMutex_.lock_shared();
        readCount_.fetch_add(1);
        DP_OSAL_LOGD("Read lock acquired\n");
    }

    bool doTryReadLock() {
        bool result = sharedMutex_.try_lock_shared();
        DP_OSAL_LOGD("Try read lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doReadLockFor(uint32_t timeout) {
        bool result = sharedMutex_.try_lock_shared_for(std::chrono::milliseconds(timeout));
        DP_OSAL_LOGD("Read lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void doReadUnlock() {
        readCount_.fetch_sub(1);
        sharedMutex_.unlock_shared();
        DP_OSAL_LOGD("Read lock released\n");
    }

    void doWriteLock() {
        sharedMutex_.lock();
        writeLocked_.store(true);
        DP_OSAL_LOGD("Write lock acquired\n");
    }

    bool doTryWriteLock() {
        bool result = sharedMutex_.try_lock();
        DP_OSAL_LOGD("Try write lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doWriteLockFor(uint32_t timeout) {
        bool result = sharedMutex_.try_lock_for(std::chrono::milliseconds(timeout));
        DP_OSAL_LOGD("Write lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void doWriteUnlock() {
        writeLocked_.store(false);
        sharedMutex_.unlock();
        DP_OSAL_LOGD("Write lock released\n");
    }

    size_t doGetReadLockCount() const { return readCount_.load(); }

    bool doIsWriteLocked() const { return writeLocked_.load(); }
    std::atomic<size_t> readCount_{0};
    std::atomic<bool> writeLocked_{false};
    mutable std::shared_timed_mutex sharedMutex_;
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_RW_LOCK */

#endif  // DP_OSAL_POSIX_RWLOCK_H_
