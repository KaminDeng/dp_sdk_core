//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_SPIN_LOCK_H_
#define DP_OSAL_POSIX_SPIN_LOCK_H_

#if DP_OSAL_ENABLE_SPIN_LOCK

#include <atomic>
#include <chrono>

#include "interface_spin_lock.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class SpinLock : public SpinLockBase<SpinLock> {
    friend class SpinLockBase<SpinLock>;

public:
    SpinLock() : flag_{false} {}

    ~SpinLock() = default;

private:
    void doLock() {
        while (flag_.exchange(true, std::memory_order_acquire)) {
            // 自旋等待
        }
        DP_OSAL_LOGD("Lock acquired\n");
    }

    bool doTryLock() {
        bool expected = false;
        bool result =
            flag_.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed);
        DP_OSAL_LOGD("Try lock %s\n", result ? "succeeded" : "failed");
        return result;
    }

    bool doLockFor(uint32_t timeout) {
        auto start = std::chrono::steady_clock::now();
        std::chrono::milliseconds timeout_duration(timeout);

        while (flag_.exchange(true, std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() - start >= timeout_duration) {
                DP_OSAL_LOGD("Lock with timeout failed\n");
                return false;
            }
            // 自旋等待
        }
        DP_OSAL_LOGD("Lock with timeout succeeded\n");
        return true;
    }

    void doUnlock() {
        flag_.store(false, std::memory_order_release);
        DP_OSAL_LOGD("Lock released\n");
    }

    bool doIsLocked() const {
        bool result = flag_.load(std::memory_order_relaxed);
        DP_OSAL_LOGD("Requested lock status: %s\n", result ? "locked" : "unlocked");
        return result;
    }

    std::atomic<bool> flag_;
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_SPIN_LOCK */

#endif  // DP_OSAL_POSIX_SPIN_LOCK_H_
