/** @file interface_mutex.h
 *  @brief CRTP mutex interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_MUTEX_H_
#define DP_OSAL_INTERFACE_MUTEX_H_

#include <cstdint>

#include "dp_osal_compat.h"
#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief CRTP base for mutex implementations. */
template <typename Impl>
class MutexBase {
public:
    DP_OSAL_HOT bool lock() {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doLock();
        const uint32_t finished = Measurement::now_us();
        Measurement::record_wait(SynchronizationKind::kLock, started, finished, result);
        return result;
    }
    DP_OSAL_HOT bool unlock() { return impl().doUnlock(); }
    bool tryLock() {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryLock();
        const uint32_t finished = Measurement::now_us();
        Measurement::record_wait(SynchronizationKind::kLock, started, finished, result);
        return result;
    }
    DP_OSAL_COLD bool tryLockFor(uint32_t timeout) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryLockFor(timeout);
        const uint32_t finished = Measurement::now_us();
        Measurement::record_wait(SynchronizationKind::kLock, started, finished, result);
        return result;
    }

protected:
    ~MutexBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_MUTEX_H_
