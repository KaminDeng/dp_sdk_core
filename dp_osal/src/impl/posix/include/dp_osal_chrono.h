/**
 * @file dp_osal_chrono.h
 * @brief POSIX steady_clock 毫秒 tick 后端，绝对期限等待使用单调时钟。
 * @details 继承 ChronoBase 并提供私有 do* 实现。TimePoint 收窄为 32 位，必须考虑回绕；
 * 转换为 time_t 仅做频率缩放，没有单调起点到 UTC 的校准。to_string 的日历外观
 * 不代表时间已同步；localtime 使用宿主库共享状态，调用方还须考虑并发和返回失败。
 */
//
// Created by kamin.deng on 2024/8/22.
//

#ifndef OSAL_CHRONO_H
#define OSAL_CHRONO_H

#if DP_OSAL_ENABLE_CHRONO

#include <chrono>
#include <limits>
#include <time.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#include "interface_chrono.h"

namespace dp::osal {

/**
 * @brief 所选后端的时钟单例，通过 CRTP 提供统一访问。
 */
class Chrono : public ChronoBase<Chrono> {
    friend class ChronoBase<Chrono>;

public:
    /**
     * @brief 惰性构造并返回本后端唯一时钟对象。
     * @return 静态对象引用，调用者不负责释放。
     */
    static Chrono &getInstance() {
        static Chrono instance;
        return instance;
    }

private:
    static bool clockValue(clockid_t clock, uint64_t scale, uint64_t divisor, uint64_t *output) noexcept {
        if (output == nullptr) return false;
        timespec value{};
        if (::clock_gettime(clock, &value) != 0 || value.tv_sec < 0 || value.tv_nsec < 0 ||
            value.tv_nsec >= 1000000000L)
            return false;
        const auto seconds = static_cast<uint64_t>(value.tv_sec);
        const auto fraction = static_cast<uint64_t>(value.tv_nsec) / divisor;
        if (seconds > (UINT64_MAX - fraction) / scale) return false;
        *output = seconds * scale + fraction;
        return true;
    }
    static uint64_t doSteadyNs() noexcept {
        uint64_t value{};
        return clockValue(CLOCK_MONOTONIC, UINT64_C(1000000000), 1U, &value) ? value : 0U;
    }
    static bool doWallTimeUs(uint64_t *value) noexcept { return clockValue(CLOCK_REALTIME, 1000000U, 1000U, value); }
    static bool doThreadCpuTimeUs(uint64_t *value) noexcept {
#if defined(CLOCK_THREAD_CPUTIME_ID)
        return clockValue(CLOCK_THREAD_CPUTIME_ID, 1000000U, 1000U, value);
#else
        (void)value;
        return false;
#endif
    }
    /**
     * @brief 读取 steady_clock 自身 epoch 以来的毫秒并收窄为 uint32_t。
     * @return 可回绕的后端 tick，不是日历时间。
     */
    TimePoint doNow() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return static_cast<TimePoint>(duration.count());
    }

    /**
     * @brief 报告 POSIX 毫秒计数固定频率。
     * @return 1000 Hz。
     */
    uint32_t doTicksPerSecond() const { return 1000U; }

    /**
     * @brief 检查未来期限后，根据剩余毫秒构造 steady_clock 等待目标。
     * @param absoluteDeadline 与 doNow 同域的绝对 tick。
     * @note 已到/已过直接返回；调度可使实际恢复较晚，再次读取 now 构造目标也会引入少量时间差。
     */
    void doSleepUntil(const TimePoint absoluteDeadline) const {
        const TimePoint current = doNow();
        if (!tickAfter(absoluteDeadline, current)) {
            return;
        }

        const auto remaining = static_cast<uint32_t>(absoluteDeadline - current);
        std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(remaining));
    }

    /**
     * @brief 用无符号 32 位差计算间隔，再按频率换算为秒。
     * @param start 起始 tick。
     * @param end 结束 tick。
     * @return 秒数；只适用于调用者已约束的计数区间，无法恢复多个完整回绕。
     */
    Duration doElapsed(const TimePoint &start, const TimePoint &end) const {
        return static_cast<Duration>(end - start) / 1000.0;  // 毫秒转换为秒
    }

    /**
     * @brief 将 tick 除以每秒频率得到整秒计数。
     * @param timePoint 待转换的后端 tick。
     * @return 从后端起点算的整数秒，亚秒截断，不具有真实 UTC 绑定。
     */
    std::time_t doToTimeT(const TimePoint &timePoint) const {
        return static_cast<std::time_t>(timePoint / 1000);  // 毫秒转换为秒
    }

    /**
     * @brief 把输入秒数乘以频率并收窄为 32 位 tick。
     * @param time 秒数，本实现不主动检查负值或乘法范围。
     * @return 按本后端频率缩放的 tick；调用者限制可表示范围。
     */
    TimePoint doFromTimeT(std::time_t time) const {
        return static_cast<TimePoint>(time * 1000);  // 秒转换为毫秒
    }

    /**
     * @brief 把 tick 换成秒并交给 localtime 生成日历样式文本。
     * @param timePoint 待格式化 tick。
     * @return 年-月-日 时:分:秒文本，不应据此记录真实事件日期。
     * @note 当前实现未检查 localtime 返回空指针；这是原有调用前提，注释不新增容错。
     */
    std::string doToString(const TimePoint &timePoint) const {
        std::time_t time = doToTimeT(timePoint);
        std::tm *tm = std::localtime(&time);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    /**
     * @brief 私有默认析构，单例随静态对象生命周期释放。
     */
    ~Chrono() = default;

    /**
     * @brief 私有默认构造，不允许外部创建额外实例。
     */
    Chrono() = default;
};

}  // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CHRONO */

#endif  // OSAL_CHRONO_H
