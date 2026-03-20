#include "gtest/gtest.h"
#include "osal_queue.h"

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