#include <gtest/gtest.h>

#include <atomic>

#include "osal_spin_lock.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

TEST(TestOSALSpinLock, Lock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();

    // Multi-threaded test
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            spinlock.lock();
            OSALSystem::getInstance().sleep_ms(1000);  // Keep locked
            EXPECT_TRUE(spinlock.isLocked());
            spinlock.unlock();
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure thread1 has the lock

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(spinlock.tryLock());  // thread1 holds the lock, thread2 should not acquire it
            task2Executed = true;
        },
        nullptr, 0, 1024);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(task1Executed);
    EXPECT_TRUE(task2Executed);
#else
    GTEST_SKIP();
#endif
}

TEST(TestOSALSpinLock, TryLock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_TRUE(spinlock.tryLock());
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();

    // Multi-threaded test
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            spinlock.lock();
            OSALSystem::getInstance().sleep_ms(1000);  // Keep locked
            EXPECT_TRUE(spinlock.isLocked());
            spinlock.unlock();
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure thread1 has the lock

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(spinlock.tryLock());  // thread1 holds the lock, thread2 should not acquire it
            task2Executed = true;
        },
        nullptr, 0, 1024);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(task1Executed);
    EXPECT_TRUE(task2Executed);
#else
    GTEST_SKIP();
#endif
}

TEST(TestOSALSpinLock, LockFor) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_TRUE(spinlock.lockFor(500));
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();

    // Multi-threaded test
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            spinlock.lock();
            OSALSystem::getInstance().sleep_ms(1000);  // Keep locked
            EXPECT_TRUE(spinlock.isLocked());
            spinlock.unlock();
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure thread1 has the lock

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(spinlock.lockFor(500));  // thread1 holds the lock, thread2 should not acquire it
            task2Executed = true;
        },
        nullptr, 0, 1024);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(task1Executed);
    EXPECT_TRUE(task2Executed);
#else
    GTEST_SKIP();
#endif
}

TEST(TestOSALSpinLock, Unlock) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());

    // Multi-threaded test
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            spinlock.lock();
            OSALSystem::getInstance().sleep_ms(1000);  // Keep locked
            EXPECT_TRUE(spinlock.isLocked());
            spinlock.unlock();
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure thread1 has the lock

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(spinlock.tryLock());  // thread1 holds the lock, thread2 should not acquire it
            task2Executed = true;
        },
        nullptr, 0, 1024);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(task1Executed);
    EXPECT_TRUE(task2Executed);
#else
    GTEST_SKIP();
#endif
}

TEST(TestOSALSpinLock, IsLocked) {
#if (OSAL_TEST_SPINLOCK_ENABLED || OSAL_TEST_ALL)
    osal::OSALSpinLock spinlock;
    EXPECT_FALSE(spinlock.isLocked());
    spinlock.lock();
    EXPECT_TRUE(spinlock.isLocked());
    spinlock.unlock();
    EXPECT_FALSE(spinlock.isLocked());

    // Multi-threaded test
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            spinlock.lock();
            OSALSystem::getInstance().sleep_ms(1000);  // Keep locked
            EXPECT_TRUE(spinlock.isLocked());
            spinlock.unlock();
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure thread1 has the lock

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(spinlock.tryLock());  // thread1 holds the lock, thread2 should not acquire it
            task2Executed = true;
        },
        nullptr, 0, 1024);

    thread1.join();
    thread2.join();

    EXPECT_TRUE(task1Executed);
    EXPECT_TRUE(task2Executed);
#else
    GTEST_SKIP();
#endif
}