
#ifndef DP_OSAL_INTERFACE_MUTEX_H_
#define DP_OSAL_INTERFACE_MUTEX_H_

#include <cstdint>

#include "dp_osal_compat.h"
#include "dp_osal_measurement.h"

namespace dp::osal {


template <typename Impl>
class MutexBase {
public:

    DP_OSAL_HOT bool lock() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doLock();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doLock();
    }

    DP_OSAL_HOT bool unlock() { return impl().doUnlock(); }

    bool tryLock() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryLock();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryLock();
    }

    DP_OSAL_COLD bool tryLockFor(uint32_t timeout) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryLockFor(timeout);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryLockFor(timeout);
    }

protected:

    ~MutexBase() = default;

private:

    Impl &impl() { return *static_cast<Impl *>(this); }

    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_MUTEX_H_
