#include <gtest/gtest.h>

#include <atomic>

#include "dp_osal_mutex.h"
#include "dp_osal_system.h"
#include "dp_osal_thread.h"

// 使用命名空间以避免在每个调用中使用完整的命名空间路径
using namespace dp::osal;

// Test: lock() and unlock() both return true on success, and can be called repeatedly.
TEST(OSALMutexTest, TestOSALMutexLockUnlock) {
#if (DP_OSAL_TEST_MUTEX_ENABLED || DP_OSAL_TEST_ALL)
    Mutex mutex;
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());
    // Second round: verify mutex is reusable after unlock.
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMutexTest, TestOSALMutexTryLock) {
#if (DP_OSAL_TEST_MUTEX_ENABLED || DP_OSAL_TEST_ALL)
    Mutex mutex;

    // 在同一个线程中测试递归锁的行为
    EXPECT_TRUE(mutex.tryLock());
    EXPECT_TRUE(mutex.tryLock());  // 递归锁应该允许同一个线程再次获取锁
    EXPECT_TRUE(mutex.unlock());
    EXPECT_TRUE(mutex.unlock());

    // 在不同线程中测试非递归锁的行为
    Thread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            EXPECT_TRUE(mutex.lock());
            System::getInstance().sleep_ms(1000);  // 保持锁定状态
            EXPECT_TRUE(mutex.unlock());
            task1Executed = true;
        },
        nullptr, 0, 1024);

    System::getInstance().sleep_ms(100);  // 确保thread1已经获取锁

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(mutex.tryLock());  // thread1持有锁，thread2应该无法获取锁
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

TEST(OSALMutexTest, TestOSALMutexTryLockFor) {
#if (DP_OSAL_TEST_MUTEX_ENABLED || DP_OSAL_TEST_ALL)
    Mutex mutex;

    // 在同一个线程中测试递归锁的行为
    EXPECT_TRUE(mutex.tryLockFor(500));
    EXPECT_TRUE(mutex.tryLockFor(500));  // 递归锁应该允许同一个线程再次获取锁
    EXPECT_TRUE(mutex.unlock());
    EXPECT_TRUE(mutex.unlock());

    // 在不同线程中测试非递归锁的行为
    Thread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            EXPECT_TRUE(mutex.lock());
            System::getInstance().sleep_ms(1000);  // 保持锁定状态
            EXPECT_TRUE(mutex.unlock());
            task1Executed = true;
        },
        nullptr, 0, 1024);

    System::getInstance().sleep_ms(100);  // 确保thread1已经获取锁

    thread2.start(
        "Thread2",
        [&](void *) {
            EXPECT_FALSE(mutex.tryLockFor(500));  // thread1持有锁，thread2应该无法获取锁
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

TEST(OSALMutexTest, TestOSALMutexLockAndUnlock) {
#if (DP_OSAL_TEST_MUTEX_ENABLED || DP_OSAL_TEST_ALL)
    Mutex mutex;
    std::atomic<bool> taskExecuted(false);

    auto taskFunction = [&](void *) {
        mutex.lock();
        System::getInstance().sleep_ms(500);
        taskExecuted = true;
        mutex.unlock();
    };

    Thread thread;
    thread.start("TestThread", taskFunction, nullptr, 0, 1024);

    // 尝试锁定互斥锁
    System::getInstance().sleep_ms(100);
    EXPECT_FALSE(mutex.tryLock());

    // 等待线程执行完毕
    thread.join();

    // 检查任务是否执行
    EXPECT_TRUE(taskExecuted.load());

    // 尝试再次锁定互斥锁
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
#else
    GTEST_SKIP();
#endif
}

// Test: mutex protects a shared non-atomic counter from concurrent increment races
TEST(OSALMutexTest, TestOSALMutexConcurrentCounter) {
#if (DP_OSAL_TEST_MUTEX_ENABLED || DP_OSAL_TEST_ALL)
    Mutex mutex;
    int counter = 0;  // intentionally non-atomic — protected only by mutex
    const int INCREMENTS = 500;
    const int NUM_THREADS = 4;

    Thread threads[NUM_THREADS];
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads[t].start(
            "Worker",
            [&](void *) {
                for (int i = 0; i < INCREMENTS; ++i) {
                    mutex.lock();
                    ++counter;
                    mutex.unlock();
                }
            },
            nullptr, 0, 4096);
    }

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads[t].join();
    }
    EXPECT_EQ(counter, NUM_THREADS * INCREMENTS);
#else
    GTEST_SKIP();
#endif
}