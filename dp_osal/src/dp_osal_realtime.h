/**
 * @file dp_osal_realtime.h
 * @brief 由调用者提供执行栈、使用 raw 入口的单次实时线程门面。
 * @details 启动前检查指针和栈大小，然后调用普通 raw 后端或可选持久线程端口。
 * “实时”表示入口及显式栈约束，不证明调度延迟上界，也不保证启动/连接阶段无OS资源分配。
 * 成功启动后对象被永久标记 consumed，普通线程 join 后也不能复用本门面再次启动。
 * 存储、外部栈和上下文必须覆盖后端使用期；本层不接管上下文释放或保证硬件运动停止。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "dp_osal_port.h"
#include "dp_osal_thread.h"

namespace dp::osal {

/**
 * @brief 与 ThreadEntry 相同的原始函数指针，入口参数为调用者上下文。
 */
using RealtimeEntry = ThreadEntry;

/**
 * @brief 实时线程门面的参数、状态和后端启动结果。
 */
enum class RealtimeStatus : uint8_t {
    kOk = 0,          ///< 当前门面操作完成。
    kInvalidArgument, ///< 入口/栈为空或栈大小不满足范围。
    kInvalidState,    ///< 已消费对象、未启动join或持久线程join等状态不允许。
    kAlreadyStarted,  ///< 当前仍处于started状态，不能重复启动。
    kStartFailed,     ///< 普通后端返回非零或持久端口返回空句柄。
};

/**
 * @brief 包装具体线程后端，成功启动一次后禁止重用。
 * @tparam ThreadType 默认所选 Thread；测试可注入具有 startRaw/join 的替身。
 * @note 本类不加锁，生命周期操作由单一owner串行完成。
 */
template <typename ThreadType = Thread>
class RealtimeThreadWithBackend {
public:
    /**
     * @brief 默认构造后端成员及状态，不启动任务。
     */
    RealtimeThreadWithBackend() = default;
    /**
     * @brief 禁止复制，避免多个门面共享一次线程及外部栈生命周期。
     */
    RealtimeThreadWithBackend(const RealtimeThreadWithBackend&) = delete;
    /**
     * @brief 禁止复制赋值，保持单一owner。
     */
    RealtimeThreadWithBackend& operator=(const RealtimeThreadWithBackend&) = delete;

    /**
     * @brief 校验执行栈后启动一次raw任务，成功后标记不可再次使用。
     * @param entry 有效的原始任务入口。
     * @param context 借用上下文，本层允许NULL，入口与后端须自行遵守各自要求。
     * @param priority 传给后端的优先级编码。
     * @param caller_stack 调用者提供且覆盖整个任务使用期的栈存储，不能为NULL。
     * @param stack_size 栈大小，单位字节，必须不小于端口最小值且可表示为int。
     * @return 依次可能为参数错误、已启动、已消费状态、后端启动失败或kOk。
     * @details 参数先于状态检查；普通路径使用固定名dp_realtime调用startRaw，0视为成功。
     * 若启用持久端口且模板类型恰为默认Thread，则直接调用专用端口并拒绝后续join。
     * @note 这里不检测栈对齐，不验证优先级或上下文内容，不将“调用者栈”扩展成全链路零分配承诺。
     */
    RealtimeStatus start(RealtimeEntry entry, void* context, int priority,
                         void* caller_stack, size_t stack_size) {
        if (entry == nullptr || caller_stack == nullptr ||
            stack_size < static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE) ||
            stack_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return RealtimeStatus::kInvalidArgument;
        }
        if (started_) {
            return RealtimeStatus::kAlreadyStarted;
        }
        if (consumed_) {
            return RealtimeStatus::kInvalidState;
        }
#if defined(DP_OSAL_PORT_PERSISTENT_REALTIME_THREAD)
        if constexpr (std::is_same<ThreadType, Thread>::value) {
            if (dp_osal_port_start_persistent_realtime_thread(entry, context, priority, caller_stack,
                                                               static_cast<uint32_t>(stack_size)) == nullptr) {
                return RealtimeStatus::kStartFailed;
            }
            // 专用持久线程不存入thread_的普通raw句柄；外部栈/上下文须按端口持续存活。
            started_ = true;
            consumed_ = true;
            persistent_ = true;
            return RealtimeStatus::kOk;
        }
#endif
        const int result = thread_.startRaw("dp_realtime", entry, context, priority,
                                            static_cast<int>(stack_size), caller_stack);
        if (result != 0) {
            return RealtimeStatus::kStartFailed;
        }
        started_ = true;
        consumed_ = true;
        return RealtimeStatus::kOk;
    }

    /**
     * @brief 对已启动的普通raw线程调用join，然后清除started标记。
     * @return 未启动或持久路径返回kInvalidState；普通后端join返回后为kOk。
     * @note 不清除consumed，不能借join重启；后端join无状态返回，本门面不额外证明底层连接成功。
     */
    RealtimeStatus join() {
        if (!started_) {
            return RealtimeStatus::kInvalidState;
        }
        if (persistent_) return RealtimeStatus::kInvalidState;
        thread_.join();
        started_ = false;
        return RealtimeStatus::kOk;
    }

private:
    ThreadType thread_;  ///< 普通raw路径使用的后端成员；其析构行为由后端负责。
    bool started_ = false;    ///< 门面认为当前已启动；并非持续查询任务是否仍在执行。
    bool consumed_ = false;   ///< 成功启动后永久为true，join不重置。
    bool persistent_ = false; ///< 使用专用持久端口后为true，该路径不支持本门面的join。
};

/**
 * @brief 绑定产品所选Thread后端的常用实时门面别名。
 */
using RealtimeThread = RealtimeThreadWithBackend<>;

}  // namespace dp::osal
