#pragma once
#include <cstdint>
#include <cstdlib>
#define configUSE_TASK_NOTIFICATIONS 1
#define configTICK_RATE_HZ 128U
#ifdef FIXTURE_NO_IRQ_HOOKS
#define DP_OSAL_PORT_NATIVE_IRQ_HOOKS 0
#else
#define DP_OSAL_PORT_NATIVE_IRQ_HOOKS 1
#endif
#ifdef FIXTURE_INDEXED_NOTIFICATIONS
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2U
#endif
using BaseType_t = int;
using TickType_t = uint16_t;
using StackType_t = uint32_t;
struct StaticTask_t {
    uint32_t storage[4];
};
using TaskHandle_t = void*;
using osThreadId_t = void*;
using osStatus_t = int;
constexpr int osOK = 0, pdFALSE = 0, pdTRUE = 1;
constexpr TickType_t portMAX_DELAY = UINT16_MAX;
struct osThreadAttr_t {
    void* stack_mem;
    uint32_t stack_size;
    void* cb_mem;
    uint32_t cb_size;
};
inline bool fixture_isr = false, fixture_masked = false;
inline uint32_t fixture_bits = 0, fixture_ticks = 0, fixture_calls = 0, fixture_yields = 0;
inline bool dp_osal_port_in_interrupt() { return fixture_isr; }
inline uint32_t dp_osal_port_irq_save() {
    bool old = fixture_masked;
    fixture_masked = true;
    return old;
}
inline void dp_osal_port_irq_restore(uint32_t old) { fixture_masked = old != 0; }
inline void fixture_yield(BaseType_t wake) {
    if (fixture_masked || !fixture_isr) std::abort();
    fixture_yields += wake != 0;
}
#define portYIELD_FROM_ISR(value) fixture_yield(value)
