/** @file interface_mutex.h
 *  @brief CRTP mutex interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_MUTEX_H_
#define DP_OSAL_INTERFACE_MUTEX_H_

#include <cstdint>

#include "dp_osal_compat.h"

namespace dp::osal {

/** @brief CRTP base for mutex implementations. */
template <typename Impl>
class MutexBase {
public:
    DP_OSAL_HOT bool lock() { return impl().doLock(); }
    DP_OSAL_HOT bool unlock() { return impl().doUnlock(); }
    bool tryLock() { return impl().doTryLock(); }
    DP_OSAL_COLD bool tryLockFor(uint32_t timeout) { return impl().doTryLockFor(timeout); }

protected:
    ~MutexBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_MUTEX_H_
