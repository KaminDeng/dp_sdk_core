/**
 * @file dp_osal_thread_snapshot.h
 * @brief shell/监测使用的便携线程快照结构、时间域及计数差辅助函数。
 * @details 字段以独立有效标记描述后端能力，零值可能是“未知/不支持”，不能只读数值忽略标记。
 * 不同后端的原始计数域不同，须结合counter_hz、time_domain和采样窗口解释。
 * 这里不采集线程、不读取硬件，也不把诊断快照作为设备执行授权。
 */
#ifndef DP_OSAL_THREAD_SNAPSHOT_H_
#define DP_OSAL_THREAD_SNAPSHOT_H_

#include <cstdint>

namespace dp::osal {

/** @brief 说明运行时测量可用性与拒绝原因，值来自后端判定而非本结构自动检测。 */
enum class MeasurementValidity : uint8_t {
    kUnsupported = 0U,        ///< 后端没有提供所需能力。
    kValid = 1U,              ///< 后端认为当前窗口符合其有效性条件。
    kInsufficientWindow = 2U, ///< 采样窗口不足，不能作所需比较。
    kCounterAmbiguous = 3U,   ///< 计数差/回绕等存在歧义。
    kSampleTruncated = 4U,    ///< 有界采集未能保留所需全部样本。
};

/** @brief 原始运行时计数的时间域，频率和单位须与counter_hz一起检查。 */
enum class MeasurementTimeDomain : uint8_t {
    kUnknown = 0U,               ///< 尚未说明或不支持的时间域。
    kFreeRtosRuntimeCounter = 1U, ///< FreeRTOS配置的运行时统计计数。
    kDwtCycles = 2U,             ///< DWT周期计数，需要有效的周期频率。
    kPosixProcStatTicks = 3U,    ///< POSIX /proc统计tick，不直接等于微秒。
    kMonotonicMicroseconds = 4U, ///< 单调微秒计数域。
};

/**
 * @brief 以无符号减法取得模2^32计数差，支持一次邻近回绕。
 * @param newer 较新的计数观测。
 * @param older 较旧的计数观测。
 * @return 模2^32的差，函数本身不证明先后顺序或完整回绕次数。
 */
constexpr uint32_t runtime_counter_delta_u32(uint32_t newer, uint32_t older) {
    return newer - older;
}

/**
 * @brief 要求模计数差不超过预算，且预算严格小于半圈。
 * @param newer 较新计数。
 * @param older 较旧计数。
 * @param max_delta 允许的最大计数差，必须小于2^31。
 * @return 两个条件同时满足为true；仍需外部采样约束排除多个完整回绕。
 */
constexpr bool runtime_counter_delta_is_bounded(uint32_t newer, uint32_t older, uint32_t max_delta) {
    return runtime_counter_delta_u32(newer, older) <= max_delta && max_delta < 0x80000000U;
}

/**
 * @brief 用64位中间乘积把计数差换成微秒，再收窄成32位结果。
 * @param delta 原始计数差。
 * @param counter_hz 每秒计数频率；零频率直接返回零。
 * @return 向下取整的微秒结果，超出uint32_t仍会截断，不是饱和转换。
 * @pre 调用者若需要精确32位结果，应先限制delta/频率对应的微秒范围。
 */
constexpr uint32_t runtime_counter_delta_to_us(uint32_t delta, uint32_t counter_hz) {
    return counter_hz == 0U
               ? 0U
               : static_cast<uint32_t>((static_cast<uint64_t>(delta) * 1000000ULL) / counter_hz);
}

/**
 * @brief 一项线程诊断快照，字符串与所有数值都由所选后端填充。
 * @note CPU、运行时、栈及ISR能力分别标记；validity给出整体测量边界原因。
 */
struct ThreadSnapshot {
    char name[16]{}; ///< 有界线程名，由后端负责复制及终止符，可能截断。
    uint32_t cpu_pct_x10{};  ///< CPU百分比乘10，如125代表12.5%；先检查cpu_valid。
    uint32_t stack_hwm{};    ///< 后端提供的最小空闲栈字节数，先检查stack_valid。
    uint8_t state{}; ///< 后端线程状态编码，不跨后端统一为操作系统原始枚举。
    uint32_t stack_pointer{};  ///< 任务保存的栈指针（预缓存用，0 = 未知）
    uint32_t runtime_us{};   ///< 当前窗口归属本线程的运行微秒数，先检查runtime_valid。
    uint32_t isr_runtime_us{};  ///< 平台可观测的ISR运行微秒数，先检查isr_valid。
    uint32_t sample_window_us{}; ///< 本次统计窗口长度，单位微秒。
    uint32_t sample_sequence{}; ///< 后端采样序号，用于识别观测，不是设备会话身份。
    uint32_t counter_hz{}; ///< 原始统计计数频率，单位Hz，零表示未知/不可换算。
    uint16_t instrumentation_overhead_us{}; ///< 后端记录的插桩开销微秒值，范围及截断策略看提供者。
    uint8_t cpu_valid{}; ///< CPU占比字段是否有效。
    uint8_t runtime_valid{}; ///< 线程运行时字段是否有效。
    uint8_t stack_valid{}; ///< 栈余量字段是否有效。
    uint8_t isr_valid{}; ///< ISR运行时字段是否有效，不等价于线程运行时有效。
    uint8_t isr_wrapped{};  ///< 本样本已注册板级IRQ包装；不是计数器发生回绕的标记。
    uint8_t time_domain{static_cast<uint8_t>(MeasurementTimeDomain::kUnknown)}; ///< MeasurementTimeDomain的字节表示。
    uint8_t validity{static_cast<uint8_t>(MeasurementValidity::kUnsupported)}; ///< MeasurementValidity的字节表示及原因。
};

} // namespace dp::osal

#endif  // DP_OSAL_THREAD_SNAPSHOT_H_
