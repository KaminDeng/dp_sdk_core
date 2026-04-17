/** @file interface_mutex.h
 *  @brief CRTP mutex interface for OSAL. */
#ifndef OSAL_INTERFACE_MUTEX_H_
#define OSAL_INTERFACE_MUTEX_H_

#include <cstdint>

#include "osal_compat.h"

namespace osal {

/** @brief CRTP base for mutex implementations. */
template <typename Impl>
class MutexBase {
public:
    OSAL_HOT bool lock() { return impl().doLock(); }
    OSAL_HOT bool unlock() { return impl().doUnlock(); }
    bool tryLock() { return impl().doTryLock(); }
    OSAL_COLD bool tryLockFor(uint32_t timeout) { return impl().doTryLockFor(timeout); }

protected:
    ~MutexBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_MUTEX_H_
