/** @file interface_memory_manager.h
 *  @brief CRTP interface for OSAL memory pool/heap management. */
#ifndef DP_OSAL_INTERFACE_MEMORY_MANAGER_H_
#define DP_OSAL_INTERFACE_MEMORY_MANAGER_H_

#include <cstddef>

#if OSAL_ENABLE_MEMORY_MANAGER

namespace dp::osal {

template <typename Impl>
class MemoryManagerBase {
public:
    bool initialize(size_t block_size, size_t block_count) { return impl().doInitialize(block_size, block_count); }
    void *allocate(size_t size) { return impl().doAllocate(size); }
    void deallocate(void *ptr) { impl().doDeallocate(ptr); }
    void *reallocate(void *ptr, size_t newSize) { return impl().doReallocate(ptr, newSize); }
    void *allocateAligned(size_t size, size_t alignment) { return impl().doAllocateAligned(size, alignment); }
    [[nodiscard]] size_t getAllocatedSize() const { return impl().doGetAllocatedSize(); }

protected:
    ~MemoryManagerBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif /* OSAL_ENABLE_MEMORY_MANAGER */

#endif  // DP_OSAL_INTERFACE_MEMORY_MANAGER_H_
