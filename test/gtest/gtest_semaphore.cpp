#include "gtest/gtest.h"
#include "osal_semaphore.h"

using namespace osal;

TEST(OSALSemaphoreTest, TestOSALSemaphoreInit) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(5);
    EXPECT_EQ(semaphore.getValue(), 5);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALSemaphoreTest, TestOSALSemaphoreWaitSignal) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(1);
    semaphore.wait();
    EXPECT_EQ(semaphore.getValue(), 0);
    semaphore.signal();
    EXPECT_EQ(semaphore.getValue(), 1);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALSemaphoreTest, TestOSALSemaphoreTryWait) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(1);
    EXPECT_TRUE(semaphore.tryWait());
    EXPECT_FALSE(semaphore.tryWait());
    semaphore.signal();
    EXPECT_TRUE(semaphore.tryWait());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALSemaphoreTest, TestOSALSemaphoreTryWaitFor) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(1);
    EXPECT_TRUE(semaphore.tryWaitFor(500));
    EXPECT_FALSE(semaphore.tryWaitFor(500));
    semaphore.signal();
    EXPECT_TRUE(semaphore.tryWaitFor(500));
#else
    GTEST_SKIP();
#endif
}

TEST(OSALSemaphoreTest, TestOSALSemaphoreGetValue) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(3);
    EXPECT_EQ(semaphore.getValue(), 3);
    semaphore.wait();
    EXPECT_EQ(semaphore.getValue(), 2);
    semaphore.signal();
    EXPECT_EQ(semaphore.getValue(), 3);
#else
    GTEST_SKIP();
#endif
}