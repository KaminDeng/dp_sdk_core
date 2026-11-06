#ifndef DP_OSAL_FINITE_ESP_FIXTURE_H
#define DP_OSAL_FINITE_ESP_FIXTURE_H
#pragma once
#include <cstdint>
using TickType_t = uint16_t;
using SemaphoreHandle_t = void*;
extern uint32_t frequency;
extern bool isr;
#define configTICK_RATE_HZ frequency
constexpr TickType_t portMAX_DELAY = 0xffff;
constexpr int pdTRUE = 1;
inline bool dp_osal_port_in_isr() { return isr; }
inline TickType_t dp_osal_port_ms_to_ticks(uint32_t ms) { return TickType_t((uint64_t(ms) * frequency + 999) / 1000); }
void* xSemaphoreCreateRecursiveMutex();
void vSemaphoreDelete(void*);
int xSemaphoreTakeRecursive(void*, TickType_t);
int xSemaphoreGiveRecursive(void*);

#endif
