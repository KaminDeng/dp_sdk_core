/** @file capability_model.cpp
 * @brief 真实FreeRTOS内部端口的能力编译与IRQ批次/有限tick合同；只替换内核叶函数。
 * @details 无indexed数组/API的内核不能因此产生通知能力；所有检查在NDEBUG下仍执行。
 */
#include <cstdio>
#include <cstdlib>
#include "dp_osal_freertos_thread_port.h"
#define DP_OSAL_TEST_REQUIRE(expr)                                     \
    do {                                                               \
        if (!(expr)) {                                                 \
            std::fprintf(stderr, "检查失败:%d:%s\n", __LINE__, #expr); \
            std::abort();                                              \
        }                                                              \
    } while (false)
int main() {
#if defined(FIXTURE_INDEXED_NOTIFICATIONS) && !defined(FIXTURE_NO_IRQ_HOOKS)
#ifndef DP_OSAL_PORT_HAS_INDEXED_TASK_NOTIFICATIONS
#error "有真实indexed API的后端应暴露能力"
#endif
    using Port = dp::osal::detail::TaskNotificationPort;
    static_assert(Port::kIndexCapacity == 2);
    auto task = Port::current();
    Port::WakeState wake;
    const auto saved = Port::saveInterruptMask();
    fixture_isr = true;
    Port::signal(task, 1, 4, true, wake);
    Port::signal(task, 1, 8, true, wake);
    DP_OSAL_TEST_REQUIRE(fixture_bits == 12 && fixture_yields == 0);
    Port::restoreInterruptMask(saved);
    Port::yieldFromInterrupt(wake);
    DP_OSAL_TEST_REQUIRE(fixture_yields == 1);
    fixture_isr = false;
    DP_OSAL_TEST_REQUIRE(Port::wait(1, 4, 1));
    DP_OSAL_TEST_REQUIRE(fixture_ticks == 1 && fixture_bits == 8);
    DP_OSAL_TEST_REQUIRE(!Port::wait(1, 4, 0));
    DP_OSAL_TEST_REQUIRE(fixture_ticks == 0 && fixture_bits == 8);
    DP_OSAL_TEST_REQUIRE(Port::wait(1, 8, UINT32_MAX));
    DP_OSAL_TEST_REQUIRE(fixture_ticks == UINT16_MAX - 1 && fixture_bits == 0);
    fixture_bits = 12;
    Port::clear(task, 1, 4);
    DP_OSAL_TEST_REQUIRE(fixture_bits == 8 && fixture_calls == 1);
    Port::signal(task, 1, 4, false, wake);
    DP_OSAL_TEST_REQUIRE(fixture_bits == 12);
#else
#ifdef DP_OSAL_PORT_HAS_INDEXED_TASK_NOTIFICATIONS
#error "旧内核只有单通知能力，不能声明不存在的indexed API"
#endif
#endif
    std::puts("通知端口能力与资源边界PASS");
}
