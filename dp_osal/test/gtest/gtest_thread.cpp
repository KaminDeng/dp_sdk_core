//
// Created by kamin.deng on 2024/8/23.
//

#include <gtest/gtest.h>

#include <atomic>

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal_chrono.h"
#endif
#include "dp_osal_system.h"
#include "dp_osal_thread.h"

using namespace dp::osal;

TEST(OSALThreadTest, TestOSALThreadStart) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<int> taskExecuted(1);

    thread.start(
        "TestThread",
        [&](void *) {
            taskExecuted = 2;
            System::getInstance().sleep_ms(10000);
            taskExecuted = 3;
        },
        nullptr, 0, 1024);

    auto timestamp_now = System::getInstance().get_tick_ms();
    System::getInstance().sleep_ms(100);
    EXPECT_EQ(taskExecuted, 2);
    thread.stop();
    auto interval = System::getInstance().get_tick_ms() - timestamp_now;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    /* Exception build: sleep_ms throws, task function aborted at sleep site.
     * taskExecuted stays 2 — the line after sleep_ms never runs. */
    EXPECT_EQ(taskExecuted, 2);
#else
    /* Bare-metal (-fno-exceptions): sleep_ms returns early, but the task
     * function continues executing (taskExecuted becomes 3).  Only the
     * stop timing (interval < 500ms) is validated here. */
    EXPECT_TRUE(taskExecuted == 2 || taskExecuted == 3);
#endif
    EXPECT_TRUE(interval > 50 && interval < 500);

    // 尝试再次启动同一个线程，应该失败
    // EXPECT_TRUE(0 != thread.start("TestThread", [&](void*) {}));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStop) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    thread.join();
    EXPECT_TRUE(taskExecuted);

    // 尝试再次启动同一个线程，应该失败
    // EXPECT_TRUE(0 != thread.start("TestThread", [&](void*) {}));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStopIdempotent) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

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
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

    thread.join();
    EXPECT_TRUE(taskExecuted);

    // 尝试再次join同一个线程，应该安全
    thread.join();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadDetach) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread",
        [&](void *) {
            System::getInstance().sleep_ms(50);
            taskExecuted = true;
        },
        nullptr, 0, 1024);

    thread.detach();
    System::getInstance().sleep_ms(100);
    EXPECT_TRUE(taskExecuted);

    // 尝试再次detach同一个线程，应该安全
    thread.detach();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadIsRunning) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread",
        [&](void *) {
            System::getInstance().sleep_ms(100);
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
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    thread.start(
        "TestThread", [&](void *) { taskExecuted = true; }, nullptr, 0, 1024);

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
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> taskExecuted(false);

    std::function<void(void *)> taskFunction = [&](void *) {
        System::getInstance().sleep_ms(100);
        taskExecuted = true;
    };

    thread.start("TestThread", taskFunction, nullptr, 0, 1024);

    // 暂停线程
    thread.suspend();
    System::getInstance().sleep_ms(200);
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