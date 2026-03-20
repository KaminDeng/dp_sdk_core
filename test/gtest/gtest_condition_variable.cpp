#include <atomic>

#include "gtest/gtest.h"
#include "osal_condition_variable.h"
#include "osal_mutex.h"
#include "osal_system.h"
#include "osal_test_framework_config.h"
#include "osal_thread.h"

using namespace osal;

TEST(OSALConditionVariableTest, TestOSALConditionVariableWaitAndNotifyOne) {
#if (TestOSALConditionVariableWaitAndNotifyOneEnabled)
    osal::OSALMutex mutex;
    osal::OSALConditionVariable condVar;

    EXPECT_TRUE(mutex.lock());

    // Start a thread to notify the condition variable after a delay
    OSALThread notifyThread;
    notifyThread.start(
        "NotifyThread",
        [](void *arg) {
            OSALSystem::getInstance().sleep_ms(200);  // Simulate a delay
            static_cast<osal::OSALConditionVariable *>(arg)->notifyOne();
        },
        &condVar, 0, 1024);

    // Test waiting on the condition variable
    condVar.wait(mutex);
    EXPECT_TRUE(mutex.unlock());

    notifyThread.join();  // Ensure the notify thread completes
#else
    GTEST_SKIP();
#endif
}

TEST(OSALConditionVariableTest, TestOSALConditionVariableWaitForTimeout) {
#if (TestOSALConditionVariableWaitForTimeoutEnabled)
    osal::OSALMutex mutex;
    osal::OSALConditionVariable condVar;

    EXPECT_TRUE(mutex.lock());

    // Start a thread to notify the condition variable after a longer delay
    OSALThread timeoutThread;
    timeoutThread.start(
        "TimeoutThread",
        [](void *arg) {
            OSALSystem::getInstance().sleep_ms(1000);  // Simulate a delay longer than the timeout
            static_cast<osal::OSALConditionVariable *>(arg)->notifyOne();
        },
        &condVar, 0, 1024);

    // Should return false as the wait should time out before notify
    EXPECT_FALSE(condVar.waitFor(mutex, 500));
    EXPECT_TRUE(mutex.unlock());

    timeoutThread.join();  // Ensure the timeout thread completes
#else
    GTEST_SKIP();
#endif
}

TEST(OSALConditionVariableTest, TestOSALConditionVariableNotifyAll) {
#if (TestOSALConditionVariableNotifyAllEnabled)
    osal::OSALMutex mutex;
    osal::OSALConditionVariable condVar;
    std::atomic<int> taskExecutedCount(0);

    // Start multiple threads to wait on the condition variable
    auto worker = [&](void *arg) {
        (void)arg;
        mutex.lock();
        condVar.wait(mutex);
        mutex.unlock();
        taskExecutedCount++;
    };

    OSALThread thread1("test_thread1", worker, nullptr, 0, 1024);
    OSALThread thread2("test_thread2", worker, nullptr, 0, 1024);
    OSALThread thread3("test_thread3", worker, nullptr, 0, 1024);

    OSALSystem::getInstance().sleep_ms(100);  // Ensure all threads are waiting
    EXPECT_EQ(taskExecutedCount.load(), 0);
    {
        mutex.lock();
        condVar.notifyAll();
        mutex.unlock();
    }

    thread1.join();
    thread2.join();
    thread3.join();
    EXPECT_EQ(taskExecutedCount.load(), 3);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALConditionVariableTest, TestOSALConditionVariableWaitCount) {
#if (TestOSALConditionVariableWaitCountEnabled)
    OSALConditionVariable condVar;
    OSALMutex mutex;
    std::atomic<int> readyCount(0);

    auto workerTask = [&](void *) {
        mutex.lock();
        readyCount.fetch_add(1);
        condVar.wait(mutex);
        mutex.unlock();
    };

    OSALThread thread1, thread2;
    thread1.start("TestThread1", workerTask, nullptr, 0, 1024);
    thread2.start("TestThread2", workerTask, nullptr, 0, 1024);

    // Spin-wait until both threads enter wait (max 2s)
    for (int i = 0; i < 200 && readyCount.load() < 2; ++i) {
        OSALSystem::getInstance().sleep_ms(10);
    }
    EXPECT_EQ(condVar.getWaitCount(), 2);

    {
        mutex.lock();
        condVar.notifyAll();
        mutex.unlock();
    }

    thread1.join();
    thread2.join();
    EXPECT_EQ(condVar.getWaitCount(), 0);
#else
    GTEST_SKIP();
#endif
}