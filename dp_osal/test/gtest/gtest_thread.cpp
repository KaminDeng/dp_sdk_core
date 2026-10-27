/** @file gtest_thread.cpp
 * @brief 所有后端共用的显式线程生命周期测试，不假设停止异常或强制挂起。
 */
#include <gtest/gtest.h>
#include <atomic>
#include "dp_osal_system.h"
#include "dp_osal_thread.h"
using namespace dp::osal;

TEST(OSALThreadTest, TestOSALThreadStart) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<int> stage{0};
    ASSERT_TRUE(thread
                    .start("cooperative",
                           [&](void*) {
                               stage.store(1);
                               while (!thread.stopRequested()) System::getInstance().sleep_ms(10000);
                               // 停止不是异常：业务退出收尾始终执行。
                               stage.store(2);
                           })
                    .ok());
    const auto deadline = System::getInstance().get_tick_ms() + 2000U;
    while (stage.load() == 0 && System::getInstance().get_tick_ms() < deadline) System::getInstance().sleep_ms(1);
    EXPECT_EQ(stage.load(), 1);
    const auto before = System::getInstance().get_tick_ms();
    EXPECT_TRUE(thread.requestStop());
    EXPECT_FALSE(thread.requestStop());
    EXPECT_TRUE(thread.join().ok());
    EXPECT_EQ(stage.load(), 2);
    EXPECT_LT(System::getInstance().get_tick_ms() - before, 1000U);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStop) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> executed{false};
    ASSERT_TRUE(thread.start("single", [&](void*) { executed = true; }).ok());
    EXPECT_TRUE(thread.join().ok());
    EXPECT_TRUE(executed.load());
    EXPECT_FALSE(thread.joinable());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadStopIdempotent) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    ASSERT_TRUE(thread.start("single", [](void*) {}).ok());
    EXPECT_TRUE(thread.stop().ok());
    EXPECT_EQ(thread.stop().status, ThreadStatus::kInvalidState);
    EXPECT_FALSE(thread.requestStop());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadJoin) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    ASSERT_TRUE(thread.start("single", [](void*) {}).ok());
    EXPECT_TRUE(thread.join().ok());
    EXPECT_EQ(thread.join().status, ThreadStatus::kInvalidState);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadDetach) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    ASSERT_TRUE(thread.start("single", [](void*) {}).ok());
    EXPECT_EQ(thread.detach().status, ThreadStatus::kNotSupported);
    EXPECT_TRUE(thread.joinable());
    EXPECT_TRUE(thread.join().ok());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadIsRunning) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    std::atomic<bool> release{false};
    ASSERT_TRUE(thread
                    .start("held",
                           [&](void*) {
                               while (!release.load()) System::getInstance().sleep_ms(1);
                           })
                    .ok());
    EXPECT_TRUE(thread.isRunning());
    EXPECT_TRUE(thread.joinable());
    release = true;
    EXPECT_TRUE(thread.join().ok());
    EXPECT_FALSE(thread.isRunning());
    EXPECT_FALSE(thread.joinable());
#else
    GTEST_SKIP();
#endif
}

TEST(OSALThreadTest, TestOSALThreadSetAndGetPriority) {
#if (DP_OSAL_TEST_THREAD_ENABLED || DP_OSAL_TEST_ALL)
    Thread thread;
    ASSERT_TRUE(thread.start("single", [](void*) {}).ok());
    thread.setPriority(10);
    EXPECT_EQ(thread.getPriority(), 10);
    thread.setPriority(5);
    EXPECT_EQ(thread.getPriority(), 5);
    EXPECT_TRUE(thread.join().ok());
#else
    GTEST_SKIP();
#endif
}
