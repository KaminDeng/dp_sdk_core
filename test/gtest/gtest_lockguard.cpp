#include <atomic>

#include "gtest/gtest.h"
#include "osal_lockguard.h"
#include "osal_thread.h"

using namespace osal;

// Test: LockGuard acquires the mutex on construction (isLocked()==true inside scope)
// and releases it automatically on destruction (mutex usable again after scope exits).
// This combines the Constructor, IsLocked, and Destructor cases into one coherent RAII test.
TEST(OSALLockGuardTest, TestOSALLockGuardRAII) {
#if (OSAL_TEST_LOCKGUARD_ENABLED || OSAL_TEST_ALL)
    osal::OSALMutex mutex;
    {
        osal::OSALLockGuard lockGuard(mutex);
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
#if (OSAL_TEST_LOCKGUARD_ENABLED || OSAL_TEST_ALL)
    OSALThread thread;
    osal::OSALMutex mutex;
    std::atomic<bool> taskExecuted(false);

    auto workerTask = [&](void *) {
        osal::OSALLockGuard lockGuard(mutex);
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
