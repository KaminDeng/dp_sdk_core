//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_RWLOCK_H__
#define __OSAL_RWLOCK_H__

#include <atomic>
#include <chrono>
#include <shared_mutex>

#include "interface_rwlock.h"
#include "osal_debug.h"

namespace osal {

class OSALRWLock : public IRWLock {
public:
    OSALRWLock() = default;

    ~OSALRWLock() = default;

    void readLock() override {
        sharedMutex_.lock_shared();
        readCount_.fetch_add(1);
        OSAL_LOGD("Read lock acquired\n");
    }

    bool tryReadLock() override {
        bool result = sharedMutex_.try_lock_shared();
        OSAL_LOGD("Try read lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool readLockFor(uint32_t timeout) override {
        bool result = sharedMutex_.try_lock_shared_for(std::chrono::milliseconds(timeout));
        OSAL_LOGD("Read lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void readUnlock() override {
        readCount_.fetch_sub(1);
        sharedMutex_.unlock_shared();
        OSAL_LOGD("Read lock released\n");
    }

    void writeLock() override {
        sharedMutex_.lock();
        writeLocked_.store(true);
        OSAL_LOGD("Write lock acquired\n");
    }

    bool tryWriteLock() override {
        bool result = sharedMutex_.try_lock();
        OSAL_LOGD("Try write lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool writeLockFor(uint32_t timeout) override {
        bool result = sharedMutex_.try_lock_for(std::chrono::milliseconds(timeout));
        OSAL_LOGD("Write lock with timeout %s\n", result ? "succeeded" : "failed");
        return result;
    }

    void writeUnlock() override {
        writeLocked_.store(false);
        sharedMutex_.unlock();
        OSAL_LOGD("Write lock released\n");
    }

    size_t getReadLockCount() const override { return readCount_.load(); }

    bool isWriteLocked() const override { return writeLocked_.load(); }

private:
    std::atomic<size_t> readCount_{0};
    std::atomic<bool> writeLocked_{false};
    mutable std::shared_timed_mutex sharedMutex_;
};

}  // namespace osal

#endif  // __OSAL_RWLOCK_H__