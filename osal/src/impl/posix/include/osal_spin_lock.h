//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_SPINLOCK_H__
#define __OSAL_SPINLOCK_H__

#if OSAL_ENABLE_SPIN_LOCK

#include <atomic>
#include <chrono>

#include "interface_spin_lock.h"
#include "osal_debug.h"

namespace osal {

class OSALSpinLock : public SpinLockBase<OSALSpinLock> {
    friend class SpinLockBase<OSALSpinLock>;

public:
    OSALSpinLock() : flag_{false} {}

    ~OSALSpinLock() = default;

private:
    void doLock() {
        while (flag_.exchange(true, std::memory_order_acquire)) {
            // 自旋等待
        }
        OSAL_LOGD("Lock acquired\n");
    }

    bool doTryLock() {
        bool expected = false;
        bool result =
            flag_.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed);
        OSAL_LOGD("Try lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doLockFor(uint32_t timeout) {
        auto start = std::chrono::steady_clock::now();
        std::chrono::milliseconds timeout_duration(timeout);

        while (flag_.exchange(true, std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() - start >= timeout_duration) {
                OSAL_LOGD("Lock with timeout failed\n");
                return false;
            }
            // 自旋等待
        }
        OSAL_LOGD("Lock with timeout succeeded\n");
        return true;
    }

    void doUnlock() {
        flag_.store(false, std::memory_order_release);
        OSAL_LOGD("Lock released\n");
    }

    bool doIsLocked() const {
        bool result = flag_.load(std::memory_order_relaxed);
        OSAL_LOGD("Requested lock status: %s\n", result ? "locked" : "unlocked");
        return result;
    }

    std::atomic<bool> flag_;
};

}  // namespace osal

#endif /* OSAL_ENABLE_SPIN_LOCK */

#endif  // __OSAL_SPINLOCK_H__
