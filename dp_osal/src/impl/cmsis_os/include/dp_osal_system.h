/**
 * @file dp_osal_system.h
 * @brief CMSIS系统服务及三种线程诊断路径：流式runtime、批量runtime、基础枚举回退。
 * @details 通用层计算窗口/任务generation匹配/有效标记，端口提供原始任务元数据与计数。
 * runtime历史由一次尝试采样门保护；嵌套或争用返回0，不要求调用方新增单owner约束。
 * sleep_ms按实际内核频率换算，协作停止仅提前返回，不中断业务栈。
 * 调度锁不屏蔽全部中断，快照诊断也不构成硬件身份、运动授权或完整资格认证。
 */
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <cstring>

#include "interface_system.h"
#include "detail/dp_osal_snapshot_detail.h"
#include "dp_osal.h"
#include "dp_osal_thread_stop.h"

namespace dp::osal {

/**
 * @brief CMSIS系统单例与后端诊断装配，按端口宏选择现有能力路径。
 */
class System : public SystemBase<System> {
    friend class SystemBase<System>;

public:
    /**
     * @brief 惰性构造并返回系统单例。
     * @return 静态对象引用，不转移所有权。
     */
    static System& getInstance() {
        static System instance;
        return instance;
    }

private:
    /**
     * @brief 先调用osKernelInitialize再osKernelStart，两个状态返回值都被忽略。
     * @note 是否可返回及启动失败处理依实际内核装配，void返回不提供成功证明。
     */
    void doStartScheduler() {
        /* Ensure the kernel is initialized before starting the scheduler.
         * On MCU firmware (dp_stm32f427_dev) osKernelInitialize() is called
         * by the dp_stm32f427 cmsis_os2 port inside osThreadNew() if the
         * kernel is still inactive.  On POSIX simulator products the kernel
         * starts inactive here, so we must call osKernelInitialize() to
         * transition KernelState to osKernelReady before osKernelStart();
         * without this osKernelStart() returns osError immediately and the
         * scheduler never runs, causing the Thread destructor to call
         * vPortYield from the main thread and fire vAssertCalled. */
        osKernelInitialize();
        osKernelStart();
    }

    /** @brief 分片睡眠，毫秒按内核频率向上换算；停止请求仅提前返回，不抛异常。
     * 业务循环必须自行检查 stopRequested；睡眠不保证任意业务自动退出。
     */
    void doSleepMs(const uint32_t milliseconds) const {
        constexpr uint32_t kChunkMs = 100U;
        uint32_t remaining = milliseconds;
        while (remaining != 0U) {
            auto* stop_flag = osal_cmsis_stop_flag_get();
            if (stop_flag != nullptr && stop_flag->load(std::memory_order_acquire)) return;
            const uint32_t chunk = remaining > kChunkMs ? kChunkMs : remaining;
            const uint64_t ticks = detail::cmsisMillisecondsToTicks(chunk, osKernelGetTickFreq());
            if (ticks != 0U) (void)osDelay(static_cast<uint32_t>(ticks));
            remaining -= chunk;
        }
    }

    /** @brief 分秒睡眠避免 seconds * 1000 的 32 位回绕。 */
    void doSleep(const uint32_t seconds) const {
        uint32_t remaining = seconds;
        while (remaining != 0U) {
            auto* stop_flag = osal_cmsis_stop_flag_get();
            if (stop_flag != nullptr && stop_flag->load(std::memory_order_acquire)) return;
            const uint32_t chunk = remaining > 3600U ? 3600U : remaining;
            doSleepMs(chunk * 1000U);
            remaining -= chunk;
        }
    }

    /**
     * @brief 原生内核tick乘1000再除频率，得到32位毫秒表示。
     * @return 收窄后的计数，不是Unix日期；原生tick回绕及频率会影响结果。
     * @pre 内核频率必须非零，本实现不额外检测。
     */
    [[nodiscard]] uint32_t doGetTickMs() const {
        return static_cast<uint32_t>(static_cast<uint64_t>(osKernelGetTickCount()) * 1000ULL / osKernelGetTickFreq());
    }

    /**
     * @brief 请求osKernelLock并忽略状态，不保存进入前锁状态。
     * @note 这是调度器锁，不是屏蔽所有IRQ，也不提供跨核数据互斥或任意嵌套恢复保证。
     */
    void doEnterCritical() const { (void)osKernelLock(); }
    /**
     * @brief 请求osKernelUnlock并忽略状态，调用者负责配对及当前上下文合法性。
     */
    void doExitCritical() const { (void)osKernelUnlock(); }

    /**
     * @brief 返回固定CMSIS后端说明。
     * @return 静态字符串，调用者不释放。
     */
    [[nodiscard]] const char* doGetSystemInfo() const {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS
    /**
     * @brief 流式路径的旧任务身份与计数，句柄加generation共同匹配，避免只按复用地址识别。
     */
    struct RuntimeBaseline {
        void* handle;              ///< 端口给出的任务句柄，只作身份匹配，不在通用层解引用。
        uint32_t task_generation;  ///< 与句柄组合区分复用任务，由端口提供可靠性。
        uint32_t runtime_counter;  ///< 上一窗口该任务的原始32位运行计数。
    };
    /**
     * @brief 一次流式采样的共享窗口信息，各有效条件独立记录。
     */
    struct RuntimeWindow {
        uint32_t total_delta{};       ///< 本次总运行计数与旧基线的模差。
        uint32_t window_us{};         ///< 由总计数差/频率换算的32位微秒窗口。
        uint32_t isr_delta{};         ///< ISR计数与旧基线的模差。
        uint32_t counter_hz{};        ///< 当前端口计数频率，零不可换算有效窗口。
        uint32_t sample_sequence{};   ///< 跨调用的样本序号。
        bool total_bounded{};         ///< 总计数差具备独立宽源范围证明。
        bool isr_bounded{};           ///< ISR计数差不大于已证明总窗口。
        bool truncated{};             ///< 实际总数超过可用输出/端口容量或返回不完整。
        uint8_t isr_counter_valid{};  ///< 端口声明ISR计数源有效，不是通用层硬件认证。
    };
    /**
     * @brief 端口定义的流式会话游标类型，具体锁定与枚举存储由端口实现。
     */
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
    using RuntimeCursor = DP_OSAL_PORT_RUNTIME_CURSOR_TYPE;
    /**
     * @brief 端口定义的单行任务元数据类型，供通用层逐行转换。
     */
#endif
    using RuntimeRow = DP_OSAL_PORT_RUNTIME_ROW_TYPE;
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM

    /**
     * @brief 借用cursor的RAII结束守卫，正常完成或提前return均调用一次端口END。
     */
    class RuntimeScope {
    public:
        /**
         * @brief 保存借用cursor地址，不自行开始会话。
         * @param cursor 已由端口BEGIN初始化且覆盖此守卫存活期的游标。
         */
        explicit RuntimeScope(RuntimeCursor& cursor) noexcept : cursor_(&cursor) {}
        /**
         * @brief 禁止复制，避免一个游标被多个守卫结束。
         */
        RuntimeScope(const RuntimeScope&) = delete;
        /**
         * @brief 禁止复制赋值覆盖所负责的游标。
         */
        RuntimeScope& operator=(const RuntimeScope&) = delete;
        /**
         * @brief 调用finish释放尚未结束的端口会话。
         */
        ~RuntimeScope() { finish(); }
        /**
         * @brief 若cursor非空则调用端口END并置空，重复调用不再结束第二次。
         * @note 不检查END结果，不证明端口底层资源关闭一定成功。
         */
        void finish() noexcept {
            if (cursor_ != nullptr) {
                DP_OSAL_PORT_RUNTIME_END(cursor_);
                cursor_ = nullptr;
            }
        }

    private:
        RuntimeCursor* cursor_;  ///< 借用cursor，finish后置空防重复END。
    };

#endif
    /** @brief 可靠宽源缺失时明确返回无效，不能从32位差推测完整回绕次数。 */
    static detail::SnapshotCounterStamp runtime_stamp() noexcept {
#if defined(DP_OSAL_PORT_RUNTIME_COUNTER_WIDE_NOW) && defined(DP_OSAL_PORT_RUNTIME_COUNTER_EPOCH)
        const uint32_t frequency = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        return {DP_OSAL_PORT_RUNTIME_COUNTER_WIDE_NOW(), frequency, DP_OSAL_PORT_RUNTIME_COUNTER_EPOCH(),
                frequency != 0U};
#else
        return {};
#endif
    }
    /** @brief 同域宽采样括住总计数采样点，受检窗口排除多圈与微秒溢出。 */
    static void complete_runtime_window(RuntimeWindow& window, uint32_t total, uint32_t isr, uint32_t previous_total,
                                        uint32_t previous_isr, const detail::SnapshotCounterStamp& previous_before,
                                        const detail::SnapshotCounterStamp& after) noexcept {
        uint64_t bound = 0U;
        window.counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        const bool bounded = detail::snapshotCounterBound(previous_before, after, window.counter_hz, bound);
        window.total_delta = total - previous_total;
        window.isr_delta = isr - previous_isr;
        window.total_bounded = bounded && window.total_delta <= bound && window.total_delta > 0U &&
                               detail::ticksToUs(window.total_delta, window.counter_hz, window.window_us) &&
                               window.window_us > 0U;
        window.isr_bounded = window.total_bounded && window.isr_delta <= window.total_delta;
    }
    /**
     * @brief 整体清零输出行，再从端口填名字、栈字段与归一化状态，先标栈有效。
     * @param[out] output 待初始化的调用者输出槽。
     * @param row 端口当前任务元数据，name为空时使用unknown。
     * @note 只转换已有数据，stack_valid=1沿用端口前提，不独立验证真实栈扫描质量。
     */
    static void initialize_runtime_row(ThreadSnapshot& output, const RuntimeRow& row) noexcept {
        output = ThreadSnapshot{};
        const char* name = row.name == nullptr ? "unknown" : row.name;
        std::strncpy(output.name, name, sizeof(output.name) - 1U);
        output.name[sizeof(output.name) - 1U] = '\0';
        output.stack_hwm = row.stack_hwm_bytes;
        output.stack_pointer = static_cast<uint32_t>(row.stack_pointer);
        output.stack_valid = 1U;
        switch (row.state) {
            case 0U:
                output.state = 1U;
                break;
            case 1U:
                output.state = 2U;
                break;
            case 2U:
                output.state = 3U;
                break;
            case 3U:
                output.state = 4U;
                break;
            default:
                output.state = 5U;
                break;
        }
    }

    /**
     * @brief 按旧任务身份、窗口界限和截断标记补齐一行CPU/runtime/ISR有效性。
     * @param output 已初始化输出行。
     * @param row 当前任务原始计数与身份。
     * @param window 当前采样窗口及端口能力。
     * @param previous 旧任务基线表，整个查找阶段保持不被覆盖。
     * @param previous_count 旧表有效条目数。
     * @param have_window 前一采样是否已建立可用窗口。
     * @param first_row 是否首条输出，只有首条承载共享ISR统计。
     * @details 任务runtime须无截断、已建窗口、身份匹配、总窗口有宽源证明且微秒非零，任务差不得超出该窗口；
     * CPU×10按任务差/总差并钳到1000。ISR用独立有效门，仅在首行且源有效/差有界/此前有窗口时写入。
     * 截断最后撤销CPU/runtime/stack/ISR有效标记；不能只见某个数值非零就当作有效测量。
     */
    [[gnu::noinline]] static void complete_runtime_row(ThreadSnapshot& output, const RuntimeRow& row,
                                                       const RuntimeWindow& window, const RuntimeBaseline* previous,
                                                       size_t previous_count, bool have_window,
                                                       bool first_row) noexcept {
        output.isr_wrapped = window.isr_counter_valid;
        output.sample_window_us = window.window_us;
        output.sample_sequence = window.sample_sequence;
        output.counter_hz = window.counter_hz;
#ifdef DP_OSAL_PORT_RUNTIME_TIME_DOMAIN
        output.time_domain = static_cast<uint8_t>(DP_OSAL_PORT_RUNTIME_TIME_DOMAIN);
#else
        output.time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kFreeRtosRuntimeCounter);
#endif
        output.validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
        bool found = false;
        uint32_t previous_runtime = 0U;
        for (size_t index = 0U; index < previous_count; ++index) {
            if (previous[index].handle == row.handle && previous[index].task_generation == row.task_generation) {
                found = true;
                previous_runtime = previous[index].runtime_counter;
                break;
            }
        }
        if (!window.truncated && have_window && found && window.total_bounded && window.window_us > 0U) {
            const uint32_t runtime_delta = runtime_counter_delta_u32(row.runtime_counter, previous_runtime);
            uint32_t runtime_us = 0U;
            if (runtime_delta <= window.total_delta &&
                detail::ticksToUs(runtime_delta, window.counter_hz, runtime_us)) {
                output.runtime_us = runtime_us;
                const uint64_t percent = (static_cast<uint64_t>(runtime_delta) * 1000ULL) /
                                         static_cast<uint64_t>(window.total_delta == 0U ? 1U : window.total_delta);
                output.cpu_pct_x10 = static_cast<uint32_t>(percent > 1000ULL ? 1000ULL : percent);
                output.runtime_valid = 1U;
                output.cpu_valid = 1U;
                output.validity = static_cast<uint8_t>(MeasurementValidity::kValid);
            } else {
                output.validity = static_cast<uint8_t>(MeasurementValidity::kCounterAmbiguous);
            }
        }
        if (first_row && !window.truncated && window.isr_counter_valid != 0U && window.isr_bounded && have_window &&
            detail::ticksToUs(window.isr_delta, window.counter_hz, output.isr_runtime_us))
            output.isr_valid = 1U;
        if (window.truncated) {
            output.cpu_valid = 0U;
            output.runtime_valid = 0U;
            output.stack_valid = 0U;
            output.isr_valid = 0U;
            output.validity = static_cast<uint8_t>(MeasurementValidity::kSampleTruncated);
        }
    }

    // 只持有句柄 cursor 与单行 metadata；全部 baseline 查找后才开始覆盖旧表。
    /**
     * @brief 用cursor及单行元数据完成三轮枚举，保留旧表直到全部输出统计计算结束。
     * @param[out] output 至少容纳max项的输出数组，外层已验证非空。
     * @param max 调用者容量，与端口最大线程数取较小值。
     * @return 实际输出条目数；任一READ失败清窗口基线并返回0，忽略可能已局部写入的数组内容。
     * @details 首轮遍历全体enumerated行、只初始化可输出部分；随后采样总/ISR计数。第二轮对
     * 可输出行匹配旧表并算窗口，第三轮才覆盖基线。RAII结束cursor后测量插桩开销并饱和到16位。
     * @note 外层门保护共享历史；32位窗口必须取得独立宽源证明与受检换算。
     */
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
    [[gnu::noinline]] size_t doGetThreadSnapshotStream(ThreadSnapshot* output, size_t max) const {
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};  ///< 跨调用历史，由外层采样门保护。
        static size_t previous_count = 0U;                                    ///< 旧表实际保留的任务数。
        static uint32_t previous_total = 0U;                                  ///< 上次总运行计数。
        static uint8_t previous_isr_valid = 0U;
        static uint32_t previous_isr = 0U;  ///< 上次ISR计数。
        static uint32_t sequence = 0U;      ///< 当前路径共享的样本序号。
        static bool have_window = false;    ///< 是否保留完整旧窗口。
        static detail::SnapshotCounterStamp previous_before{};
        const auto stamp_before = runtime_stamp();
        RuntimeCursor cursor{};
        RuntimeRow row{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS ? max : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        size_t total_count = 0U;
        const size_t enumerated = DP_OSAL_PORT_RUNTIME_BEGIN(&cursor, &total_count);
        RuntimeScope scope(cursor);
        const uint32_t sample_sequence = ++sequence;
        const size_t got = enumerated < capacity ? enumerated : capacity;
        // 第一轮遍历全部enumerated行，请求READ模式1；即使输出容量较小也保留全体扫描后的计数采样点。
        for (size_t index = 0U; index < enumerated; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 1U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            if (index < got) initialize_runtime_row(output[index], row);
        }
        uint32_t total_counter = 0U;
        uint32_t isr_counter = 0U;
        uint8_t isr_counter_valid = 0U;
        // 保留原全体 HWM 扫描结束后的 counter 采样点。
        DP_OSAL_PORT_RUNTIME_COUNTERS(&cursor, &total_counter, &isr_counter, &isr_counter_valid);
        RuntimeWindow window{};
        complete_runtime_window(window, total_counter, isr_counter, previous_total, previous_isr, previous_before,
                                runtime_stamp());
        window.sample_sequence = sample_sequence;
        window.truncated = total_count > capacity || got != total_count || enumerated != total_count;
        window.isr_counter_valid = isr_counter_valid;
        window.isr_bounded = window.isr_bounded && previous_isr_valid != 0U;
        // 后续逐行遍历分别完成统计与基线发布，计算全部旧表匹配之前不覆盖previous。
        for (size_t index = 0U; index < got; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            complete_runtime_row(output[index], row, window, previous, previous_count, have_window, index == 0U);
        }
        if (!window.truncated && got > 0U) {
            for (size_t index = 0U; index < got; ++index) {
                if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                    previous_count = 0U;
                    have_window = false;
                    return 0U;
                }
                previous[index] = {row.handle, row.task_generation, row.runtime_counter};
            }
            previous_count = got;
            previous_total = total_counter;
            previous_isr = isr_counter;
            previous_isr_valid = isr_counter_valid;
            previous_before = stamp_before;
            have_window = stamp_before.valid;
        } else {
            previous_count = 0U;
            have_window = false;
        }
        scope.finish();
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        uint32_t overhead_us = 0U;
        (void)detail::ticksToUs(counter_after - counter_before, window.counter_hz, overhead_us);
        const uint16_t overhead = static_cast<uint16_t>(overhead_us > 0xFFFFU ? 0xFFFFU : overhead_us);
        for (size_t index = 0U; index < got; ++index) output[index].instrumentation_overhead_us = overhead;
        return got;
    }
#endif
#endif

    /**
     * @brief 根据端口宏选择流式、批量runtime或仅基础CMSIS线程枚举。
     * @param[out] buf 至少可容纳max项的调用者缓冲，空指针直接返回0。
     * @param max 输出容量，零直接返回0；各端口/回退还会限制内部最大条目数。
     * @return 实际写入条目数，失败或无任务可为0；不构成全量稳定任务集合证明。
     * @details runtime路径按句柄+generation匹配旧表，有界且连续无截断窗口才赋有效运行时；
     * CPU/runtime和首行ISR分别有门。无runtime能力时最多枚举32项，只提供基础名字/状态/栈，
     * CPU/runtime/ISR标无效，未覆盖的数值可能保留调用者旧缓冲内容，必须检查标记。
     * @note runtime路径共享一次尝试门，争用返回0；普通枚举状态沿内核编码。
     */
    size_t doGetThreadSnapshot(ThreadSnapshot* buf, size_t max) const {
        if (buf == nullptr || max == 0) {
            return 0;
        }

#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS
        static std::atomic_flag gate = ATOMIC_FLAG_INIT;
        detail::SnapshotTryScope sampling(gate);
        if (!sampling) return 0U;
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
        return doGetThreadSnapshotStream(buf, max);
#else
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        static size_t previous_count = 0U;
        static uint32_t previous_total = 0U, previous_isr = 0U, sequence = 0U;
        static uint8_t previous_isr_valid = 0U;
        static bool have_window = false;
        static detail::SnapshotCounterStamp previous_before{};
        RuntimeRow runtime[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS ? max : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const auto stamp_before = runtime_stamp();
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        uint32_t total_counter = 0U, isr_counter = 0U;
        uint8_t isr_valid = 0U;
        size_t total_count = 0U;
        const size_t returned =
            DP_OSAL_PORT_RUNTIME_GET_THREADS(runtime, capacity, &total_counter, &isr_counter, &isr_valid, &total_count);
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        RuntimeWindow window{};
        complete_runtime_window(window, total_counter, isr_counter, previous_total, previous_isr, previous_before,
                                runtime_stamp());
        window.sample_sequence = ++sequence;
        window.truncated = total_count > capacity || returned != total_count || returned > capacity;
        window.isr_counter_valid = isr_valid;
        window.isr_bounded = window.isr_bounded && previous_isr_valid != 0U;
        const size_t got = returned < capacity ? returned : capacity;
        uint32_t overhead_us = 0U;
        (void)detail::ticksToUs(counter_after - counter_before, window.counter_hz, overhead_us);
        for (size_t i = 0U; i < got; ++i) {
            initialize_runtime_row(buf[i], runtime[i]);
            complete_runtime_row(buf[i], runtime[i], window, previous, previous_count, have_window, i == 0U);
            buf[i].instrumentation_overhead_us =
                static_cast<uint16_t>(overhead_us > UINT16_MAX ? UINT16_MAX : overhead_us);
        }
        if (!window.truncated && got > 0U) {
            for (size_t i = 0U; i < got; ++i)
                previous[i] = {runtime[i].handle, runtime[i].task_generation, runtime[i].runtime_counter};
            previous_count = got;
            previous_total = total_counter;
            previous_isr = isr_counter;
            previous_isr_valid = isr_valid;
            previous_before = stamp_before;
            have_window = stamp_before.valid;
        } else {
            previous_count = 0U;
            have_window = false;
        }
        return got;
#endif
#else

        // 无runtime能力时只枚举基础信息，CPU/运行时/ISR标无效；不伪造性能计数。
        uint32_t count = osThreadGetCount();
        if (count == 0) {
            return 0;
        }

        osThreadId_t ids[32] = {nullptr};
        const bool sample_truncated = count > max || count > 32U;
        uint32_t cap = count;
        if (cap > max) {
            cap = static_cast<uint32_t>(max);
        }
        if (cap > 32U) {
            cap = 32U;
        }
        uint32_t got = osThreadEnumerate(ids, cap);

        size_t out = 0;
        for (uint32_t i = 0; i < got && out < max; ++i, ++out) {
            buf[out] = ThreadSnapshot{};
            const char* name = osThreadGetName(ids[i]);
            if (name == nullptr) {
                name = "unknown";
            }
            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            // 此回退分支不整体清buf[out]，未覆盖的旧数值必须结合无效标记拒绝使用。
            buf[out].cpu_pct_x10 = 0U;
            buf[out].stack_hwm = osThreadGetStackSpace(ids[i]);
            buf[out].state = static_cast<uint8_t>(osThreadGetState(ids[i]));
            buf[out].stack_pointer = DP_OSAL_PORT_THREAD_STACK_POINTER_FROM_ID(ids[i]);
            buf[out].cpu_valid = 0U;
            buf[out].runtime_valid = 0U;
            buf[out].stack_valid = 1U;
            buf[out].isr_valid = 0U;
            buf[out].isr_wrapped = 0U;
            buf[out].time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kUnknown);
            buf[out].validity = static_cast<uint8_t>(sample_truncated ? MeasurementValidity::kSampleTruncated
                                                                      : MeasurementValidity::kUnsupported);
            if (sample_truncated) {
                buf[out].stack_valid = 0U;
            }
        }
        return out;
#endif
    }
#endif

    /**
     * @brief 私有空构造，不在构造阶段启动调度器。
     */
    System(){};

    /**
     * @brief 私有空析构，不停止系统任务或设备。
     */
    ~System(){};
};

}  // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
