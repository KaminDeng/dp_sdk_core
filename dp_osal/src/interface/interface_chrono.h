/**
 * @file interface_chrono.h
 * @brief 用 CRTP 转发单调 tick、绝对期限等待及时间表示转换。
 * @details TimePoint 是 32 位后端 tick，不统一等于毫秒；POSIX 当前为 1000 Hz，
 * CMSIS 使用内核 tick。Duration 以秒表示。日历转换仅按频率换算计数，
 * 没有把单调时钟 epoch 校准到真实 UTC，因此不能把格式化结果当作可靠墙钟。
 */
#ifndef DP_OSAL_INTERFACE_CHRONO_H_
#define DP_OSAL_INTERFACE_CHRONO_H_

#include <cstdint>
#include <ctime>
#include <string>

#include "dp_osal_compat.h"

#if DP_OSAL_ENABLE_CHRONO

namespace dp::osal {

/**
 * @brief 编译期转发到 Impl::do*，不引入虚函数或平台头依赖。
 * @tparam Impl 提供时钟操作并允许基类访问的具体后端。
 */
template <typename Impl>
class ChronoBase {
public:
    /**
     * @brief 可回绕的 32 位原生 tick 计数；单位由 ticksPerSecond 决定。
     */
    using TimePoint = uint32_t;
    /**
     * @brief 以秒为单位的浮点时间差。
     */
    using Duration = double;

    /** @brief 实际64位单调纳秒，零表示能力/时钟不可用；不由32位tick扩展。 */
    [[nodiscard]] static uint64_t steadyNs() noexcept { return Impl::doSteadyNs(); }
    /** @brief Unix墙钟微秒，可跳变；失败不改变输出。 */
    static bool wallTimeUs(uint64_t *value) noexcept { return Impl::doWallTimeUs(value); }
    /** @brief 当前线程CPU微秒；无可靠源时明确失败，不替换为墙钟。 */
    static bool threadCpuTimeUs(uint64_t *value) noexcept { return Impl::doThreadCpuTimeUs(value); }

    /**
     * @brief 获取当前后端 tick。
     * @return 从后端起点累计的 32 位值，长期运行会回绕。
     */
    [[nodiscard]] TimePoint now() const { return impl().doNow(); }
    /**
     * @brief 用有符号差判断 lhs 是否严格晚于 rhs，支持相邻回绕。
     * @param lhs 候选较晚的 tick。
     * @param rhs 比较基准 tick。
     * @return 差值解释为 int32_t 后大于零时为 true，相等为 false。
     * @pre 两个时刻的有效距离必须小于 2^31 tick，半圈及更远的先后关系不适用此判断。
     */
    [[nodiscard]] static constexpr bool tickAfter(const TimePoint lhs, const TimePoint rhs) {
        return static_cast<int32_t>(lhs - rhs) > 0;
    }
    /**
     * @brief 获取每秒 tick 数，换算期限时不能自行假定 1 tick=1 ms。
     * @return 后端时钟频率，单位 Hz。
     */
    [[nodiscard]] uint32_t ticksPerSecond() const { return impl().doTicksPerSecond(); }
    /**
     * @brief 当前线程等待至绝对 tick 期限，已到或已过的期限直接返回。
     * @param absoluteDeadline 与 now 使用同一时间域的期限，应满足半圈比较前提。
     * @note 调度延迟和后端等待结果不通过本 void 接口报告。
     */
    void sleepUntil(const TimePoint absoluteDeadline) const { impl().doSleepUntil(absoluteDeadline); }
    /**
     * @brief 将后端 tick 差换算成秒。
     * @param start 起始 tick。
     * @param end 结束 tick。
     * @return 后端计算的秒数；调用者应将间隔限制在可解释的回绕窗口内。
     */
    [[nodiscard]] Duration elapsed(const TimePoint &start, const TimePoint &end) const {
        return impl().doElapsed(start, end);
    }
    /**
     * @brief 按后端频率把 tick 换成整秒表示，丢失亚秒部分。
     * @param timePoint 原生 tick。
     * @return 计数起点以来的秒数，不代表已同步的 Unix 日历时间。
     */
    [[nodiscard]] std::time_t to_time_t(const TimePoint &timePoint) const { return impl().doToTimeT(timePoint); }
    /**
     * @brief 把秒数缩放成后端 tick。
     * @param time 要转换的秒数。
     * @return 收窄为 32 位的 tick，负值及超范围输入没有统一校验。
     */
    [[nodiscard]] TimePoint from_time_t(std::time_t time) const { return impl().doFromTimeT(time); }
    /**
     * @brief 将 tick 换成秒后交由后端格式化为日历样式字符串。
     * @param timePoint 待显示的 tick。
     * @return 后端生成的文本；当前实现依赖 localtime，不用于控制期限或真实 UTC 证明。
     */
    [[nodiscard]] std::string to_string(const TimePoint &timePoint) const { return impl().doToString(timePoint); }

protected:
    /**
     * @brief 受保护的非虚析构，只允许随具体后端对象销毁。
     */
    ~ChronoBase() = default;

private:
    /**
     * @brief 将基类静态转回具体后端；对象必须确实属于 Impl。
     * @return 当前具体后端的只读引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CHRONO */

#endif  // DP_OSAL_INTERFACE_CHRONO_H_
