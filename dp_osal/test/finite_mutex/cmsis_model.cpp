/** @file cmsis_model.cpp @brief 实际Mutex后端的受控时钟/内核边界；检查不受NDEBUG裁掉。 */

#define DP_OSAL_DEBUG_H_
#define DP_OSAL_LOGD(...) ((void)0)
#define DP_OSAL_LOGE(...) ((void)0)
#include <cstdlib>
#define DP_OSAL_TEST_REQUIRE(value)                                                    \
    do {                                                                               \
        if (!(value)) {                                                                \
            std::fprintf(stderr, "检查失败: %s:%d: %s\n", __FILE__, __LINE__, #value); \
            std::abort();                                                              \
        }                                                                              \
    } while (false)
#include <initializer_list>
#include <cstdio>
#include "dp_osal_port.h"
uint32_t frequency = 1000, last_ticks = 0, calls = 0;
bool isr = false;
osMutexId_t osMutexNew(const osMutexAttr_t*) { return (void*)1; }
int osMutexDelete(void*) { return 0; }
int osMutexRelease(void*) { return 0; }
uint32_t osKernelGetTickFreq() { return frequency; }
int osMutexAcquire(void*, uint32_t ticks) {
    ++calls;
    last_ticks = ticks;
    return isr ? osErrorISR : osOK;
}
#include MUTEX_BACKEND
int main() {
    dp::osal::Mutex m;
    for (auto hz : {1U, 100U, 128U, 1000U, 10000U}) {
        frequency = hz;
        DP_OSAL_TEST_REQUIRE(m.tryLockForMs(1));
        DP_OSAL_TEST_REQUIRE(last_ticks == (hz + 999) / 1000);
    }
    frequency = 128;
    DP_OSAL_TEST_REQUIRE(m.tryLockFor(7));
    DP_OSAL_TEST_REQUIRE(last_ticks == 7);
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(1001));
    DP_OSAL_TEST_REQUIRE(last_ticks == 129);
    frequency = 0;
    auto n = calls;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(calls == n);
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(0));
    DP_OSAL_TEST_REQUIRE(last_ticks == 0);
    frequency = 1000;
    n = calls;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(0xffffffffU));
    DP_OSAL_TEST_REQUIRE(calls == n);
    frequency = 0xffffffffU;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(0xffffffffU));
    DP_OSAL_TEST_REQUIRE(calls == n);
    frequency = 1000;
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(0xfffffffeU));
    DP_OSAL_TEST_REQUIRE(last_ticks == 0xfffffffeU);
    frequency = 1000;
    isr = true;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(last_ticks == 1);
    puts("CMSIS 13 boundary groups PASS");
}
