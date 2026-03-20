//
// Created by kamin.deng on 2024/8/23.
//

#include <gtest/gtest.h>

#include <atomic>

#include "osal_chrono.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

TEST(OSALThreadTest, TestOSALThreadStart) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<int> taskExecuted(1);

    thread.start(
        "TestThread",
        [&](void *) {
            taskExecuted = 2;
            OSALSystem::getInstance().sleep_ms(10000);
            taskExecuted = 3;
        },
        nullptr, 0, 1024);

    auto timestamp_now = OSALChrono::getInstance().now();
    OSALSystem::getInstance().sleep_ms(100);
    EXPECT_EQ(taskExecuted, 2);
    thread.stop();
    auto interval = OSALChrono::getInstance().now() - timestamp_now;
    EXPECT_EQ(taskExecuted, 2);
    EXPECT_TRUE(interval > 50 && interval < 500);

    // 尝试再次启动同一个线程，应该失败
    // EXPECT_TRUE(0 != thread.start("TestThread", [&](void*) {}));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStop) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start("TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    thread.join();
    EXPECT_TRUE(taskExecuted);

    // 尝试再次启动同一个线程，应该失败
    // EXPECT_TRUE(0 != thread.start("TestThread", [&](void*) {}));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStopIdempotent) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start("TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    thread.join();
    EXPECT_TRUE(taskExecuted);

    // Second stop() and third stop() must be safe (no double-join)
    thread.stop();
    thread.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadJoin) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start("TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    thread.join();
    EXPECT_TRUE(taskExecuted);

    // 尝试再次join同一个线程，应该安全
    thread.join();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadDetach) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread",
        [&](void *) {
            OSALSystem::getInstance().sleep_ms(50);
            taskExecuted = true;
        },
        nullptr, 0, 1024);

    thread.detach();
    OSALSystem::getInstance().sleep_ms(100);
    EXPECT_TRUE(taskExecuted);

    // 尝试再次detach同一个线程，应该安全
    thread.detach();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadIsRunning) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread",
        [&](void *) {
            OSALSystem::getInstance().sleep_ms(100);
            taskExecuted = true;
        },
        nullptr, 0, 1024);

    EXPECT_TRUE(thread.isRunning());
    thread.join();
    EXPECT_FALSE(thread.isRunning());

    // 尝试在线程结束后检查状态
    EXPECT_FALSE(thread.isRunning());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadSetAndGetPriority) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start("TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    // 设置线程优先级
    int priority = 10;
    thread.setPriority(priority);

    // 获取线程优先级
    int retrievedPriority = thread.getPriority();

    EXPECT_EQ(priority, retrievedPriority);

    priority = 5;
    // 尝试在线程结束后设置和获取优先级
    thread.setPriority(priority);
    EXPECT_EQ(priority, thread.getPriority());

    thread.join();

    EXPECT_TRUE(taskExecuted);

    // priority = 2;
    // 尝试在线程结束后设置和获取优先级
    // thread.setPriority(priority);
    // EXPECT_EQ(priority, thread.getPriority());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadSuspendAndResume) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    std::atomic<bool> taskExecuted(false);

    std::function<void(void *)> taskFunction = [&](void *) {
        OSALSystem::getInstance().sleep_ms(100);
        taskExecuted = true;
    };

    thread.start("TestThread", taskFunction, nullptr, 0, 1024);

    // 暂停线程
    thread.suspend();
    OSALSystem::getInstance().sleep_ms(200);
    EXPECT_FALSE(taskExecuted.load());

    // 恢复线程
    thread.resume();
    thread.join();
    EXPECT_TRUE(taskExecuted.load());

    // 尝试在线程结束后暂停和恢复
    thread.suspend();
    thread.resume();
#else
    GTEST_SKIP();
#endif
}