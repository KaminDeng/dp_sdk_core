/** @file gtest_contract_compat.cpp
 * @brief 旧时钟Mock的源码兼容及真实CRTP时钟适配回归。 */
#include <gtest/gtest.h>
#include <cstdio>
#include <type_traits>

#include "dp_osal_virtual.h"

#if DP_OSAL_ENABLE_CHRONO
#include "dp_osal_chrono.h"

namespace {
class OldContractChrono final : public dp::osal::IChrono {
public:
    TimePoint now() const override { return 0U; }
    Duration elapsed(const TimePoint &, const TimePoint &) const override { return 0.0; }
    std::time_t to_time_t(const TimePoint &) const override { return 0; }
    TimePoint from_time_t(std::time_t) const override { return 0U; }
    std::string to_string(const TimePoint &) const override { return {}; }
};
static_assert(!std::is_abstract_v<OldContractChrono>, "旧Mock不应因新增能力成为抽象类");

// 独立Host入口先于GTest初始化调用；外层子进程驱动验证真正的SIGABRT。
int runUnsupportedChronoDefault() {
    OldContractChrono clock;
    std::fputs("OSAL_UNSUPPORTED_CHRONO_ENTERED\n", stderr);
    std::fflush(stderr);
    clock.sleepUntil(1U);
    return 86; // 若错误地空返回，驱动必须失败。
}

TEST(OSALContractCompat, OldChronoDefaults) {
    OldContractChrono clock;
    EXPECT_EQ(clock.ticksPerSecond(), 0U);
#if GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(clock.sleepUntil(1U), "");
#endif
}

TEST(OSALContractCompat, RealChronoVirtualForwardsDeadlines) {
    auto &real = dp::osal::Chrono::getInstance();
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

// 任务4：普通旧入口与显式兼容接口的编译契约，独立命名空间避免单TU辅助类型冲突。
#include "dp_osal_thread.h"
#include <functional>
#include <utility>
#include <atomic>
#include <thread>
namespace osal_thread_contract {
template<class T, class = void> struct HasRaw : std::false_type {};
template<class T> struct HasRaw<T, std::void_t<decltype(std::declval<T&>().startRaw(
    "probe", static_cast<dp::osal::ThreadEntry>(nullptr), nullptr))>> : std::true_type {};
static_assert(HasRaw<dp::osal::Thread>::value == bool(DP_OSAL_ENABLE_LEGACY_COMPAT),
              "raw入口仅在统一兼容配置启用时可见");
class OldStartBackend : public dp::osal::ThreadBase<OldStartBackend> {
    friend class dp::osal::ThreadBase<OldStartBackend>;
private:
    int doStart(const char*, std::function<void(void*)> fn, void* context, int, int, void*) {
        fn(context); return 17;
    }
};
void increment(void* context) { ++*static_cast<unsigned*>(context); }
TEST(OSALThreadContract, OldBackendNeedsOnlyOriginalDoStart) {
    OldStartBackend backend; unsigned count = 0U;
    EXPECT_EQ(backend.start("pointer", increment, &count), 17);
    EXPECT_EQ(backend.start("lambda", [](void* context) { increment(context); }, &count), 17);
    EXPECT_EQ(backend.start("capture", [&count](void*) { ++count; }, nullptr), 17);
    std::function<void(void*)> callable = increment;
    EXPECT_EQ(backend.start("function", callable, &count), 17);
    EXPECT_EQ(count, 4U);
}
TEST(OSALThreadContract, RealOrdinaryStartAcceptsAllOriginalCallableForms) {
    dp::osal::Thread thread; unsigned count = 0U;
    ASSERT_EQ(thread.start("pointer", increment, &count), 0); thread.join();
    ASSERT_EQ(thread.start("lambda", [](void* context) { increment(context); }, &count), 0); thread.join();
    ASSERT_EQ(thread.start("capture", [&count](void*) { ++count; }, nullptr), 0); thread.join();
    std::function<void(void*)> callable = increment;
    ASSERT_EQ(thread.start("function", callable, &count), 0); thread.join();
    EXPECT_EQ(count, 4U);
}
} // namespace osal_thread_contract

#if DP_OSAL_ENABLE_LEGACY_COMPAT
namespace osal_raw_start_contract {
TEST(OSALThreadContract, RawKeepsDuplicateAndNullEntryResultsSeparateFromOrdinaryStart) {
    dp::osal::Thread thread;
    std::atomic<bool> entered{false}, release{false};
    auto wait = +[](void* context) {
        auto** flags = static_cast<std::atomic<bool>**>(context);
        flags[0]->store(true, std::memory_order_release);
        while (!flags[1]->load(std::memory_order_acquire)) std::this_thread::yield();
    };
    std::atomic<bool>* flags[]{&entered, &release};
    ASSERT_EQ(thread.startRaw("raw-running", wait, flags), 0);
    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    EXPECT_EQ(thread.startRaw("duplicate-raw", wait, flags), -1);
#if defined(DP_OSAL_BACKEND_POSIX)
    EXPECT_EQ(thread.start("duplicate-ordinary", std::function<void(void*)>{}, nullptr), 0);
#else
    EXPECT_EQ(thread.start("duplicate-ordinary", std::function<void(void*)>{}, nullptr), -1);
#endif
    release.store(true, std::memory_order_release); thread.join();
    EXPECT_EQ(thread.startRaw("null-raw", nullptr, nullptr), -1);
    EXPECT_EQ(thread.start("empty-ordinary", std::function<void(void*)>{}, nullptr), 0); thread.join();
}
} // namespace osal_raw_start_contract
#endif
