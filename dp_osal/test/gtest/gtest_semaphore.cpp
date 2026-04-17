#include <atomic>

#include "gtest/gtest.h"
#include "dp_osal_semaphore.h"
#include "dp_osal_system.h"
#include "dp_osal_thread.h"

using namespace dp::osal;

TEST(OSALSemaphoreTest, TestOSALSemaphoreInit) {
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
    semaphore.init(5);
    EXPECT_EQ(semaphore.getValue(), 5);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALSemaphoreTest, TestOSALSemaphoreWaitSignal) {
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
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
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
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
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
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
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
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

// Test: wait() blocks the calling thread until another thread calls signal()
TEST(OSALSemaphoreTest, TestOSALSemaphoreBlockingWait) {
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
    semaphore.init(0);
    std::atomic<bool> consumerDone(false);

    Thread producer, consumer;
    consumer.start(
        "Consumer",
        [&](void *) {
            semaphore.wait();  // must block until producer signals
            consumerDone = true;
        },
        nullptr, 0, 2048);

    System::getInstance().sleep_ms(100);
    EXPECT_FALSE(consumerDone.load());  // still blocking

    producer.start(
        "Producer", [&](void *) { semaphore.signal(); }, nullptr, 0, 2048);

    consumer.join();
    producer.join();
    EXPECT_TRUE(consumerDone.load());
#else
    GTEST_SKIP();
#endif
}

// Test: producer-consumer pattern — N signals allow N waits
TEST(OSALSemaphoreTest, TestOSALSemaphoreProducerConsumer) {
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
    semaphore.init(0);
    std::atomic<int> consumed(0);
    const int N = 5;

    Thread producer;
    producer.start(
        "Producer",
        [&](void *) {
            for (int i = 0; i < N; ++i) {
                System::getInstance().sleep_ms(20);
                semaphore.signal();
            }
        },
        nullptr, 0, 2048);

    Thread consumer;
    consumer.start(
        "Consumer",
        [&](void *) {
            for (int i = 0; i < N; ++i) {
                semaphore.wait();
                consumed.fetch_add(1);
            }
        },
        nullptr, 0, 2048);

    producer.join();
    consumer.join();
    EXPECT_EQ(consumed.load(), N);
    EXPECT_EQ(semaphore.getValue(), 0);
#else
    GTEST_SKIP();
#endif
}

// Regression: init() must reset the semaphore value correctly without
// destroying the underlying OS handle (which would leave any blocked thread
// with a dangling pointer).  This test verifies init() is idempotent in the
// single-threaded (no waiters) case that is always safe to call.
TEST(OSALSemaphoreTest, TestOSALSemaphoreReinit) {
#if (DP_OSAL_TEST_SEMAPHORE_ENABLED || DP_OSAL_TEST_ALL)
    dp::osal::Semaphore semaphore;
    semaphore.init(3);
    EXPECT_EQ(semaphore.getValue(), 3);

    // Re-init to a lower value — must drain and refill correctly
    semaphore.init(1);
    EXPECT_EQ(semaphore.getValue(), 1);

    // Verify the semaphore is still functional after re-init
    EXPECT_TRUE(semaphore.tryWait());
    EXPECT_EQ(semaphore.getValue(), 0);
    semaphore.signal();
    EXPECT_EQ(semaphore.getValue(), 1);
#else
    GTEST_SKIP();
#endif
}