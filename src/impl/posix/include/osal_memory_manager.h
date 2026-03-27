//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_MEMORY_MANAGER_H__
#define __OSAL_MEMORY_MANAGER_H__

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

#include "interface_memory_manager.h"
#include "osal_debug.h"

namespace osal {

class OSALMemoryManager : public IMemoryManager {
public:
    OSALMemoryManager(size_t block_size, size_t block_count)
        : pool_(nullptr), freeList_(nullptr), blockSize_(block_size), blockCount_(block_count) {
        initialize(block_size, block_count);
    }

    ~OSALMemoryManager() override {
        if (pool_ != nullptr) {
            std::free(pool_);
            pool_ = nullptr;
        }
    }

    bool initialize(size_t block_size, size_t block_count) override {
        blockSize_ = block_size;
        blockCount_ = block_count;
        size_t poolSize = blockSize_ * blockCount_;

        pool_ = std::malloc(poolSize);
        if (pool_ == nullptr) {
            OSAL_LOGE("MemoryPool initialization failed: unable to allocate memory.");
            return false;
        }

        freeList_ = reinterpret_cast<void **>(pool_);
        // Build the free-list: write the address of the NEXT block at the
        // START of EACH block (step by blockSize_, not by sizeof(void*)).
        for (size_t i = 0; i < blockCount_ - 1; ++i) {
            void **slot = reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(pool_) + i * blockSize_);
            *slot = reinterpret_cast<uint8_t *>(pool_) + (i + 1) * blockSize_;
        }
        // Last block's next pointer is nullptr
        void **lastSlot =
            reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(pool_) + (blockCount_ - 1) * blockSize_);
        *lastSlot = nullptr;

        OSAL_LOGD("MemoryPool initialized with block size: %zu, block count: %zu.", blockSize_, blockCount_);
        return true;
    }

    void *allocate(size_t size) override {
        if (size > blockSize_) {
            OSAL_LOGE("Allocation failed: requested size %zu exceeds block size %zu.", size, blockSize_);
            return nullptr;
        }

        if (freeList_ == nullptr) {
            OSAL_LOGE("Allocation failed: no free blocks available.");
            return nullptr;
        }

        void *block = freeList_;
        freeList_ = reinterpret_cast<void **>(*freeList_);
        OSAL_LOGD("Allocated block at address: %p.", block);
        return block;
    }

    void deallocate(void *ptr) override {
        if (ptr == nullptr) {
            OSAL_LOGE("Deallocate failed: pointer is null.");
            return;
        }
        // Determine if ptr came from allocate() (raw block start) or allocateAligned()
        // (offset within a raw block). allocate() returns pool_ + k*blockSize_ exactly;
        // allocateAligned() returns raw + offset where offset >= 2*sizeof(uintptr_t).
        // Reading meta[-2] is only safe when ptr is NOT a raw block start (i.e., there
        // are at least 2*sizeof(uintptr_t) bytes of valid memory before it).
        const auto *ptr8  = reinterpret_cast<const uint8_t *>(ptr);
        const auto *pool8 = reinterpret_cast<const uint8_t *>(pool_);
        bool is_raw_start = (ptr8 >= pool8) &&
                            (ptr8 < pool8 + blockSize_ * blockCount_) &&
                            (static_cast<size_t>(ptr8 - pool8) % blockSize_ == 0);
        if (!is_raw_start) {
            // Aligned allocation: metadata stored 2 words before ptr (within raw block)
            uintptr_t *meta = reinterpret_cast<uintptr_t *>(ptr) - 2;
            if (meta[1] == MAGIC_ALIGNED) {
                void *raw = reinterpret_cast<void *>(meta[0]);
                meta[1] = 0;  // Clear magic to prevent double-free confusion
                *reinterpret_cast<void **>(raw) = freeList_;
                freeList_ = reinterpret_cast<void **>(raw);
                OSAL_LOGD("Deallocated aligned block, raw=%p.\n", raw);
            } else {
                OSAL_LOGE("Deallocate: invalid pointer %p (not a pool block, no MAGIC_ALIGNED).", ptr);
            }
            return;
        }
        // Plain allocation: ptr is the raw block start
        *reinterpret_cast<void **>(ptr) = freeList_;
        freeList_ = reinterpret_cast<void **>(ptr);
        OSAL_LOGD("Deallocated block at address: %p.", ptr);
    }

    void *reallocate(void *ptr, size_t newSize) override {
        if (newSize > blockSize_) {
            OSAL_LOGE("Reallocate failed: requested size %zu exceeds block size %zu.", newSize, blockSize_);
            return nullptr;
        }

        void *newPtr = allocate(newSize);
        if (newPtr && ptr) {
            std::memcpy(newPtr, ptr, std::min(newSize, blockSize_));  // 复制旧数据到新块
            deallocate(ptr);
        }

        return newPtr;
    }

    void *allocateAligned(size_t size, size_t alignment) override {
        if (alignment < sizeof(void *)) {
            alignment = sizeof(void *);
        }
        // Need: 2 * sizeof(uintptr_t) metadata + up to (alignment-1) padding + size
        size_t totalSize = 2 * sizeof(uintptr_t) + alignment - 1 + size;
        if (totalSize > blockSize_) {
            OSAL_LOGE("Aligned allocation failed: totalSize %zu exceeds blockSize %zu.\n", totalSize, blockSize_);
            return nullptr;
        }
        void *raw = allocate(totalSize);
        if (!raw) {
            OSAL_LOGE("Aligned allocation failed: no free blocks.\n");
            return nullptr;
        }
        // Reserve 2 * sizeof(uintptr_t) at the front for metadata, then align
        uintptr_t base = reinterpret_cast<uintptr_t>(raw) + 2 * sizeof(uintptr_t);
        uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);
        // Store original block pointer and magic just before the aligned address
        uintptr_t *meta = reinterpret_cast<uintptr_t *>(aligned) - 2;
        meta[0] = reinterpret_cast<uintptr_t>(raw);
        meta[1] = MAGIC_ALIGNED;
        OSAL_LOGD("Aligned allocation: raw=%p aligned=%p.\n", raw, reinterpret_cast<void *>(aligned));
        return reinterpret_cast<void *>(aligned);
    }

    [[nodiscard]] size_t getAllocatedSize() const override { return blockSize_; }

private:
    static constexpr uintptr_t MAGIC_ALIGNED = 0xA119ED00u;
    void *pool_;
    void **freeList_;
    size_t blockSize_;
    size_t blockCount_;
};

}  // namespace osal

#endif  // __OSAL_MEMORY_MANAGER_H__