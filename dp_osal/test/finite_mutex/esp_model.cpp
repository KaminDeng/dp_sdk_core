/** @file esp_model.cpp @brief 实际Mutex后端的受控时钟/内核边界；检查不受NDEBUG裁掉。 */

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
#include <cstdio>
#include <initializer_list>
#include "dp_osal_port.h"
uint32_t frequency = 1000, calls = 0;
TickType_t last_ticks = 0;
bool isr = false;
void* xSemaphoreCreateRecursiveMutex() { return (void*)1; }
void vSemaphoreDelete(void*) {}
int xSemaphoreGiveRecursive(void*) { return pdTRUE; }
int xSemaphoreTakeRecursive(void*, TickType_t ticks) {
    ++calls;
    last_ticks = ticks;
    return pdTRUE;
}
#include MUTEX_BACKEND
int main() {
    dp::osal::Mutex m;
    for (auto hz : {100U, 128U, 1000U, 10000U}) {
        frequency = hz;
        DP_OSAL_TEST_REQUIRE(m.tryLockForMs(1));
        DP_OSAL_TEST_REQUIRE(last_ticks == (hz + 999) / 1000);
    }
    frequency = 0;
    auto zero_calls = calls;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(calls == zero_calls);
    frequency = 1000;
    auto n = calls;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(65535));
    DP_OSAL_TEST_REQUIRE(calls == n);
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(65534));
    DP_OSAL_TEST_REQUIRE(last_ticks == 65534);
    n = calls;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(0xffffffff));
    DP_OSAL_TEST_REQUIRE(calls == n);
    isr = true;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(calls == n);
    isr = false;
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(0));
    DP_OSAL_TEST_REQUIRE(last_ticks == 0);
    DP_OSAL_TEST_REQUIRE(m.tryLockFor(2));
    DP_OSAL_TEST_REQUIRE(last_ticks == 2);
    puts("ESP 11 boundary groups PASS");
}
