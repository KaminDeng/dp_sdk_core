#pragma once
#include "FreeRTOS.h"
enum eTaskState { eSuspended };
inline eTaskState eTaskGetState(TaskHandle_t) { return eSuspended; }
inline void vTaskDelay(TickType_t) {}
inline void vTaskDelete(TaskHandle_t) {}
inline void vTaskSuspend(TaskHandle_t) {}
#ifdef FIXTURE_INDEXED_NOTIFICATIONS
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return &fixture_calls; }
inline BaseType_t xTaskNotifyStateClearIndexed(TaskHandle_t, uint32_t) {
    ++fixture_calls;
    return pdTRUE;
}
inline uint32_t ulTaskNotifyValueClearIndexed(TaskHandle_t, uint32_t, uint32_t bits) {
    fixture_bits &= ~bits;
    return 0;
}
inline BaseType_t xTaskNotifyIndexed(TaskHandle_t, uint32_t, uint32_t bits, int) {
    fixture_bits |= bits;
    return pdTRUE;
}
inline BaseType_t xTaskNotifyIndexedFromISR(TaskHandle_t, uint32_t, uint32_t bits, int, BaseType_t* wake) {
    fixture_bits |= bits;
    *wake = pdTRUE;
    return pdTRUE;
}
inline BaseType_t xTaskNotifyWaitIndexed(uint32_t, uint32_t, uint32_t clear, uint32_t* value, TickType_t ticks) {
    fixture_ticks = ticks;
    *value = fixture_bits;
    fixture_bits &= ~clear;
    return *value ? pdTRUE : pdFALSE;
}
constexpr int eSetBits = 1;
#endif
