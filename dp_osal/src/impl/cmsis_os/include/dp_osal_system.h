/**
 * @file dp_osal_system.h
 * @brief CMSIS系统服务及三种线程诊断路径：流式runtime、批量runtime、基础枚举回退。
 * @details 通用层计算窗口/任务generation匹配/有效标记，端口提供原始任务元数据与计数。
 * runtime历史为共享静态表且不加锁，调用者须单owner或外部串行化；端口cursor保护不替代此约束。
 * sleep_ms当前把分片数直接给osDelay，未按频率换算；物理毫秒仅在相应tick配置成立。
 * 调度锁不屏蔽全部中断，快照诊断也不构成硬件身份、运动授权或完整资格认证。
 */
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <cstring>

#include "interface_system.h"
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
    static System &getInstance() {
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

    /* 保留现有分片算法：每次最多取数值100并直接传osDelay，返回后检查停止标志。
     * 这里不调用osThreadFlagsWait，也不按内核频率把名义毫秒换成tick，不能据旧注释保证1ms停止。
     * 实际物理时长/停止延迟还依赖tick频率、调度与任务是否继续执行。 */
    /**
     * @brief 将名义毫秒请求拆成最多100的片段，直接osDelay后检查任务停止标志。
     * @param milliseconds 请求量名称为毫秒，现实现不按osKernelGetTickFreq转换成tick。
     * @details osDelay消费原生tick；只有tick为1000Hz等相应配置时请求数值与物理毫秒一致。
     * 停止标志在每段返回后检查，启用异常时抛内部停止异常，禁异常时仅返回。
     * @note 不等待线程flags，零请求不进入循环或检查停止；不能凭此代码承诺1ms/100ms硬停止上界。
     */
    void doSleepMs(const uint32_t milliseconds) const {
        static constexpr uint32_t SLEEP_CHUNK_MS = 100U; ///< 名义毫秒分片上限，传osDelay时未经tick频率换算。
        uint32_t remaining = milliseconds;
        while (remaining > 0U) {
            uint32_t chunk = (remaining > SLEEP_CHUNK_MS) ? SLEEP_CHUNK_MS : remaining;
            osDelay(chunk);
            remaining -= chunk;
            auto *stop_flag = osal::osal_cmsis_stop_flag_get();
            if (stop_flag != nullptr && stop_flag->load(std::memory_order_acquire)) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                throw CmsisThreadStopException{};
#else
                return;  /* cooperative early return on MCU firmware */
#endif
            }
        }
    }

    /**
     * @brief 将秒数乘1000后交给现有doSleepMs。
     * @param seconds 相对秒请求，调用者限制乘法范围。
     * @note 乘法可能在32位回绕，且沿用doSleepMs未按tick频率换算的现有边界。
     */
    void doSleep(const uint32_t seconds) const { doSleepMs(seconds * 1000); }

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
    void doEnterCritical() const {
        (void)osKernelLock();
    }
    /**
     * @brief 请求osKernelUnlock并忽略状态，调用者负责配对及当前上下文合法性。
     */
    void doExitCritical() const {
        (void)osKernelUnlock();
    }

    /**
     * @brief 返回固定CMSIS后端说明。
     * @return 静态字符串，调用者不释放。
     */
    [[nodiscard]] const char *doGetSystemInfo() const {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS && \
    defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
    /**
     * @brief 流式路径的旧任务身份与计数，句柄加generation共同匹配，避免只按复用地址识别。
     */
    struct RuntimeBaseline {
        void* handle; ///< 端口给出的任务句柄，只作身份匹配，不在通用层解引用。
        uint32_t task_generation; ///< 与句柄组合区分复用任务，由端口提供可靠性。
        uint32_t runtime_counter; ///< 上一窗口该任务的原始32位运行计数。
    };
    /**
     * @brief 一次流式采样的共享窗口信息，各有效条件独立记录。
     */
    struct RuntimeWindow {
        uint32_t total_delta{}; ///< 本次总运行计数与旧基线的模差。
        uint32_t window_us{}; ///< 由总计数差/频率换算的32位微秒窗口。
        uint32_t isr_delta{}; ///< ISR计数与旧基线的模差。
        uint32_t counter_hz{}; ///< 当前端口计数频率，零不可换算有效窗口。
        uint32_t sample_sequence{}; ///< 跨调用的样本序号。
        bool total_bounded{}; ///< 总计数差通过半圈预算检查。
        bool isr_bounded{}; ///< ISR计数差独立通过半圈预算检查。
        bool truncated{}; ///< 实际总数超过可用输出/端口容量或返回不完整。
        uint8_t isr_counter_valid{}; ///< 端口声明ISR计数源有效，不是通用层硬件认证。
    };
    /**
     * @brief 端口定义的流式会话游标类型，具体锁定与枚举存储由端口实现。
     */
    using RuntimeCursor = DP_OSAL_PORT_RUNTIME_CURSOR_TYPE;
    /**
     * @brief 端口定义的单行任务元数据类型，供通用层逐行转换。
     */
    using RuntimeRow = DP_OSAL_PORT_RUNTIME_ROW_TYPE;

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
        RuntimeCursor* cursor_; ///< 借用cursor，finish后置空防重复END。
    };

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
            case 0U: output.state = 1U; break;
            case 1U: output.state = 2U; break;
            case 2U: output.state = 3U; break;
            case 3U: output.state = 4U; break;
            default: output.state = 5U; break;
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
     * @details 任务runtime须无截断、已建窗口、身份匹配、总窗口有界且微秒非零，再检查任务差小于半圈；
     * CPU×10按任务差/总差并钳到1000。ISR用独立有效门，仅在首行且源有效/差有界/此前有窗口时写入。
     * 截断最后撤销CPU/runtime/stack/ISR有效标记；不能只见某个数值非零就当作有效测量。
     */
    [[gnu::noinline]] static void complete_runtime_row(
        ThreadSnapshot& output, const RuntimeRow& row, const RuntimeWindow& window,
        const RuntimeBaseline* previous, size_t previous_count,
        bool have_window, bool first_row) noexcept {
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
            if (previous[index].handle == row.handle &&
                previous[index].task_generation == row.task_generation) {
                found = true;
                previous_runtime = previous[index].runtime_counter;
                break;
            }
        }
        if (!window.truncated && have_window && found &&
            window.total_bounded && window.window_us > 0U) {
            const uint32_t runtime_delta = runtime_counter_delta_u32(row.runtime_counter, previous_runtime);
            if (runtime_counter_delta_is_bounded(row.runtime_counter, previous_runtime, 0x7FFFFFFFU)) {
                output.runtime_us = runtime_counter_delta_to_us(runtime_delta, window.counter_hz);
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
        if (first_row && window.isr_counter_valid != 0U && window.isr_bounded && have_window) {
            output.isr_runtime_us = runtime_counter_delta_to_us(window.isr_delta, window.counter_hz);
            output.isr_valid = 1U;
        }
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
     * @note 静态历史无锁，调用者串行访问；微秒换算使用32位结果，不能据此排除所有计数质量问题。
     */
    [[gnu::noinline]] size_t doGetThreadSnapshotStream(ThreadSnapshot* output, size_t max) const {
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{}; ///< 跨调用历史，通用层不加锁，须owner串行访问。
        static size_t previous_count = 0U; ///< 旧表实际保留的任务数。
        static uint32_t previous_total = 0U; ///< 上次总运行计数。
        static uint32_t previous_isr = 0U; ///< 上次ISR计数。
        static uint32_t sequence = 0U; ///< 当前路径共享的样本序号。
        static bool have_window = false; ///< 前一窗口是否具备无截断、有界且非零的基线条件。
        RuntimeCursor cursor{};
        RuntimeRow row{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS
            ? max : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
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
        window.counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        window.total_delta = runtime_counter_delta_u32(total_counter, previous_total);
        window.total_bounded = runtime_counter_delta_is_bounded(total_counter, previous_total, 0x7FFFFFFFU);
        window.window_us = window.total_bounded
            ? runtime_counter_delta_to_us(window.total_delta, window.counter_hz) : 0U;
        window.isr_delta = runtime_counter_delta_u32(isr_counter, previous_isr);
        window.isr_bounded = runtime_counter_delta_is_bounded(isr_counter, previous_isr, 0x7FFFFFFFU);
        window.sample_sequence = sample_sequence;
        window.truncated = total_count > capacity || got < total_count;
        window.isr_counter_valid = isr_counter_valid;
        // 后续逐行遍历分别完成统计与基线发布，计算全部旧表匹配之前不覆盖previous。
        for (size_t index = 0U; index < got; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            complete_runtime_row(output[index], row, window, previous, previous_count,
                                 have_window, index == 0U);
        }
        // 后续逐行遍历分别完成统计与基线发布，计算全部旧表匹配之前不覆盖previous。
        for (size_t index = 0U; index < got; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            // 第三轮在所有旧表查找之后才发布下一次的基线，避免边读旧表边覆盖。
            previous[index] = {row.handle, row.task_generation, row.runtime_counter};
        }
        previous_count = got;
        previous_total = total_counter;
        previous_isr = isr_counter;
        have_window = !window.truncated && window.total_bounded && window.window_us > 0U;
        scope.finish();
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        const uint32_t overhead_us = runtime_counter_delta_to_us(
            counter_after - counter_before, window.counter_hz);
        const uint16_t overhead = static_cast<uint16_t>(overhead_us > 0xFFFFU ? 0xFFFFU : overhead_us);
        for (size_t index = 0U; index < got; ++index)
            output[index].instrumentation_overhead_us = overhead;
        return got;
    }
#endif

    /**
     * @brief 根据端口宏选择流式、批量runtime或仅基础CMSIS线程枚举。
     * @param[out] buf 至少可容纳max项的调用者缓冲，空指针直接返回0。
     * @param max 输出容量，零直接返回0；各端口/回退还会限制内部最大条目数。
     * @return 实际写入条目数，失败或无任务可为0；不构成全量稳定任务集合证明。
     * @details runtime路径按句柄+generation匹配旧表，有界且连续无截断窗口才赋有效运行时；
     * CPU/runtime和首行ISR分别有门。无runtime能力时最多枚举32项，只提供基础名字/状态/栈，
     * CPU/runtime/ISR标无效，未覆盖的数值可能保留调用者旧缓冲内容，必须检查标记。
     * @note 各runtime路径的static历史未加锁，须由单owner或外部同步；普通枚举状态沿内核编码。
     */
    size_t doGetThreadSnapshot(ThreadSnapshot *buf, size_t max) const {
        if (buf == nullptr || max == 0) {
            return 0;
        }

#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
        return doGetThreadSnapshotStream(buf, max);
#else
        /* Runtime stats are collected through a bounded port callback.  The
         * OSAL layer owns the validity/window arithmetic; application code
         * never needs FreeRTOS task handles or counters. */
        struct RuntimeBaseline {
            void *handle; ///< 批量回调路径的旧任务句柄。
            uint32_t task_generation; ///< 端口给出的任务generation，防止仅句柄复用误匹配。
            uint32_t runtime_counter; ///< 旧窗口任务原始计数。
        };
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{}; ///< 跨调用历史，通用层不加锁，须owner串行访问。
        static size_t previous_count = 0U; ///< 旧表实际保留的任务数。
        static uint32_t previous_total = 0U; ///< 上次总运行计数。
        static uint32_t previous_isr = 0U; ///< 上次ISR计数。
        static uint32_t sequence = 0U; ///< 当前路径共享的样本序号。
        static bool have_window = false; ///< 前一窗口是否具备无截断、有界且非零的基线条件。

        // 批量路径使用既有固定本地数组；与stream逐行cursor的栈占用不同，不在注释中改布局。
        dp_f405_thread_runtime_t runtime[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS
                                    ? max
                                    : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        uint32_t total_counter = 0U;
        uint32_t isr_counter = 0U;
        uint8_t isr_counter_valid = 0U;
        size_t total_count = 0U;
        const size_t got = DP_OSAL_PORT_RUNTIME_GET_THREADS(runtime, capacity, &total_counter,
                                                            &isr_counter, &isr_counter_valid,
                                                            &total_count);
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        const uint32_t counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        const uint32_t instrumentation_counter = counter_after - counter_before;
        const uint32_t total_delta = runtime_counter_delta_u32(total_counter, previous_total);
        const bool total_bounded = runtime_counter_delta_is_bounded(total_counter, previous_total,
                                                                    0x7FFFFFFFU);
        const uint32_t window_us = total_bounded
                                       ? runtime_counter_delta_to_us(total_delta, counter_hz)
                                       : 0U;
        const uint32_t isr_delta = runtime_counter_delta_u32(isr_counter, previous_isr);
        const bool isr_bounded = runtime_counter_delta_is_bounded(isr_counter, previous_isr,
                                                                  0x7FFFFFFFU);
        const uint32_t overhead_us = runtime_counter_delta_to_us(instrumentation_counter, counter_hz);
        const uint32_t sample_sequence = ++sequence;
        const uint32_t sample_overhead = overhead_us > 0xFFFFU ? 0xFFFFU : overhead_us;
        const bool sample_truncated = total_count > capacity || got < total_count;

        size_t out = 0U;
        for (size_t i = 0U; i < got && out < max; ++i, ++out) {
            // 输出可由调用者长期复用；无效窗口不能继承上一轮有效标记或计量值。
            buf[out] = ThreadSnapshot{};
            const char *name = runtime[i].name == nullptr ? "unknown" : runtime[i].name;
            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            buf[out].stack_hwm = runtime[i].stack_hwm_bytes;
            buf[out].stack_pointer = static_cast<uint32_t>(runtime[i].stack_pointer);
            buf[out].stack_valid = 1U;
            buf[out].isr_wrapped = isr_counter_valid;
            buf[out].sample_window_us = window_us;
            buf[out].sample_sequence = sample_sequence;
            buf[out].counter_hz = counter_hz;
            buf[out].instrumentation_overhead_us = static_cast<uint16_t>(sample_overhead);
#ifdef DP_OSAL_PORT_RUNTIME_TIME_DOMAIN
            buf[out].time_domain = static_cast<uint8_t>(DP_OSAL_PORT_RUNTIME_TIME_DOMAIN);
#else
            buf[out].time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kFreeRtosRuntimeCounter);
#endif
            buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
            switch (runtime[i].state) {
                case 0U:
                    buf[out].state = 1U;
                    break;
                case 1U:
                    buf[out].state = 2U;
                    break;
                case 2U:
                    buf[out].state = 3U;
                    break;
                case 3U:
                    buf[out].state = 4U;
                    break;
                default:
                    buf[out].state = 5U;
                    break;
            }

            bool found = false;
            uint32_t previous_runtime = 0U;
            for (size_t j = 0U; j < previous_count; ++j) {
                if (previous[j].handle == runtime[i].handle &&
                    previous[j].task_generation == runtime[i].task_generation) {
                    found = true;
                    previous_runtime = previous[j].runtime_counter;
                    break;
                }
            }
            if (!sample_truncated && have_window && found && total_bounded && window_us > 0U) {
                const uint32_t runtime_delta = runtime_counter_delta_u32(runtime[i].runtime_counter,
                                                                          previous_runtime);
                if (runtime_counter_delta_is_bounded(runtime[i].runtime_counter, previous_runtime,
                                                     0x7FFFFFFFU)) {
                    const uint32_t runtime_us = runtime_counter_delta_to_us(runtime_delta, counter_hz);
                    const uint64_t pct_x10 = (static_cast<uint64_t>(runtime_delta) * 1000ULL) /
                                             static_cast<uint64_t>(total_delta == 0U ? 1U : total_delta);
                    buf[out].runtime_us = runtime_us;
                    buf[out].cpu_pct_x10 = static_cast<uint32_t>(pct_x10 > 1000ULL ? 1000ULL : pct_x10);
                    buf[out].runtime_valid = 1U;
                    buf[out].cpu_valid = 1U;
                    buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kValid);
                } else {
                    buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kCounterAmbiguous);
                }
            }
            if (out == 0U && isr_counter_valid != 0U && isr_bounded && have_window) {
                buf[out].isr_runtime_us = runtime_counter_delta_to_us(isr_delta, counter_hz);
                buf[out].isr_valid = 1U;
            }
        }

        if (sample_truncated) {
            for (size_t i = 0U; i < out; ++i) {
                buf[i].cpu_valid = 0U;
                buf[i].runtime_valid = 0U;
                buf[i].stack_valid = 0U;
                buf[i].isr_valid = 0U;
                buf[i].validity = static_cast<uint8_t>(MeasurementValidity::kSampleTruncated);
            }
        }

        const size_t next_count = got < DP_OSAL_PORT_RUNTIME_MAX_THREADS
                                      ? got
                                      : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        for (size_t i = 0U; i < next_count; ++i) {
            previous[i].handle = runtime[i].handle;
            previous[i].task_generation = runtime[i].task_generation;
            previous[i].runtime_counter = runtime[i].runtime_counter;
        }
        previous_count = next_count;
        previous_total = total_counter;
        previous_isr = isr_counter;
        have_window = !sample_truncated && total_bounded && window_us > 0U;
        return out;
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
            const char *name = osThreadGetName(ids[i]);
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
            buf[out].validity = static_cast<uint8_t>(sample_truncated
                                                         ? MeasurementValidity::kSampleTruncated
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

} // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
