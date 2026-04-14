#include <atomic>

#include "gtest/gtest.h"
#if OSAL_ENABLE_CHRONO
#include "osal_chrono.h"
#endif
#include "osal_system.h"
#if OSAL_ENABLE_THREAD_POOL
#include "osal_thread_pool.h"
#endif

using namespace osal;

#if !OSAL_ENABLE_THREAD_POOL
/* Entire test file is disabled when OSAL_ENABLE_THREAD_POOL=0. */
#else

TEST(OSALThreadPoolTests, TestOSALThreadPoolStartStop) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);
    ASSERT_TRUE(threadPool.isStarted());
    auto task = [](void *arg) {
        (void)arg;
        OSALSystem::getInstance().sleep_ms(10000);  // Simulate task execution time
    };
    threadPool.submit(task, nullptr, 0);
    auto timestamp_now = OSALChrono::getInstance().now();
    OSALSystem::getInstance().sleep_ms(100);
    threadPool.stop();
    auto interval = OSALChrono::getInstance().now() - timestamp_now;
    ASSERT_TRUE(interval > 50 && interval < 500);
    ASSERT_FALSE(threadPool.isStarted());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolSuspendResume) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);
    ASSERT_TRUE(threadPool.isStarted());
    ASSERT_EQ(threadPool.suspend(), 0);
    ASSERT_TRUE(threadPool.isSuspended());
    ASSERT_EQ(threadPool.resume(), 0);
    ASSERT_FALSE(threadPool.isSuspended());
    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

auto g_submit_task = [](void *arg) {
    bool *executed = static_cast<bool *>(arg);
    *executed = true;
};

TEST(OSALThreadPoolTests, TestOSALThreadPoolSubmitTask) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);

    bool taskExecuted = false;
    threadPool.submit(g_submit_task, &taskExecuted, 0);
    OSALSystem::getInstance().sleep_ms(500);  // Wait for task execution
    ASSERT_TRUE(taskExecuted);

    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolSetPriority) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.setPriority(5);
    ASSERT_EQ(threadPool.getPriority(), 5);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolGetTaskQueueSize) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);

    auto task = [](void *arg) {
        (void)arg;
        OSALSystem::getInstance().sleep_ms(1000);  // Simulate task execution time
    };

    threadPool.suspend();
    threadPool.submit(task, nullptr, 0);
    ASSERT_EQ(threadPool.getTaskQueueSize(), 1);

    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolGetActiveThreadCount) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);
    ASSERT_EQ(threadPool.getActiveThreadCount(), 0);

    bool taskExecuted = false;
    auto task = [](void *arg) {
        (void)arg;
        OSALSystem::getInstance().sleep_ms(500);
    };
    for (int i = 0; i < 4; i++) {
        threadPool.submit(task, &taskExecuted, 0);
    }
    OSALSystem::getInstance().sleep_ms(200);
    ASSERT_EQ(threadPool.getActiveThreadCount(), 4);
    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolCancelTask) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);

    bool taskExecuted = false;
    std::function<void(void *)> task = [](void *arg) {
        bool *executed = static_cast<bool *>(arg);
        *executed = true;
    };

    /* Submit a task and let it execute, then try to cancel it by ID.
     * The task will have already completed, so cancelTask() must return false. */
    uint32_t taskId1 = threadPool.submit(task, &taskExecuted, 0);
    OSALSystem::getInstance().sleep_ms(500);  // Wait for task execution
    ASSERT_FALSE(threadPool.cancelTask(taskId1));
    ASSERT_TRUE(taskExecuted);

    /* Suspend the pool so the submitted task stays queued, then cancel it.
     * cancelTask() must return true and the task must not have executed. */
    taskExecuted = false;
    threadPool.suspend();
    uint32_t taskId2 = threadPool.submit(task, &taskExecuted, 0);
    ASSERT_TRUE(threadPool.cancelTask(taskId2));
    ASSERT_FALSE(taskExecuted);

    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolSetTaskFailureCallback) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(4, 0, 1024);

    bool callbackCalled = false;
    auto callback = [](void *arg) {
        bool *called = static_cast<bool *>(arg);
        *called = true;
    };

    threadPool.setTaskFailureCallback(callback);
    threadPool.submit(nullptr, &callbackCalled, 0);  // Submit an invalid task to trigger callback
    OSALSystem::getInstance().sleep_ms(500);         // Wait for callback execution
    ASSERT_TRUE(callbackCalled);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolSetMaxThreads) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.start(2, 0, 1024);
    ASSERT_EQ(threadPool.getMinThreads(), 2);

    threadPool.setMaxThreads(4);
    ASSERT_EQ(threadPool.getMaxThreads(), 4);

    bool taskExecuted = false;
    auto task = [](void *arg) {
        (void)arg;
        OSALSystem::getInstance().sleep_ms(600);
    };
    for (int i = 0; i < 4; i++) {
        threadPool.submit(task, &taskExecuted, 0);
        OSALSystem::getInstance().sleep_ms(100);
    }
    ASSERT_EQ(threadPool.getActiveThreadCount(), 4);
    threadPool.stop();
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadPoolTests, TestOSALThreadPoolSetMinThreads) {
#if (OSAL_TEST_THREAD_POOL_ENABLED || OSAL_TEST_ALL)
    osal::OSALThreadPool threadPool;
    threadPool.setMinThreads(2);
    ASSERT_EQ(threadPool.getMinThreads(), 2);
#else
    GTEST_SKIP();
#endif
}
#endif /* OSAL_ENABLE_THREAD_POOL */
