
#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "cmsis_os2.h"
namespace snapshot_cmsis_fixture {
struct Row {
    void* handle;
    const char* name;
    uint32_t runtime_counter;
    uint32_t stack_hwm_bytes;
    uint8_t state;
    uintptr_t stack_pointer;
    uint32_t task_generation;
};

inline uint64_t wide = 0U;
inline uint32_t frequency = 128U, epoch = 1U, total = 0U, runtime = 0U, generation = 1U;
inline bool truncated = false, read_failed = false, zero = false, isr_ready = true;
inline std::atomic<bool> hold{false}, entered{false};
inline unsigned ends = 0U, reads = 0U, fail_at_read = 0U;
inline void (*on_begin)() = nullptr;
inline void begin_hook() {
    entered = true;
    if (on_begin != nullptr) {
        auto hook = on_begin;
        on_begin = nullptr;
        hook();
    }
    while (hold.load()) {
    }
}




inline size_t batch(Row* row, size_t, uint32_t* all, uint32_t* isr, uint8_t* valid, size_t* count) {
    begin_hook(); *count = zero ? 0U : truncated ? 2U : 1U;
    *all = total; *isr = 0U; *valid = isr_ready ? 1U : 0U;
    ++ends;
    if (zero) return 0U;
    *row = {reinterpret_cast<void*>(1U), "fixture", runtime, 128U, 0U, 0U, generation};
    return 1U;
}
}  // namespace snapshot_cmsis_fixture
#define DP_OSAL_PORT_HAS_RUNTIME_STATS 1
#define DP_OSAL_PORT_RUNTIME_ROW_TYPE snapshot_cmsis_fixture::Row
#define DP_OSAL_PORT_RUNTIME_MAX_THREADS 4U
#define DP_OSAL_PORT_RUNTIME_COUNTER_NOW() (static_cast<uint32_t>(snapshot_cmsis_fixture::wide))
#define DP_OSAL_PORT_RUNTIME_COUNTER_HZ() (snapshot_cmsis_fixture::frequency)
#if !defined(SNAPSHOT_FIXTURE_NO_WIDE)
#define DP_OSAL_PORT_RUNTIME_COUNTER_WIDE_NOW() (snapshot_cmsis_fixture::wide)
#define DP_OSAL_PORT_RUNTIME_COUNTER_EPOCH() (snapshot_cmsis_fixture::epoch)
#endif
#define DP_OSAL_PORT_RUNTIME_GET_THREADS(...) snapshot_cmsis_fixture::batch(__VA_ARGS__)
#define DP_OSAL_PORT_THREAD_STACK_POINTER_FROM_ID(id) (0U)
