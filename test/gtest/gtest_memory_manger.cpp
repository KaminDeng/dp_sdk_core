#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"
#include "osal_memory_manager.h"

using namespace osal;

TEST(OSALMemoryManagerTest, TestOSALMemoryManagerAllocate) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    OSALMemoryManager mm(128, 10);
    void *ptr = mm.allocate(50);
    ASSERT_NE(ptr, nullptr);
    mm.deallocate(ptr);
    // After deallocate, pool should still work
    void *ptr2 = mm.allocate(50);
    EXPECT_NE(ptr2, nullptr);
    mm.deallocate(ptr2);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMemoryManagerTest, TestOSALMemoryManagerDeallocate) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    OSALMemoryManager mm(128, 10);
    // Exhaust all blocks
    void *ptrs[10];
    for (int i = 0; i < 10; ++i) {
        ptrs[i] = mm.allocate(10);
        ASSERT_NE(ptrs[i], nullptr);
    }
    // Pool should be full
    EXPECT_EQ(mm.allocate(10), nullptr);
    // Release all
    for (int i = 0; i < 10; ++i) {
        mm.deallocate(ptrs[i]);
    }
    // Pool should be usable again
    void *p = mm.allocate(10);
    EXPECT_NE(p, nullptr);
    mm.deallocate(p);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMemoryManagerTest, TestOSALMemoryManagerReallocate) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    OSALMemoryManager mm(128, 10);
    void *ptr = mm.allocate(50);
    ASSERT_NE(ptr, nullptr);
    std::memset(ptr, 0xAB, 50);
    void *newPtr = mm.reallocate(ptr, 50);
    ASSERT_NE(newPtr, nullptr);
    // Content should be preserved (copied min(50, 128) bytes)
    uint8_t *bytes = reinterpret_cast<uint8_t *>(newPtr);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(bytes[i], 0xAB) << "byte " << i << " mismatch after reallocate";
    }
    mm.deallocate(newPtr);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMemoryManagerTest, TestOSALMemoryManagerAllocateAligned) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    // Use block size large enough for metadata + alignment + data
    // 2*sizeof(uintptr_t)=16 + 63 padding + 30 data = 109 bytes minimum → use 256
    OSALMemoryManager mm(256, 10);
    void *ptr = mm.allocateAligned(30, 64);
    ASSERT_NE(ptr, nullptr);
    // Verify alignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0u);
    // Write to verify the address is writable
    std::memset(ptr, 0xCD, 30);
    mm.deallocate(ptr);
    // Pool health check after aligned dealloc (verifies C1 fix: free-list not corrupted)
    void *ptr2 = mm.allocateAligned(30, 64);
    EXPECT_NE(ptr2, nullptr);
    if (ptr2) mm.deallocate(ptr2);
    // Plain alloc should also work
    void *plain = mm.allocate(10);
    EXPECT_NE(plain, nullptr);
    if (plain) mm.deallocate(plain);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMemoryManagerTest, TestOSALMemoryManagerGetAllocatedSize) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    // getAllocatedSize() takes no arguments — returns blockSize_
    OSALMemoryManager mm(128, 10);
    EXPECT_EQ(mm.getAllocatedSize(), 128u);
#else
    GTEST_SKIP();
#endif
}

// Test: deallocate(nullptr) must not crash and pool remains usable
TEST(OSALMemoryManagerTest, TestOSALMemoryManagerDeallocateNull) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    OSALMemoryManager mm(128, 10);
    mm.deallocate(nullptr);  // must not crash or corrupt state
    void *p = mm.allocate(10);
    EXPECT_NE(p, nullptr);
    mm.deallocate(p);
#else
    GTEST_SKIP();
#endif
}

// Test: allocate(size > block_size) must return nullptr
TEST(OSALMemoryManagerTest, TestOSALMemoryManagerAllocateOversize) {
#if (OSAL_TEST_MEMORY_MANAGER_ENABLED || OSAL_TEST_ALL)
    OSALMemoryManager mm(64, 10);
    void *p = mm.allocate(65);  // one byte over block_size
    EXPECT_EQ(p, nullptr);
    // Normal allocation still works
    void *p2 = mm.allocate(64);
    EXPECT_NE(p2, nullptr);
    mm.deallocate(p2);
#else
    GTEST_SKIP();
#endif
}
