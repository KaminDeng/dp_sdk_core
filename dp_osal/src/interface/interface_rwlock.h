/** @file interface_rwlock.h
 *  @brief CRTP reader-writer lock interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_RWLOCK_H_
#define DP_OSAL_INTERFACE_RWLOCK_H_

#include <cstddef>
#include <cstdint>

#if OSAL_ENABLE_RW_LOCK

namespace dp::osal {

template <typename Impl>
class RWLockBase {
public:
    void readLock() { impl().doReadLock(); }
    bool tryReadLock() { return impl().doTryReadLock(); }
    bool readLockFor(uint32_t timeout) { return impl().doReadLockFor(timeout); }
    void readUnlock() { impl().doReadUnlock(); }
    void writeLock() { impl().doWriteLock(); }
    bool tryWriteLock() { return impl().doTryWriteLock(); }
    bool writeLockFor(uint32_t timeout) { return impl().doWriteLockFor(timeout); }
    void writeUnlock() { impl().doWriteUnlock(); }
    [[nodiscard]] size_t getReadLockCount() const { return impl().doGetReadLockCount(); }
    [[nodiscard]] bool isWriteLocked() const { return impl().doIsWriteLocked(); }

protected:
    ~RWLockBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#else
namespace dp::osal {
/* RWLock disabled */
}
#endif /* OSAL_ENABLE_RW_LOCK */

#endif  // DP_OSAL_INTERFACE_RWLOCK_H_
