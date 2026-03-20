#include <gtest/gtest.h>

#include <atomic>

#include "osal_mutex.h"
#include "osal_system.h"
#include "osal_thread.h"

// 使用命名空间以避免在每个调用中使用完整的命名空间路径
using namespace osal;

TEST(OSALMutexTest, TestOSALMutexLock) {
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    OSALMutex mutex;
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());

    // 尝试再次锁定和解锁
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMutexTest, TestOSALMutexUnlock) {
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    OSALMutex mutex;
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());

    // 尝试解锁未锁定的互斥锁（假设此操作是安全的）
    // EXPECT_FALSE(mutex.unlock());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMutexTest, TestOSALMutexTryLock) {
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    OSALMutex mutex;

    // 在同一个线程中测试递归锁的行为
    EXPECT_TRUE(mutex.tryLock());
    EXPECT_TRUE(mutex.tryLock());  // 递归锁应该允许同一个线程再次获取锁
    EXPECT_TRUE(mutex.unlock());
    EXPECT_TRUE(mutex.unlock());

    // 在不同线程中测试非递归锁的行为
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            EXPECT_TRUE(mutex.lock());
            OSALSystem::getInstance().sleep_ms(1000);  // 保持锁定状态
            EXPECT_TRUE(mutex.unlock());
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // 确保thread1已经获取锁

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
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    OSALMutex mutex;

    // 在同一个线程中测试递归锁的行为
    EXPECT_TRUE(mutex.tryLockFor(500));
    EXPECT_TRUE(mutex.tryLockFor(500));  // 递归锁应该允许同一个线程再次获取锁
    EXPECT_TRUE(mutex.unlock());
    EXPECT_TRUE(mutex.unlock());

    // 在不同线程中测试非递归锁的行为
    OSALThread thread1, thread2;
    std::atomic<bool> task1Executed(false);
    std::atomic<bool> task2Executed(false);

    thread1.start(
        "Thread1",
        [&](void *) {
            EXPECT_TRUE(mutex.lock());
            OSALSystem::getInstance().sleep_ms(1000);  // 保持锁定状态
            EXPECT_TRUE(mutex.unlock());
            task1Executed = true;
        },
        nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // 确保thread1已经获取锁

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
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    OSALMutex mutex;
    std::atomic<bool> taskExecuted(false);

    auto taskFunction = [&](void *) {
        mutex.lock();
        OSALSystem::getInstance().sleep_ms(500);
        taskExecuted = true;
        mutex.unlock();
    };

    OSALThread thread;
    thread.start("TestThread", taskFunction, nullptr, 0, 1024);

    // 尝试锁定互斥锁
    OSALSystem::getInstance().sleep_ms(100);
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