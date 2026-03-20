#include "gtest/gtest.h"
#include "osal_semaphore.h"
#include <atomic>
#include "osal_system.h"
#include "osal_thread.h"

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

// Test: wait() blocks the calling thread until another thread calls signal()
TEST(OSALSemaphoreTest, TestOSALSemaphoreBlockingWait) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(0);
    std::atomic<bool> consumerDone(false);

    OSALThread producer, consumer;
    consumer.start(
        "Consumer",
        [&](void *) {
            semaphore.wait();  // must block until producer signals
            consumerDone = true;
        },
        nullptr, 0, 2048);

    OSALSystem::getInstance().sleep_ms(100);
    EXPECT_FALSE(consumerDone.load());  // still blocking

    producer.start(
        "Producer",
        [&](void *) { semaphore.signal(); },
        nullptr, 0, 2048);

    consumer.join();
    producer.join();
    EXPECT_TRUE(consumerDone.load());
#else
    GTEST_SKIP();
#endif
}

// Test: producer-consumer pattern — N signals allow N waits
TEST(OSALSemaphoreTest, TestOSALSemaphoreProducerConsumer) {
#if (OSAL_TEST_SEMAPHORE_ENABLED || OSAL_TEST_ALL)
    osal::OSALSemaphore semaphore;
    semaphore.init(0);
    std::atomic<int> consumed(0);
    const int N = 5;

    OSALThread producer;
    producer.start(
        "Producer",
        [&](void *) {
            for (int i = 0; i < N; ++i) {
                OSALSystem::getInstance().sleep_ms(20);
                semaphore.signal();
            }
        },
        nullptr, 0, 2048);

    OSALThread consumer;
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