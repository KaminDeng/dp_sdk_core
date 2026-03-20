#include "gtest/gtest.h"
#include "osal_rwlock.h"

using namespace osal;

TEST(OSALRWLockTest, TestOSALRWLockReadLock) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    rwlock.readLock();
    EXPECT_TRUE(rwlock.tryReadLock());
    rwlock.readUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockTryReadLock) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    EXPECT_TRUE(rwlock.tryReadLock());
    rwlock.readUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockReadLockFor) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    EXPECT_TRUE(rwlock.readLockFor(500));
    rwlock.readUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockWriteLock) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    rwlock.writeLock();
    EXPECT_FALSE(rwlock.tryWriteLock());
    rwlock.writeUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockTryWriteLock) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    EXPECT_TRUE(rwlock.tryWriteLock());
    rwlock.writeUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockWriteLockFor) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALRWLock rwlock;
    EXPECT_TRUE(rwlock.writeLockFor(500));
    rwlock.writeUnlock();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockGetReadLockCount) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    OSALRWLock rwlock;
    EXPECT_EQ(rwlock.getReadLockCount(), 0u);
    rwlock.readLock();
    EXPECT_EQ(rwlock.getReadLockCount(), 1u);
    rwlock.readLock();
    EXPECT_EQ(rwlock.getReadLockCount(), 2u);
    rwlock.readUnlock();
    EXPECT_EQ(rwlock.getReadLockCount(), 1u);
    rwlock.readUnlock();
    EXPECT_EQ(rwlock.getReadLockCount(), 0u);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALRWLockTest, TestOSALRWLockIsWriteLocked) {
#if (OSAL_TEST_RWLOCK_ENABLED || OSAL_TEST_ALL)
    OSALRWLock rwlock;
    EXPECT_FALSE(rwlock.isWriteLocked());
    rwlock.writeLock();
    EXPECT_TRUE(rwlock.isWriteLocked());
    rwlock.writeUnlock();
    EXPECT_FALSE(rwlock.isWriteLocked());
#else
    GTEST_SKIP();
#endif
}