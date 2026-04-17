#include <atomic>

#include "gtest/gtest.h"
#include "dp_osal_lockguard.h"
#include "dp_osal_thread.h"

using namespace dp::osal;

// Test: LockGuard acquires the mutex on construction (isLocked()==true inside scope)
// and releases it automatically on destruction (mutex usable again after scope exits).
// This combines the Constructor, IsLocked, and Destructor cases into one coherent RAII test.
TEST(OSALLockGuardTest, TestOSALLockGuardRAII) {
#if (DP_OSAL_TEST_LOCKGUARD_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Mutex mutex;
    {
        dp::osal::LockGuard lockGuard(mutex);
        EXPECT_TRUE(lockGuard.isLocked());
    }  // lockGuard goes out of scope — must release the lock

    // Verify the mutex is unlocked: locking must succeed immediately.
    EXPECT_TRUE(mutex.lock());
    EXPECT_TRUE(mutex.unlock());
#else
    GTEST_SKIP();
#endif
}

// Test: LockGuard works correctly when acquired inside a worker thread.
TEST(OSALLockGuardTest, TestOSALLockGuardMultiThread) {
#if (DP_OSAL_TEST_LOCKGUARD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    dp::osal::Mutex mutex;
    std::atomic<bool> taskExecuted(false);

    auto workerTask = [&](void *) {
        dp::osal::LockGuard lockGuard(mutex);
        // Wrap EXPECT inside a lambda to avoid compiler warning about
        // returning non-void from a void lambda via the gtest macro.
        auto check = [&]() {
            EXPECT_TRUE(lockGuard.isLocked());
            return 0;
        };
        check();
        taskExecuted = true;
    };

    thread.start("TestThread", workerTask, nullptr, 0, 1024);
    thread.join();
    EXPECT_TRUE(taskExecuted.load());
#else
    GTEST_SKIP();
#endif
}
