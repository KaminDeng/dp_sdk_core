/** @file dp_osal_freertos_thread_port.h
 * @brief FreeRTOS CMSIS线程完成与静态资源回收机制，不包含产品任务策略。
 */
#pragma once
#include <cstdint>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"

// 此协议只在单核上证明同步回收。SMP删除可能交给idle延迟处理，禁止误复用外部TCB。
#if defined(configNUMBER_OF_CORES) && configNUMBER_OF_CORES > 1
#error "CMSIS FreeRTOS SMP requires a synchronous native thread reap port"
#endif
#define DP_OSAL_PORT_THREAD_JOINABLE_ATTR 0U
#define DP_OSAL_PORT_THREAD_REAP(handle) dp_osal_freertos_thread_reap(handle)
#define DP_OSAL_PORT_THREAD_FINISH() dp_osal_freertos_thread_finish()
#define DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES(attr) dp_osal_freertos_thread_resources_valid(attr)

inline bool dp_osal_freertos_thread_resources_valid(const osThreadAttr_t& attr) noexcept {
    if (attr.stack_mem == nullptr) return attr.cb_mem == nullptr && attr.cb_size == 0U;
    if ((attr.stack_size % sizeof(StackType_t)) != 0U || attr.cb_mem == nullptr || attr.cb_size < sizeof(StaticTask_t))
        return false;
    void* stack = attr.stack_mem;
    size_t stack_capacity = attr.stack_size;
    void* control_block = attr.cb_mem;
    size_t control_capacity = attr.cb_size;
    return std::align(alignof(StackType_t), attr.stack_size, stack, stack_capacity) == attr.stack_mem &&
           std::align(alignof(StaticTask_t), sizeof(StaticTask_t), control_block, control_capacity) == attr.cb_mem;
}

inline osStatus_t dp_osal_freertos_thread_reap(osThreadId_t id) noexcept {
    auto handle = static_cast<TaskHandle_t>(id);
    // 完成token早于真正离开CPU；等挂起后删除才同步释放静态TCB。
    while (eTaskGetState(handle) != eSuspended) vTaskDelay(1U);
    vTaskDelete(handle);
    return osOK;
}

// 宿主FreeRTOS在owner删除挂起任务时通过pthread_cancel强制展开，必须允许其穿过此帧。
[[noreturn]] inline void dp_osal_freertos_thread_finish() {
    for (;;) vTaskSuspend(nullptr);
}

// 只有正规kernel port提供IRQ hook且内核真实支持indexed通知时才声明该内部能力。
// 其它RTOS/模拟FreeRTOS不因此产生新符号或索引要求；这里不保留任何具体index。
#if defined(DP_OSAL_PORT_NATIVE_IRQ_HOOKS) && DP_OSAL_PORT_NATIVE_IRQ_HOOKS && configUSE_TASK_NOTIFICATIONS == 1 && \
    defined(configTASK_NOTIFICATION_ARRAY_ENTRIES) && configTASK_NOTIFICATION_ARRAY_ENTRIES > 0
#define DP_OSAL_PORT_HAS_INDEXED_TASK_NOTIFICATIONS 1
namespace dp::osal::detail {
/** @brief 现有任务的原生通知适配，不创建/拥有任务、信号量、队列或TCB通知数组。
 * @details 句柄仅借用；caller负责在任务寿命内串行注册/撤销，等待及产品deadline由caller管理。
 * 此类是CMSIS/FreeRTOS后端内部扩展，不是通用用户CRTP原语或USB角色实现。
 */
class TaskNotificationPort {
public:
    using Handle = void*;
    /** @brief 当前已配置的通知数组槽数；不要求所有产品新增槽位。 */
    static constexpr uint32_t kIndexCapacity = configTASK_NOTIFICATION_ARRAY_ENTRIES;
    /** @brief 一次ISR批量发送的opaque唤醒累计值；vendor表示仅在本后端私有存储内。
     * @details 每个ISR批次开始时默认构造一次，批内全部signal共享该值，不能跨批次复用。
     */
    class WakeState {
        friend class TaskNotificationPort;
        BaseType_t value_ = pdFALSE;
    };
    /** @brief 借用当前任务，NULL表示无可用任务；不延长其TCB寿命。 */
    static inline Handle current() noexcept { return xTaskGetCurrentTaskHandle(); }
    /** @brief 当前是否ISR；不会切换上下文。 */
    static inline bool inInterrupt() noexcept { return dp_osal_port_in_interrupt(); }
    /** @brief 保存IRQ状态并进入单核短临界区；须配对restore，不等待。 */
    static inline uint32_t saveInterruptMask() noexcept { return dp_osal_port_irq_save(); }
    /** @brief 恢复caller保存的原IRQ状态。 */
    static inline void restoreInterruptMask(uint32_t saved) noexcept { dp_osal_port_irq_restore(saved); }
    /** @brief 清给定任务的既有index状态/选定位。
     * @pre 仅任务上下文；caller保证handle存活、index<kIndexCapacity、bits非零及该index独占，
     * 并以所需IRQ临界区序列化借用注册/撤销，不把clear用作任务销毁证明。
     */
    static inline void clear(Handle handle, uint32_t index, uint32_t bits) noexcept {
        auto task = static_cast<TaskHandle_t>(handle);
        (void)xTaskNotifyStateClearIndexed(task, index);
        (void)ulTaskNotifyValueClearIndexed(task, index, bits);
    }
    /** @brief 非阻塞合并提示；ISR累计高优先级唤醒但不在caller临界区中yield。
     * @pre isr须等于当前inInterrupt()；handle须仍存活、index<kIndexCapacity、bits非零且index独占。
     * caller以IRQ临界区保护借用handle，wake属于当前ISR批次初始构造的唯一累计值。
     */
    static inline void signal(Handle handle, uint32_t index, uint32_t bits, bool isr, WakeState& wake) noexcept {
        auto task = static_cast<TaskHandle_t>(handle);
        if (isr)
            (void)xTaskNotifyIndexedFromISR(task, index, bits, eSetBits, &wake.value_);
        else
            (void)xTaskNotifyIndexed(task, index, bits, eSetBits);
    }
    /** @brief caller恢复先前IRQ掩码后，在ISR批次收尾请求必要调度。
     * @pre 仅ISR，wake是本批次累计值；必须先退出signal所在IRQ临界区，不能在持掩码期间yield。
     */
    static inline void yieldFromInterrupt(WakeState& wake) noexcept { portYIELD_FROM_ISR(wake.value_); }
    /** @brief 本任务按有限毫秒请求等选定位，向上换tick并避免portMAX_DELAY无限等待。
     * @pre 仅任务上下文；index<kIndexCapacity、bits非零且index独占；不能持有IRQ临界区等待。
     * @param index caller已独占的现有通知索引。
     * @param bits 等待/退出时清除的提示位；不是帧数、业务完成或peer就绪。
     * @param timeout_ms 相对有限等待请求；caller须先按原绝对deadline截取。
     * @note tick向上取整及任务调度可能晚于所请求毫秒值，不承诺硬实时上限。
     * @return 收到选定位为true；超时/无选定位为false，不消费其它索引。
     */
    static inline bool wait(uint32_t index, uint32_t bits, uint32_t timeout_ms) noexcept {
        uint32_t value = 0U;
        const uint64_t ticks64 = (static_cast<uint64_t>(timeout_ms) * configTICK_RATE_HZ + 999U) / 1000U;
        const TickType_t ticks = ticks64 >= portMAX_DELAY ? portMAX_DELAY - 1U : static_cast<TickType_t>(ticks64);
        return xTaskNotifyWaitIndexed(index, 0U, bits, &value, ticks) == pdTRUE && (value & bits) != 0U;
    }
};
}  // namespace dp::osal::detail
#endif
