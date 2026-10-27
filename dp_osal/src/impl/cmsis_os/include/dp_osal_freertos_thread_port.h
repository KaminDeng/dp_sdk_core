/** @file dp_osal_freertos_thread_port.h
 * @brief FreeRTOS CMSIS线程完成与静态资源回收机制，不包含产品任务策略。
 */
#pragma once
#include <cstdint>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"

#define DP_OSAL_PORT_THREAD_JOINABLE_ATTR 0U
#define DP_OSAL_PORT_THREAD_REAP(handle) dp_osal_freertos_thread_reap(handle)
#define DP_OSAL_PORT_THREAD_FINISH() dp_osal_freertos_thread_finish()
#define DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES(attr) dp_osal_freertos_thread_resources_valid(attr)

inline bool dp_osal_freertos_thread_resources_valid(const osThreadAttr_t& attr) noexcept {
    if (attr.stack_mem == nullptr) return attr.cb_mem == nullptr && attr.cb_size == 0U;
    if ((attr.stack_size % sizeof(StackType_t)) != 0U || attr.cb_mem == nullptr ||
        attr.cb_size < sizeof(StaticTask_t)) return false;
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
