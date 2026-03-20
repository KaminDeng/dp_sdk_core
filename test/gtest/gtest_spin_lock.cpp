#include <gtest/gtest.h>

#include <atomic>

#include "osal_spin_lock.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

// Test: lock() acquires the lock; isLocked() reflects the state; unlock() releases it.
TEST(TestOSALSpinLock, Lock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_FALSE(spinlock.isLocked());
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}

// Test: tryLock() succeeds when unlocked and isLocked() reflects the acquired state.
TEST(TestOSALSpinLock, TryLock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_TRUE(spinlock.tryLock());
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}

// Test: lockFor() succeeds when the lock is available within the timeout.
TEST(TestOSALSpinLock, LockFor) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_TRUE(spinlock.lockFor(500));
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}

// Test: unlock() transitions isLocked() from true to false.
TEST(TestOSALSpinLock, Unlock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}

// Test: isLocked() accurately tracks the lock state before, during, and after locking.
TEST(TestOSALSpinLock, IsLocked) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_FALSE(spinlock.isLocked());
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}

// Test: cross-thread contention — thread1 holds the lock; thread2's tryLock()
// and lockFor() both fail; thread2's lockFor() times out before thread1 releases.
// Previously this identical block was copy-pasted into every single-method test above.
TEST(TestOSALSpinLock, ConcurrentContention) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    std::atomic<bool> thread1Done(false);
    std::atomic<bool> thread2Done(false);

    OSALThread thread1, thread2;

    thread1.start(
        "Holder",
        [&](void *) {
            spinlock.lock();
            EXPECT_TRUE(spinlock.isLocked());
            OSALSystem::getInstance().sleep_ms(1000);  // hold lock for 1 s
            spinlock.unlock();
            thread1Done = true;
        },
        nullptr, 0, 2048);

    OSALSystem::getInstance().sleep_ms(100);  // ensure thread1 holds the lock

    thread2.start(
        "Contender",
        [&](void *) {
            EXPECT_FALSE(spinlock.tryLock());    // must fail — holder has it
            EXPECT_FALSE(spinlock.lockFor(500)); // must timeout before holder releases
            thread2Done = true;
        },
        nullptr, 0, 2048);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(thread1Done.load());
    EXPECT_TRUE(thread2Done.load());
    EXPECT_FALSE(spinlock.isLocked());
#else
    GTEST_SKIP();
#endif
}
