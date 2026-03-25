//
// Created by kamin.deng on 2024/8/23.
//
#ifndef IMEM_POOL_H_
#define IMEM_POOL_H_

namespace osal {

class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;

    virtual bool initialize(size_t block_size, size_t block_count) = 0;

    virtual void *allocate(size_t size) = 0;

    virtual void deallocate(void *ptr) = 0;

    virtual void *reallocate(void *ptr, size_t newSize) = 0;           // 重新分配内存
    virtual void *allocateAligned(size_t size, size_t alignment) = 0;  // 对齐分配内存
    [[nodiscard]] virtual size_t getAllocatedSize() const = 0;         // 获取已分配内存的大小
};

}  // namespace osal

#endif  // IMEM_POOL_H_
