#include <atomic>

#include "gtest/gtest.h"
#include "osal_queue.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

TEST(OSALMessageQueueTest, TestOSALMessageQueueSendReceive) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    queue.send(42);
    int message = queue.receive();
    EXPECT_EQ(message, 42);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMessageQueueTest, TestOSALMessageQueueTryReceive) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    int message;
    EXPECT_FALSE(queue.tryReceive(message));
    queue.send(42);
    EXPECT_TRUE(queue.tryReceive(message));
    EXPECT_EQ(message, 42);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMessageQueueTest, TestOSALMessageQueueReceiveFor) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    int message;
    EXPECT_FALSE(queue.receiveFor(message, 100));
    queue.send(42);
    EXPECT_TRUE(queue.receiveFor(message, 100));
    EXPECT_EQ(message, 42);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMessageQueueTest, TestOSALMessageQueueSize) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    EXPECT_EQ(queue.size(), 0);
    queue.send(42);
    EXPECT_EQ(queue.size(), 1);
    queue.receive();
    EXPECT_EQ(queue.size(), 0);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALMessageQueueTest, TestOSALMessageQueueClear) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    queue.send(42);
    queue.send(43);
    EXPECT_EQ(queue.size(), 2);
    queue.clear();
    EXPECT_EQ(queue.size(), 0);
#else
    GTEST_SKIP();
#endif
}

// Test: messages are received in FIFO order
TEST(OSALMessageQueueTest, TestOSALMessageQueueFIFOOrder) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    queue.send(1);
    queue.send(2);
    queue.send(3);
    EXPECT_EQ(queue.receive(), 1);
    EXPECT_EQ(queue.receive(), 2);
    EXPECT_EQ(queue.receive(), 3);
#else
    GTEST_SKIP();
#endif
}

// Test: receive() blocks until a message is sent from another thread
TEST(OSALMessageQueueTest, TestOSALMessageQueueBlockingReceive) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    std::atomic<int> received(-1);

    OSALThread consumer, producer;
    consumer.start("Consumer", [&](void *) { received = queue.receive(); }, nullptr, 0, 2048);

    OSALSystem::getInstance().sleep_ms(100);
    EXPECT_EQ(received.load(), -1);  // still blocking

    producer.start("Producer", [&](void *) { queue.send(42); }, nullptr, 0, 2048);

    consumer.join();
    producer.join();
    EXPECT_EQ(received.load(), 42);
#else
    GTEST_SKIP();
#endif
}

// Test: multiple producers and consumers — total message count is preserved
TEST(OSALMessageQueueTest, TestOSALMessageQueueMultiProducerConsumer) {
#if (OSAL_TEST_QUEUE_ENABLED || OSAL_TEST_ALL)
    osal::OSALMessageQueue<int> queue;
    std::atomic<int> totalReceived(0);
    const int MSGS_PER_PRODUCER = 10;
    const int NUM_THREADS = 3;

    OSALThread producers[NUM_THREADS], consumers[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; ++t) {
        producers[t].start(
            "Producer",
            [&](void *) {
                for (int i = 0; i < MSGS_PER_PRODUCER; ++i) {
                    queue.send(i);
                }
            },
            nullptr, 0, 2048);
    }

    for (int t = 0; t < NUM_THREADS; ++t) {
        consumers[t].start(
            "Consumer",
            [&](void *) {
                for (int i = 0; i < MSGS_PER_PRODUCER; ++i) {
                    queue.receive();
                    totalReceived.fetch_add(1);
                }
            },
            nullptr, 0, 2048);
    }

    for (int t = 0; t < NUM_THREADS; ++t) {
        producers[t].join();
        consumers[t].join();
    }
    EXPECT_EQ(totalReceived.load(), NUM_THREADS * MSGS_PER_PRODUCER);
    EXPECT_EQ(queue.size(), 0u);
#else
    GTEST_SKIP();
#endif
}