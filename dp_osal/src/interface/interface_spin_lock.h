/** @file interface_spin_lock.h
 *  @brief CRTP spin-lock interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_SPIN_LOCK_H_
#define DP_OSAL_INTERFACE_SPIN_LOCK_H_

#include <cstdint>

#if DP_OSAL_ENABLE_SPIN_LOCK

namespace dp::osal {

template <typename Impl>
class SpinLockBase {
public:
    void lock() { impl().doLock(); }
    bool tryLock() { return impl().doTryLock(); }
    bool lockFor(uint32_t timeout) { return impl().doLockFor(timeout); }
    void unlock() { impl().doUnlock(); }
    [[nodiscard]] bool isLocked() const { return impl().doIsLocked(); }

protected:
    ~SpinLockBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#else
namespace dp::osal {
/* SpinLock disabled */
}
#endif /* DP_OSAL_ENABLE_SPIN_LOCK */

#endif  // DP_OSAL_INTERFACE_SPIN_LOCK_H_
