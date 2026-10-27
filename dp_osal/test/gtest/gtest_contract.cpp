/** @file gtest_contract.cpp
 * @brief 正式时钟与线程任务契约回归。 */
#include <gtest/gtest.h>
#include <cstdio>
#include <type_traits>

#include "dp_osal_virtual.h"

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal_chrono.h"

namespace {
class MinimalChrono final : public dp::osal::IChrono {
public:
    TimePoint now() const override { return 0U; }
    Duration elapsed(const TimePoint&, const TimePoint&) const override { return 0.0; }
    std::time_t to_time_t(const TimePoint&) const override { return 0; }
    TimePoint from_time_t(std::time_t) const override { return 0U; }
    std::string to_string(const TimePoint&) const override { return {}; }
};
static_assert(!std::is_abstract_v<MinimalChrono>, "未实现的可选时钟能力提供明确失败");

// 独立Host入口先于GTest初始化调用；外层子进程驱动验证真正的SIGABRT。
int runUnsupportedChronoDefault() {
    MinimalChrono clock;
    std::fputs("OSAL_UNSUPPORTED_CHRONO_ENTERED\n", stderr);
    std::fflush(stderr);
    clock.sleepUntil(1U);
    return 86;  // 若错误地空返回，驱动必须失败。
}

TEST(OSALContract, UnsupportedChronoDefaults) {
    MinimalChrono clock;
    EXPECT_EQ(clock.ticksPerSecond(), 0U);
#if GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(clock.sleepUntil(1U), "");
#endif
}

TEST(OSALContract, RealChronoVirtualForwardsDeadlines) {
    auto& real = dp::osal::Chrono::getInstance();
    dp::osal::ChronoVirtual<dp::osal::Chrono> clock(real);
    ASSERT_GT(clock.ticksPerSecond(), 0U);
    const auto before = clock.now();
    clock.sleepUntil(before - 1U);
    EXPECT_LT(clock.elapsed(before, clock.now()), 0.1);
    const auto future = clock.now() + 5U;
    clock.sleepUntil(future);
    EXPECT_FALSE(dp::osal::Chrono::tickAfter(future, clock.now()));
    EXPECT_DOUBLE_EQ(clock.elapsed(0xfffffff0U, 0x10U), 32.0 / clock.ticksPerSecond());
}
}  // namespace
#endif

// CRTP 后端共用内部入口存储，支持函数指针与 callable。
#include "dp_osal_thread.h"
#include <functional>
#include <utility>
#include <atomic>
#include <thread>
namespace osal_thread_contract {
class TaskBackend : public dp::osal::ThreadBase<TaskBackend> {
    friend class dp::osal::ThreadBase<TaskBackend>;

private:
    dp::osal::ThreadResult doStart(const char*, dp::osal::detail::ThreadEntryStorage task, void* context, int, int,
                                   void*, const dp::osal::ThreadResources&) {
        if (!task.valid()) return {dp::osal::ThreadStatus::kInvalidArgument};
        task.run(context);
        return {dp::osal::ThreadStatus::kSuccess};
    }
};
void increment(void* context) { ++*static_cast<unsigned*>(context); }
TEST(OSALThreadContract, BackendReceivesOneTaskRepresentation) {
    TaskBackend backend;
    unsigned count = 0U;
    EXPECT_TRUE(backend.start("pointer", increment, &count).ok());
    EXPECT_TRUE(backend.start("lambda", [](void* context) { increment(context); }, &count).ok());
    EXPECT_TRUE(backend.start("capture", [&count](void*) { ++count; }, nullptr).ok());
    std::function<void(void*)> callable = increment;
    EXPECT_TRUE(backend.start("function", callable, &count).ok());
    EXPECT_EQ(count, 4U);
}
TEST(OSALThreadContract, RealThreadAcceptsFunctionsAndCaptures) {
    dp::osal::Thread thread;
    unsigned count = 0U;
    ASSERT_EQ(thread.start("pointer", increment, &count), 0);
    ASSERT_EQ(thread.join(), 0);
    ASSERT_EQ(thread.start("lambda", [](void* context) { increment(context); }, &count), 0);
    ASSERT_EQ(thread.join(), 0);
    ASSERT_EQ(thread.start("capture", [&count](void*) { ++count; }, nullptr), 0);
    ASSERT_EQ(thread.join(), 0);
    std::function<void(void*)> callable = increment;
    ASSERT_EQ(thread.start("function", callable, &count), 0);
    ASSERT_EQ(thread.join(), 0);
    EXPECT_EQ(count, 4U);
}
TEST(OSALThreadContract, RejectsDuplicateAndNullEntryWithoutReplacingRunningTask) {
    dp::osal::Thread thread;
    std::atomic<bool> entered{false}, release{false};
    auto wait = +[](void* context) {
        auto** flags = static_cast<std::atomic<bool>**>(context);
        flags[0]->store(true, std::memory_order_release);
        while (!flags[1]->load(std::memory_order_acquire)) std::this_thread::yield();
    };
    std::atomic<bool>* flags[]{&entered, &release};
    ASSERT_EQ(thread.start("running", wait, flags), 0);
    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    EXPECT_NE(thread.start("duplicate", wait, flags), 0);
    EXPECT_NE(thread.start("duplicate-callable", std::function<void(void*)>{}, nullptr), 0);
    release.store(true, std::memory_order_release);
    ASSERT_EQ(thread.join(), 0);
    EXPECT_NE(thread.start("null", nullptr, nullptr), 0);
    EXPECT_NE(thread.start("empty", std::function<void(void*)>{}, nullptr), 0);
}
}  // namespace osal_thread_contract
