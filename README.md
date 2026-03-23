# osal — OS Abstraction Layer

A C++17 library that provides a portable, RTOS-agnostic interface for threading,
synchronization, memory, and timing primitives. Write your application code once
against 13 abstract interfaces; swap the backend (POSIX or CMSIS-OS2) by editing
one configuration file.

---

## Quick Start

### Step 1 — Copy the port template

```sh
cp osal/port/template/osal_port.h  my_project/my_port/osal_port.h
```

### Step 2 — Fill in `osal_port.h`

Open `my_port/osal_port.h` and complete the five sections (see reference below).

### Step 3 — Point CMake at your port

In your `CMakeLists.txt`, **before** `add_subdirectory(osal)`:

```cmake
set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
add_subdirectory(path/to/osal)
target_link_libraries(my_app PRIVATE osal)
```

That's it. No other files are needed.

---

## `osal_port.h` Reference

`osal_port.h` is the single file that configures the library for your platform.
It has five sections:

### Section 1 — Backend Selection

Uncomment **exactly one** line. Defining both is undefined behaviour.

```cpp
#define OSAL_BACKEND_POSIX      // std::thread, pthread, std::mutex, std::chrono
// #define OSAL_BACKEND_CMSIS_OS  // osThreadNew, osMutexNew, osTimerNew, …
```

| Backend | Use when |
|---------|----------|
| `OSAL_BACKEND_POSIX` | Linux, macOS, any POSIX host |
| `OSAL_BACKEND_CMSIS_OS` | FreeRTOS+CMSIS-OS2, Zephyr, ThreadX, … |

### Section 2 — Platform Includes

```cpp
// POSIX — leave empty; osal uses std:: and POSIX internally.

// CMSIS-OS2 — include the header your RTOS provides:
#include "cmsis_os2.h"   // FreeRTOS+CMSIS-OS2 (from your BSP)
// #include <cmsis_os2.h>  // Zephyr built-in compatibility layer
// #include "tx_cmsis.h"   // ThreadX CMSIS-OS2
```

### Section 3 — Platform Constants

```cpp
// Minimum thread stack in bytes.
//   POSIX:    PTHREAD_STACK_MIN (typically 8192–16384)
//   FreeRTOS: configMINIMAL_STACK_SIZE * sizeof(StackType_t)
//   Zephyr:   512
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#define OSAL_PORT_THREAD_MIN_STACK_SIZE  4096
#endif

// Default thread priority.
//   POSIX:    0  (priority is not meaningfully enforced without root)
//   CMSIS-OS: osPriorityNormal
#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY  0
#endif
```

### Section 4 — Debug Output

Implement `osal_port_debug_write` as an **inline** function (required to avoid
ODR link errors when the header is included by multiple translation units):

```cpp
#include <cstdint>

// POSIX / Linux / macOS — write to stdout:
#include <unistd.h>
inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// STM32 HAL UART — adjust handle and header to match your BSP:
// #include "usart.h"
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     HAL_UART_Transmit(&huart6, reinterpret_cast<const uint8_t*>(buf),
//                       static_cast<uint16_t>(len), HAL_MAX_DELAY);
// }

// Zephyr printk:
// #include <zephyr/sys/printk.h>
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     printk("%.*s", static_cast<int>(len), buf);
// }
```

### Section 5 — Test Feature Flags

```cpp
// OSAL_TEST_ALL = 1  → run every test component (recommended for first bring-up)
// OSAL_TEST_ALL = 0  → use per-component flags below for fine control
#define OSAL_TEST_ALL 1

#if !OSAL_TEST_ALL
#define OSAL_TEST_THREAD_ENABLED              1
#define OSAL_TEST_CHRONO_ENABLED              1
#define OSAL_TEST_MUTEX_ENABLED               1
#define OSAL_TEST_CONDITION_VARIABLE_ENABLED  1
#define OSAL_TEST_LOCKGUARD_ENABLED           1
#define OSAL_TEST_MEMORY_MANAGER_ENABLED      1
#define OSAL_TEST_QUEUE_ENABLED               1
#define OSAL_TEST_SEMAPHORE_ENABLED           1
#define OSAL_TEST_SPINLOCK_ENABLED            1
#define OSAL_TEST_RWLOCK_ENABLED              1
#define OSAL_TEST_TIMER_ENABLED               1
#define OSAL_TEST_THREAD_POOL_ENABLED         1
#endif
```

---

## Built-in POSIX Port (zero configuration)

If `OSAL_PORT_DIR` is **not set**, osal automatically uses
`port/builtin_posix/osal_port.h` — a ready-to-use POSIX port that writes debug
output to stdout and enables all tests. No configuration is required.

```cmake
# Minimum CMake integration for Linux / macOS (no OSAL_PORT_DIR needed):
add_subdirectory(path/to/osal)
target_link_libraries(my_app PRIVATE osal)
```

---

## Port Examples

The `example/` directory contains three complete, working ports:

| Directory | Platform | Backend | Debug output |
|-----------|----------|---------|--------------|
| `osal_port_freertos/` | STM32 bare-metal + FreeRTOS | CMSIS-OS2 | `HAL_UART_Transmit` on UART6 |
| `osal_port_sim_freertos/` | FreeRTOS-on-POSIX simulator | CMSIS-OS2 | `write(1, …)` (stdout) |
| `osal_port_zephyr/` | Zephyr RTOS | CMSIS-OS2 (built-in) | `printk` |

Each example directory contains only `osal_port.h` (and, for the simulator, a
`cmsis_os2_freertos/` adapter). Copy the directory that is closest to your target
and adapt it.

---

## Resource Consumption on Microcontrollers

This section quantifies the RAM, Flash, and runtime overhead of using the OSAL
abstraction layer compared to calling FreeRTOS or CMSIS-OS2 APIs directly.
All figures assume a 32-bit Cortex-M target with the CMSIS-OS2 backend.

### RAM — extra bytes per object instance

| Component | Direct OS handle | OSAL object | Extra RAM |
|-----------|-----------------|-------------|-----------|
| `OSALMutex` | 4 B (`osMutexId_t`) | 8 B | **+4 B** (vtable ptr) |
| `OSALSemaphore` | 4 B | 8 B | **+4 B** |
| `OSALThread` | 4 B (`osThreadId_t`) | ≥ 48 B | **+44 B** |
| `OSALConditionVariable` | ~12 B (hand-rolled) | 12 B | ~0 B |
| `OSALRWLock` | ~20 B (hand-rolled) | 20 B | ~0 B |
| `OSALMessageQueue<T>` | 4 B | 8 B | **+4 B** |
| `OSALLockGuard` | 0 B (stack) | 12 B (stack) | **+12 B** (stack) |
| `OSALSpinLock` | 4 B | 8 B | **+4 B** |

**`OSALThread` dominates the overhead (+44 B).**
The extra cost comes from:
- `std::function<void(void*)>` task functor — 16–32 B in-object buffer;
  lambdas capturing more than ~16 B spill to **heap allocation** (fragmentation
  risk on MCUs with tight heap).
- One additional `osSemaphoreId_t` (`exitSemaphore`) per thread instance for
  `join()` support — allocates one FreeRTOS semaphore object in the heap.
- Two `std::atomic<bool>` fields (8 B total).

### Flash — code-size additions

| Source | Estimated size |
|--------|---------------|
| vtable per component (12 components) | ~60 B each → **~720 B total** |
| `std::function` template instantiation per unique lambda signature | 200–500 B each |
| C++ runtime (constructors, destructors) | **2–4 KB** |
| **Total OSAL overhead (typical, with `-Os`)** | **~4–8 KB Flash** |

### Runtime — extra cycles per API call

Every OSAL call goes through one level of virtual dispatch:
1. Load vtable pointer from the object.
2. Load function pointer from the vtable array.
3. Execute indirect branch (BLX).

This adds approximately **3–5 CPU cycles** per call. For mutex / semaphore
operations (which typically spend hundreds of cycles in the OS scheduler), the
overhead is **< 1 %** and negligible in practice.

**Exception — `OSALRWLock`:** A single `readLock()` issues up to 5 CMSIS-OS2
calls (1 mutex + 2 semaphore operations + 2 releases) compared to 1–2 for a
direct implementation. Prefer `OSALMutex` in read-heavy scenarios on
resource-constrained MCUs.

### MCU Feasibility Summary

| Target | RAM | Feasibility | Notes |
|--------|-----|-------------|-------|
| Cortex-M4 / M7, ≥ 64 KB RAM | — | ✅ **Recommended** | Full feature set; `std::function` overhead is acceptable |
| Cortex-M3, 32–64 KB RAM | — | ⚠️ **Use with care** | Limit `OSALThread` instances; avoid lambdas with large capture lists |
| Cortex-M0 / M0+, ≤ 16 KB RAM | — | ❌ **Not recommended** | `std::function` + C++ runtime overhead consumes a disproportionate share of available Flash and RAM |

**Practical guidelines for MCU use:**
- Enable `-Os` (optimize for size) and `-fno-exceptions -fno-rtti` to strip
  C++ exception and RTTI machinery (saves 1–3 KB Flash).
- Keep lambda capture lists small (< 16 B) to avoid heap allocation inside
  `OSALThread`.
- Use `OSALMutex` instead of `OSALRWLock` unless you genuinely need concurrent
  readers; `OSALRWLock` uses three OS objects per instance.
- `OSALSemaphore::init()` must only be called when **no threads are blocked**
  on the semaphore; calling it on a live semaphore is undefined behaviour.

---

| Interface header | Abstraction | Key operations |
|-----------------|-------------|----------------|
| `interface_thread.h` | `IThread` | `start`, `stop`, `join`, `detach`, `suspend`, `resume`, `setPriority` |
| `interface_mutex.h` | `IMutex` | `lock`, `unlock`, `tryLock`, `tryLockFor` |
| `interface_condition_variable.h` | `IConditionVariable` | `wait`, `waitFor`, `notifyOne`, `notifyAll` |
| `interface_lockguard.h` | `ILockGuard` | RAII mutex guard |
| `interface_rwlock.h` | `IRWLock` | read/write lock with try and timed variants |
| `interface_semaphore.h` | `ISemaphore` | counting semaphore |
| `interface_spin_lock.h` | `ISpinLock` | spinlock (atomic_flag) |
| `interface_queue.h` | `MessageQueue<T>` | `send`, `receive`, `tryReceive`, `receiveFor`, `size`, `clear` |
| `interface_memory_manager.h` | `IMemoryManager` | block pool: `allocate`, `deallocate`, `allocateAligned` |
| `interface_timer.h` | `ITimer` | one-shot and periodic timers |
| `interface_thread_pool.h` | `IThreadPool` | dynamic thread pool with task queue |
| `interface_chrono.h` | `IChrono` | monotonic clock: `elapsed`, `to_time_t`, `from_time_t` |
| `interface_system.h` | `ISystem` | `schedulerStart`, `sleep_ms`, `sleep_s` |

All interfaces are pure-virtual (no implementation dependency at include time).
The concrete implementation is selected at link time via the backend chosen in
`osal_port.h`.

---

## Backend Selection

```
src/impl/posix/     ← OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← OSAL_BACKEND_CMSIS_OS
```

CMake auto-detects the active backend by scanning `osal_port.h` for an
uncommented `#define OSAL_BACKEND_CMSIS_OS` line and compiles the matching
source tree. Only one backend is compiled per build.

For CMSIS-OS ports that depend on additional platform libraries (e.g.
`freertos_kernel`), link them externally:

```cmake
target_link_libraries(osal PUBLIC freertos_kernel cmsis_os2_impl)
```

---

## Running Tests

Tests are compiled into the `osal` static library itself (Google Test, 12 test
suites). They run inside the host application's task/thread context — there is
no standalone test binary.

To run all tests on a POSIX host:

```sh
cmake -B build -S path/to/host_project
cmake --build build
./build/posix_demo          # Linux / direct terminal
```

On **macOS**, stdout is fully buffered when not attached to a TTY. Use
`script(1)` to allocate a pseudo-TTY so output is flushed:

```sh
script -q /tmp/osal_out.txt ./build/posix_demo 2>/dev/null &
sleep 10 && kill %1
cat /tmp/osal_out.txt
```

To skip a test suite, set the corresponding `OSAL_TEST_*_ENABLED` flag to `0`
in your `osal_port.h` (requires `OSAL_TEST_ALL 0`).
