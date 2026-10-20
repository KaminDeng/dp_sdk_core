/**
 * @file interface_system.h
 * @brief 调度启动、线程休眠、系统 tick 与诊断快照的 CRTP 接口。
 * @details sleep_ms 明确使用毫秒，get_tick_ms 返回可回绕的毫秒计数。
 * 临界区能力因后端而异：当前 POSIX 为空实现，CMSIS 锁定调度器而非屏蔽全部中断；
 * 共享数据同步不能只根据接口名称推断中断安全或跨核互斥。
 */
#ifndef DP_OSAL_INTERFACE_SYSTEM_H_
#define DP_OSAL_INTERFACE_SYSTEM_H_

#include <cstddef>
#include <cstdint>

#include "dp_osal_thread_snapshot.h"
#include "dp_osal_measurement.h"

#ifndef DP_OSAL_ENABLE_THREAD_SNAPSHOT
#define DP_OSAL_ENABLE_THREAD_SNAPSHOT 0
#endif

namespace dp::osal {

/**
 * @brief 系统服务的编译期转发基类，不自行启动操作系统。
 * @tparam Impl 所选 POSIX/CMSIS 等系统后端。
 */
template <typename Impl>
class SystemBase {
public:
    /**
     * @brief 请求后端启动调度器；宿主已运行时可为空操作。
     * @note 是否返回及失败处置由后端决定，本 void 接口不提供启动状态。
     */
    void StartScheduler() { impl().doStartScheduler(); }
    /**
     * @brief 让调用线程按毫秒请求休眠。
     * @param milliseconds 相对休眠毫秒数，实际恢复时刻还受时钟粒度和调度影响。
     */
    void sleep_ms(uint32_t milliseconds) const { impl().doSleepMs(milliseconds); }
    /**
     * @brief 按整秒请求调用线程休眠。
     * @param seconds 相对秒数；后端缩放范围及停止唤醒策略由具体实现决定。
     */
    void sleep(uint32_t seconds) const { impl().doSleep(seconds); }
    /**
     * @brief 获取系统单调毫秒计数的 32 位表示。
     * @return 后端计数转换后的毫秒值，可回绕，不用于真实日期。
     * @note CMSIS 从原生 32 位 tick 换算，tick 回绕行为还受频率影响。
     */
    [[nodiscard]] uint32_t get_tick_ms() const { return impl().doGetTickMs(); }
    /**
     * @brief 进入后端定义的临界区，必须与 exit_critical 配对。
     * @note 此层不保存嵌套状态；当前 POSIX 无保护效果，CMSIS 不屏蔽全部中断。
     */
    void enter_critical() const { impl().doEnterCritical(); }
    /**
     * @brief 退出后端临界区，恢复语义及嵌套要求由后端约定。
     */
    void exit_critical() const { impl().doExitCritical(); }
    /**
     * @brief 查询后端说明文本。
     * @return 借用字符串指针，调用者不释放；当前后端为静态字符串。
     */
    [[nodiscard]] const char *get_system_info() const { return impl().doGetSystemInfo(); }
    /**
     * @brief 读取全局同步测量收集器快照。
     * @return 按字段采集的统计对象，包含有效性/溢出标记，不是所有字段同时锁存的事务快照。
     */
    [[nodiscard]] SynchronizationMeasurement get_synchronization_measurement() const {
        return SynchronizationMeasurementCollector::snapshot();
    }
#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    /**
     * @brief 在启用线程快照时，将后端诊断写入调用者有界数组。
     * @param[out] buf 至少可容纳 max 项的输出区。
     * @param max 输出项数上限。
     * @return 实际写入条目数；每项有效标记及时间域须按后端检查，非运动授权依据。
     */
    size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const { return impl().doGetThreadSnapshot(buf, max); }
#endif

protected:
    /**
     * @brief 受保护非虚析构，只通过具体系统后端销毁。
     */
    ~SystemBase() = default;

private:
    /**
     * @brief 获取具体系统后端的可写引用。
     * @return 当前 Impl 对象。
     */
    Impl &impl() { return *static_cast<Impl *>(this); }
    /**
     * @brief 获取具体系统后端的只读引用。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_SYSTEM_H_
