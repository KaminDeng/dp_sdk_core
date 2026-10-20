/** @file dp_osal_snapshot_cmsis_host_tests.cpp
 * @brief 真实CMSIS后端头的Host模型回归；不取得真实RTOS运行资格。
 */
#include "dp_osal_port.h"
#include "dp_osal_system.h"
#include "dp_osal_chrono.h"
#include "dp_osal_virtual.h"
#include <cassert>
#include <thread>
#include <cstdio>
namespace fixture = snapshot_cmsis_fixture;
static uint32_t kernel_frequency = 128U, kernel_tick = 0U, delayed_until = 0U;
extern "C" uint32_t osKernelGetTickFreq(void) { return kernel_frequency; }
extern "C" uint32_t osKernelGetTickCount(void) { return kernel_tick; }
extern "C" osStatus_t osDelayUntil(uint32_t tick) {
    delayed_until = tick;
    return osOK;
}
static dp::osal::ThreadSnapshot rows[4]{};
static size_t sample(uint64_t step = 128U) {
    fixture::reads = 0U;
    fixture::wide += step;
    fixture::total = static_cast<uint32_t>(fixture::wide);
    fixture::runtime += 16U;
    return dp::osal::System::getInstance().get_thread_snapshot(rows, 4U);
}
static size_t nested_count = 99U;
static void nested() {
    dp::osal::ThreadSnapshot output[4]{};
    nested_count = dp::osal::System::getInstance().get_thread_snapshot(output, 4U);
}
int main() {
    auto& backend = dp::osal::Chrono::getInstance();
    dp::osal::ChronoVirtual<dp::osal::Chrono> clock(backend);
    for (uint32_t hz : {93U, 128U, 1024U, 1001U, 128000U}) {
        kernel_frequency = hz;
        assert(clock.ticksPerSecond() == hz);
    }
    kernel_tick = 100U;
    delayed_until = 0U;
    clock.sleepUntil(99U);
    assert(delayed_until == 0U);
    clock.sleepUntil(101U);
    assert(delayed_until == 101U);
    kernel_tick = UINT32_MAX - 2U;
    clock.sleepUntil(2U);
    assert(delayed_until == 2U);
    assert(sample() == 1U);
    assert(sample() == 1U);
#if !defined(SNAPSHOT_FIXTURE_NO_WIDE)
    assert(rows[0].runtime_valid == 1U && rows[0].runtime_us == 125000U && rows[0].sample_window_us == 1000000U);
#else
    assert(rows[0].runtime_valid == 0U && rows[0].isr_valid == 0U);
#endif
    fixture::isr_ready = false;
    assert(sample() == 1U);
    assert(rows[0].isr_valid == 0U);
    fixture::isr_ready = true;
    assert(sample() == 1U);
    assert(rows[0].isr_valid == 0U);
    assert(sample() == 1U);
#if !defined(SNAPSHOT_FIXTURE_NO_WIDE)
    assert(rows[0].isr_valid == 1U);
#endif
    fixture::on_begin = nested;
    assert(sample() == 1U);
    assert(nested_count == 0U);
    fixture::hold = true;
    fixture::entered = false;
    std::thread worker([] {
        dp::osal::ThreadSnapshot output[4]{};
        (void)dp::osal::System::getInstance().get_thread_snapshot(output, 4U);
    });
    while (!fixture::entered.load()) std::this_thread::yield();
    assert(dp::osal::System::getInstance().get_thread_snapshot(rows, 4U) == 0U);
    fixture::hold = false;
    worker.join();
#if defined(SNAPSHOT_FIXTURE_STREAM)
    for (unsigned phase : {1U, 2U, 3U}) {
        fixture::fail_at_read = phase;
        const unsigned ends_before = fixture::ends;
        assert(sample() == 0U);
        assert(fixture::ends == ends_before + 1U);
        fixture::fail_at_read = 0U;
        assert(sample() == 1U);
        assert(rows[0].runtime_valid == 0U);
        assert(sample() == 1U);
    }
#endif
    fixture::zero = true;
    assert(sample() == 0U);
    fixture::zero = false;
    assert(sample() == 1U);
    assert(rows[0].runtime_valid == 0U);
    assert(sample() == 1U);
    fixture::truncated = true;
    assert(sample() == 1U);
    assert(rows[0].validity == static_cast<uint8_t>(dp::osal::MeasurementValidity::kSampleTruncated));
    fixture::truncated = false;
    assert(sample() == 1U);
    assert(rows[0].runtime_valid == 0U);
    assert(sample() == 1U);
    fixture::generation++;
    assert(sample() == 1U);
    assert(rows[0].runtime_valid == 0U);
    assert(sample() == 1U);
    assert(sample((uint64_t{1U} << 32U) + 128U) == 1U);
    assert(rows[0].runtime_valid == 0U && rows[0].isr_valid == 0U);
    fixture::epoch++;
    assert(sample() == 1U);
    assert(rows[0].runtime_valid == 0U);
    fixture::frequency = 1U;
    assert(sample(5000U) == 1U);
    assert(rows[0].runtime_valid == 0U);
    assert(sample(5000U) == 1U);
    assert(rows[0].runtime_valid == 0U);
    assert(fixture::ends > 0U);
    std::puts("CMSIS快照Host模型：非1000Hz/ChronoVirtual、门控/早退、代际/截断/多圈/epoch/溢出通过");
}
