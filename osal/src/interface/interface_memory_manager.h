/** @file interface_memory_manager.h
 *  @brief Abstract interface for OSAL memory pool/heap management. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef IMEM_POOL_H_
#define IMEM_POOL_H_

#if OSAL_ENABLE_MEMORY_MANAGER

namespace osal {

/** @brief Abstract memory-manager interface.
 *
 *  Provides a portable allocation API over a fixed-size pool or heap.
 *  Concrete implementations may use a TLSF allocator, FreeRTOS heap, or
 *  the system @c malloc. */
class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;

    /** @brief Initialises the memory pool with fixed-size blocks.
     *  @param block_size   Size of each block in bytes.
     *  @param block_count  Total number of blocks in the pool.
     *  @return @c true on success, @c false on failure. */
    virtual bool initialize(size_t block_size, size_t block_count) = 0;

    /** @brief Allocates a block of at least @p size bytes.
     *  @param size  Requested allocation size in bytes.
     *  @return Pointer to allocated memory, or @c nullptr on failure. */
    virtual void *allocate(size_t size) = 0;

    /** @brief Returns a previously allocated block to the pool.
     *  @param ptr  Pointer returned by @c allocate() or @c allocateAligned(). */
    virtual void deallocate(void *ptr) = 0;

    /** @brief Resizes an existing allocation.
     *  @param ptr      Pointer to an existing allocation.
     *  @param newSize  Desired new size in bytes.
     *  @return Pointer to the resized block, or @c nullptr on failure. */
    virtual void *reallocate(void *ptr, size_t newSize) = 0;

    /** @brief Allocates a block with a specific alignment.
     *  @param size       Requested allocation size in bytes.
     *  @param alignment  Required alignment (must be a power of two).
     *  @return Aligned pointer, or @c nullptr on failure. */
    virtual void *allocateAligned(size_t size, size_t alignment) = 0;

    /** @brief Returns the total number of bytes currently allocated.
     *  @return Bytes currently in use. */
    [[nodiscard]] virtual size_t getAllocatedSize() const = 0;
};

}  // namespace osal

#endif /* OSAL_ENABLE_MEMORY_MANAGER */

#endif  // IMEM_POOL_H_
