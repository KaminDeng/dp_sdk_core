/**
 * @file dp_osal_chrono.h
 * @brief CMSIS-RTOS2 内核 tick 时钟后端，按内核频率换算秒数。
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

#include <cstdio>  // for std::sprintf
#include <ctime>   // for std::tm and std::time_t

#include "interface_chrono.h"
#include "dp_osal_port.h"

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
    // CMSIS只提供32位tick，不能据此证明64位单调时间或真实纳秒精度。
    static uint64_t doSteadyNs() noexcept { return 0U; }
    static bool doWallTimeUs(uint64_t *value) noexcept {
        (void)value;
        return false;
    }
    static bool doThreadCpuTimeUs(uint64_t *value) noexcept {
        (void)value;
        return false;
    }
    /**
     * @brief 直接读取 CMSIS 内核 tick，不换成毫秒。
     * @return 可回绕的后端 tick，不是日历时间。
     */
    TimePoint doNow() const { return osKernelGetTickCount(); }

    /**
     * @brief 用无符号 32 位差计算间隔，再按频率换算为秒。
     * @param start 起始 tick。
     * @param end 结束 tick。
     * @return 秒数；只适用于调用者已约束的计数区间，无法恢复多个完整回绕。
     * @pre 内核 tick 频率必须有效且非零，本实现不额外检测。
     */
    Duration doElapsed(const TimePoint &start, const TimePoint &end) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<Duration>(end - start) / tickFreq;
    }

    /**
     * @brief 将 tick 除以每秒频率得到整秒计数。
     * @param timePoint 待转换的后端 tick。
     * @return 从后端起点算的整数秒，亚秒截断，不具有真实 UTC 绑定。
     */
    std::time_t doToTimeT(const TimePoint &timePoint) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<std::time_t>(timePoint / tickFreq);
    }

    /**
     * @brief 把输入秒数乘以频率并收窄为 32 位 tick。
     * @param time 秒数，本实现不主动检查负值或乘法范围。
     * @return 按本后端频率缩放的 tick；调用者限制可表示范围。
     */
    TimePoint doFromTimeT(std::time_t time) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<TimePoint>(time * tickFreq);
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

        char buffer[100];  // 假设你已经定义了这个缓冲区，并且大小足够
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                      tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        return std::string(buffer);
    }

    /**
     * @brief 直接读取内核声明的实际tick频率，避免浮点倒数舍入。
     * @return 内核频率，0仍表示来源无效。
     */
    uint32_t doTicksPerSecond() const { return osKernelGetTickFreq(); }

    /**
     * @brief 仅对半圈窗口内未来期限调用 osDelayUntil。
     * @param absoluteDeadline 与 doNow 同域的绝对 tick。
     * @note 已到/已过直接返回；调度可使实际恢复较晚，osDelayUntil 状态在此被丢弃，不经 void 接口报告。
     */
    void doSleepUntil(const TimePoint absoluteDeadline) const {
        if (!tickAfter(absoluteDeadline, doNow())) {
            return;
        }
        (void)osDelayUntil(absoluteDeadline);
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
