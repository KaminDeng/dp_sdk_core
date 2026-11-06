#pragma once
#include <cstdint>
using osMutexId_t = void*;
using osStatus_t = int;
struct osMutexAttr_t {
    const char* name;
    uint32_t attr_bits;
    void* cb_mem;
    uint32_t cb_size;
};
constexpr uint32_t osMutexRecursive = 1, osMutexPrioInherit = 2, osWaitForever = 0xffffffffU;
constexpr int osOK = 0, osErrorISR = -6;
osMutexId_t osMutexNew(const osMutexAttr_t*);
osStatus_t osMutexDelete(osMutexId_t);
osStatus_t osMutexAcquire(osMutexId_t, uint32_t);
osStatus_t osMutexRelease(osMutexId_t);
uint32_t osKernelGetTickFreq();
