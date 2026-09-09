
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
