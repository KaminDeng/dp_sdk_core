#include <atomic>

#include "gtest/gtest.h"
#if DP_OSAL_ENABLE_CONDITION_VAR
#include "dp_osal_condition_variable.h"
#endif
#include "dp_osal_mutex.h"
#include "dp_osal_system.h"
#include "dp_osal_thread.h"

using namespace dp::osal;

#if !DP_OSAL_ENABLE_CONDITION_VAR
/* Entire test file is disabled when DP_OSAL_ENABLE_CONDITION_VAR=0. */
#else

TEST(OSALConditionVariableTest, TestOSALConditionVariableWaitAndNotifyOne) {
#if (DP_OSAL_TEST_CONDITION_VARIABLE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Mutex mutex;
    dp::osal::ConditionVariable condVar;

    EXPECT_TRUE(mutex.lock());

    // Start a thread to notify the condition variable after a delay
    Thread notifyThread;
    notifyThread.start(
        "NotifyThread",
        [](void *arg) {
            System::getInstance().sleep_ms(200);  // Simulate a delay
            static_cast<dp::osal::ConditionVariable *>(arg)->notifyOne();
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
#if (DP_OSAL_TEST_CONDITION_VARIABLE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Mutex mutex;
    dp::osal::ConditionVariable condVar;

    EXPECT_TRUE(mutex.lock());

    // Start a thread to notify the condition variable after a longer delay
    Thread timeoutThread;
    timeoutThread.start(
        "TimeoutThread",
        [](void *arg) {
            System::getInstance().sleep_ms(1000);  // Simulate a delay longer than the timeout
            static_cast<dp::osal::ConditionVariable *>(arg)->notifyOne();
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
#if (DP_OSAL_TEST_CONDITION_VARIABLE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Mutex mutex;
    dp::osal::ConditionVariable condVar;
    std::atomic<int> taskExecutedCount(0);

    // Start multiple threads to wait on the condition variable
    auto worker = [&](void *arg) {
        (void)arg;
        mutex.lock();
        condVar.wait(mutex);
        mutex.unlock();
        taskExecutedCount++;
    };

    Thread thread1("test_thread1", worker, nullptr, 0, 1024);
    Thread thread2("test_thread2", worker, nullptr, 0, 1024);
    Thread thread3("test_thread3", worker, nullptr, 0, 1024);

    System::getInstance().sleep_ms(100);  // Ensure all threads are waiting
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
#if (DP_OSAL_TEST_CONDITION_VARIABLE_ENABLED || DP_OSAL_TEST_ALL)
    ConditionVariable condVar;
    Mutex mutex;

    auto workerTask = [&](void *) {
        mutex.lock();
        condVar.wait(mutex);
        mutex.unlock();
    };

    Thread thread1, thread2;
    thread1.start("TestThread1", workerTask, nullptr, 0, 1024);
    thread2.start("TestThread2", workerTask, nullptr, 0, 1024);

    // Spin until both threads are actually inside condVar.wait()
    for (int i = 0; i < 200 && condVar.getWaitCount() < 2; ++i) {
        System::getInstance().sleep_ms(10);
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

// Regression: waitCount must be zero after waitFor() times out, so a
// subsequent notifyAll() does not over-signal and corrupt the remaining
// waiters.  In the CMSIS-OS2 backend this required moving waitCount--
// to AFTER mutex.lock() (not before it).
TEST(OSALConditionVariableTest, TestOSALConditionVariableTimeoutDoesNotCorruptWaitCount) {
#if (DP_OSAL_TEST_CONDITION_VARIABLE_ENABLED || DP_OSAL_TEST_ALL)
    ConditionVariable condVar;
    Mutex mutex;
    std::atomic<int> woken(0);

    // Thread A: infinite waiter — must be woken by notifyAll()
    Thread infiniteWaiter;
    infiniteWaiter.start(
        "InfiniteWaiter",
        [&](void *) {
            mutex.lock();
            condVar.wait(mutex);  // waits until notified
            mutex.unlock();
            woken.fetch_add(1);
        },
        nullptr, 0, 2048);

    // Thread B: timed waiter — times out after 200ms
    Thread timedWaiter;
    timedWaiter.start(
        "TimedWaiter",
        [&](void *) {
            mutex.lock();
            condVar.waitFor(mutex, 200);  // always times out here
            mutex.unlock();
        },
        nullptr, 0, 2048);

    // Wait for both threads to enter condVar
    for (int i = 0; i < 100 && condVar.getWaitCount() < 2; ++i) {
        System::getInstance().sleep_ms(10);
    }
    EXPECT_EQ(condVar.getWaitCount(), 2);

    timedWaiter.join();  // 200ms timeout expires, timedWaiter exits

    // After timeout, waitCount must have returned to 1 (only infiniteWaiter left)
    EXPECT_EQ(condVar.getWaitCount(), 1);

    // notifyAll() should wake exactly the remaining 1 waiter
    {
        mutex.lock();
        condVar.notifyAll();
        mutex.unlock();
    }

    infiniteWaiter.join();
    EXPECT_EQ(woken.load(), 1);
    EXPECT_EQ(condVar.getWaitCount(), 0);
#else
    GTEST_SKIP();
#endif
}
#endif /* DP_OSAL_ENABLE_CONDITION_VAR */
