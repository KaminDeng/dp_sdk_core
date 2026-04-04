# dp_sdk_core

Unified core abstraction layer for embedded systems. Write portable C++17
application code once, swap the OS backend and hardware target by editing
configuration files.

Contains three modules:

| Module | What it does | Design |
|--------|-------------|--------|
| **osal** | OS primitives (thread, mutex, queue, timer, …) | Zero-cost CRTP + backend selection |
| **hal** | Hardware peripherals (UART, GPIO, SPI, I2C, …) | Zero-cost CRTP templates |
| **device** | Device registry and lifecycle | Adapter pattern + singleton manager |

**Dependency chain:** `device → hal (public) + osal (private)` · `hal` and `osal` are independent of each other.

---

## Quick Start

dp_sdk_core is consumed by the parent project (dpsdk) via `add_subdirectory()`.
It is not built standalone.

```cmake
# In your top-level CMakeLists.txt:
set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)   # optional, defaults to builtin_posix
add_subdirectory(core/dp_sdk_core)

target_link_libraries(my_app PRIVATE osal)       # OS abstraction
target_link_libraries(my_app PRIVATE dp_hal)     # hardware abstraction
target_link_libraries(my_app PRIVATE dp_device)  # device management
```

Build from the parent dpsdk directory:

```sh
cmake -B build -DPRODUCT=posix_demo && cmake --build build
```

---

## Module 1 — osal (OS Abstraction Layer)

### Interfaces

| Header | Abstraction | Key operations |
|--------|-------------|----------------|
| `interface_thread.h` | `ThreadBase<Impl>` | `start`, `stop`, `join`, `detach`, `suspend`, `resume`, `setPriority` |
| `interface_mutex.h` | `MutexBase<Impl>` | `lock`, `unlock`, `tryLock`, `tryLockFor` |
| `interface_condition_variable.h` | `ConditionVariableBase<Impl>` | `wait`, `waitFor`, `notifyOne`, `notifyAll` |
| `interface_lockguard.h` | `LockGuard<MutexType>` | RAII mutex guard |
| `interface_rwlock.h` | `RWLockBase<Impl>` | read/write lock with try and timed variants |
| `interface_semaphore.h` | `SemaphoreBase<Impl>` | counting semaphore |
| `interface_spin_lock.h` | `SpinLockBase<Impl>` | spinlock (atomic_flag) |
| `interface_queue.h` | `QueueBase<Impl, T>` | `send`, `receive`, `tryReceive`, `receiveFor`, `size`, `clear` |
| `interface_memory_manager.h` | `MemoryManagerBase<Impl>` | block pool: `allocate`, `deallocate`, `allocateAligned` |
| `interface_timer.h` | `TimerBase<Impl>` | one-shot and periodic timers |
| `interface_thread_pool.h` | `ThreadPoolBase<Impl>` | dynamic thread pool with task queue |
| `interface_chrono.h` | `ChronoBase<Impl>` | monotonic clock: `elapsed`, `to_time_t`, `from_time_t` |
| `interface_system.h` | `SystemBase<Impl>` | `schedulerStart`, `sleep_ms`, `sleep_s` |
| `osal_virtual.h` | `IMutex` + `MutexVirtual<T>` (and others) | optional host-only virtual wrappers for injection/GMock |

CRTP interfaces have no vtable overhead. The concrete implementation is selected
at compile/link time via the backend chosen in `osal_port.h`.

### Backend Selection

```
src/impl/posix/     ← OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← OSAL_BACKEND_CMSIS_OS
```

CMake auto-detects the active backend by scanning `osal_port.h` for an
uncommented `#define OSAL_BACKEND_CMSIS_OS` line and compiles the matching
source tree. Only one backend is compiled per build.

| Backend | Use when |
|---------|----------|
| `OSAL_BACKEND_POSIX` | Linux, macOS, any POSIX host |
| `OSAL_BACKEND_CMSIS_OS` | FreeRTOS+CMSIS-OS2, Zephyr, ThreadX, … |

### OSAL Port System

Set `OSAL_PORT_DIR` before `add_subdirectory(osal)`. Defaults to
`port/builtin_posix/` (zero configuration on Linux/macOS).

`osal_port.h` has five sections:

| Section | Content |
|---------|---------|
| 1 — Backend Selection | `#define OSAL_BACKEND_POSIX` or `OSAL_BACKEND_CMSIS_OS` (exactly one) |
| 2 — Platform Includes | CMSIS-OS: `#include "cmsis_os2.h"` (POSIX: leave empty) |
| 3 — Platform Constants | `OSAL_PORT_THREAD_MIN_STACK_SIZE`, `OSAL_PORT_THREAD_DEFAULT_PRIORITY` |
| 4 — Debug Output | `inline void osal_port_debug_write(const char* buf, uint32_t len)` |
| 5 — Test Feature Flags | `OSAL_TEST_ALL` or per-component `OSAL_TEST_*_ENABLED` flags |

### Port Examples

| Directory | Platform | Backend | Debug output |
|-----------|----------|---------|--------------|
| `osal_port_freertos/` | STM32 bare-metal + FreeRTOS | CMSIS-OS2 | `HAL_UART_Transmit` on UART6 |
| `osal_port_sim_freertos/` | FreeRTOS-on-POSIX simulator | CMSIS-OS2 | `write(1, …)` (stdout) |
| `osal_port_zephyr/` | Zephyr RTOS | CMSIS-OS2 (built-in) | `printk` |

Copy the example closest to your target and adapt it.

---

## Module 2 — hal (Hardware Abstraction Layer)

### CRTP Interfaces

All interfaces are header-only templates with zero runtime overhead:

| Header | Interface | Description |
|--------|-----------|-------------|
| `dp_uart.h` | `UartBase<Impl>` | UART: configure, read, write, flush, RX/TX callbacks, DMA |
| `dp_gpio.h` | `GpioPinBase<Impl>` | GPIO: setMode, read, write, toggle, IRQ with edge triggers |
| `dp_spi.h` | `SpiBusBase<Impl>` + `SpiDevice<Bus,Cs>` | SPI bus + slave device with auto CS toggle |
| `dp_i2c.h` | `I2cBusBase<Impl>` + `I2cDevice<Bus>` | I2C bus + register read/write helpers |
| `dp_adc.h` | `AdcBase<Impl>` | ADC: configure, single-shot read, continuous mode |
| `dp_dac.h` | `DacBase<Impl>` | DAC: configure, write output |
| `dp_can.h` | `CanBase<Impl>` | CAN bus: configure, send, receive, filters |
| `dp_timer.h` | `TimerBase<Impl>` | Timer: start, stop, periodic callbacks, counter |
| `dp_pwm.h` | `PwmBase<Impl>` | PWM: start, stop, frequency and duty cycle control |
| `dp_hal_power.h` | `PowerManageable<Impl>` | Optional mixin for power management |
| `dp_hal_virtual.h` | `IUart`, `IGpioPin`, … | Virtual wrappers for dependency injection / GMock |

### CRTP Pattern

```cpp
template <typename Impl>
class UartBase {
public:
    Status write(const uint8_t* buf, size_t len) { return impl().doWrite(buf, len); }
private:
    Impl& impl() { return *static_cast<Impl*>(this); }
};
```

Implementations provide private `do*` methods and declare
`friend class UartBase<Self>`.

### HAL Port System

Port resolution (3-tier fallback):
1. `platform/<KERNEL_PORT>/hal_port/` — product-specific
2. `platform/common/hal_port/` — shared default
3. `hal/port/mock/` — built-in mock for host testing

Each port provides:
- `hal_port.h` — typedef bindings (e.g. `using DpUart = mock::MockUart;`)
- Implementation headers (e.g. `mock_uart.h`, `posix_uart.h`)
- `dp_hal_port_impl.cpp` — port functions (`dp_hal_time_us`, `dp_hal_log`, `dp_hal_assert_fail`)
- `CMakeLists.txt` — builds `dp_hal_port_impl` static target

---

## Module 3 — device (Device Management)

### Architecture

- **Device** base class: name (max 32 chars) + type + open/close refcount
- **Adapters** bridge HAL to Device with virtual interfaces:

| Adapter | HAL Interface | DeviceType |
|---------|---------------|------------|
| `SerialDevice<HalImpl>` | `IUart` | `kSerial` |
| `PinDevice<HalImpl>` | `IGpioPin` | `kPin` |
| `SpiBusDevice<HalImpl>` | `ISpi` | `kSpiBus` |
| `I2cBusDevice<HalImpl>` | `II2c` | `kI2cBus` |
| `AdcDevice<HalImpl>` | `IAdc` | `kAdc` |
| `DacDevice<HalImpl>` | `IDac` | `kDac` |
| `TimerDevice<HalImpl>` | `ITimer` | `kTimer` |

- **DeviceManager** singleton: thread-safe registry (OSAL Mutex), fixed array
  (max 32 devices, no heap), name-based and type-based lookup. No RTTI — uses
  `DeviceType` enum + `static_cast`.

---

## Running Tests

Tests are compiled into static libraries and run inside the host application's
task context (no standalone test binaries). Entry points:

```c
extern "C" void osal_test_main(void);      // 12 OSAL suites
extern "C" void dp_hal_test_main(void);    // 10 HAL suites  (requires -DDP_HAL_BUILD_TESTS=ON)
extern "C" void dp_device_test_main(void); // 5 Device suites (requires -DDP_DEVICE_BUILD_TESTS=ON)
```

Build and run from the parent dpsdk directory:

```sh
cmake -B build -DPRODUCT=posix_demo -DDP_HAL_BUILD_TESTS=ON -DDP_DEVICE_BUILD_TESTS=ON
cmake --build build
./build/dpsdk_firmware
```

### Test Controls (OSAL)

In `osal_port.h`:
- `OSAL_TEST_ALL = 1` — run all OSAL suites (recommended for first bring-up)
- `OSAL_TEST_ALL = 0` — use per-component flags (`OSAL_TEST_THREAD_ENABLED`, …)

### Host Injection Example (OSAL)

`osal/src/interface/osal_virtual.h` provides virtual wrappers on top of CRTP
objects for host-only dependency injection:

```cpp
class CounterService {
public:
    explicit CounterService(osal::IMutex& m) : mutex_(m) {}
    bool increment() {
        if (!mutex_.lock()) return false;
        ++value_;
        (void)mutex_.unlock();
        return true;
    }
private:
    osal::IMutex& mutex_;
    int value_ = 0;
};

FakeMutex fake;
osal::MutexVirtual<FakeMutex> injected(fake);
CounterService svc(injected);  // service depends on IMutex abstraction
```

### Single Translation Unit Pattern

All test `.cpp` files are `#include`d into a single `*_test_main.cpp` per module
to prevent linker dead-stripping of GTest static registrations. Never add test
files to CMake sources directly.

On **macOS**, stdout is fully buffered when not attached to a TTY. Use
`script(1)` to allocate a pseudo-TTY:

```sh
script -q /tmp/out.txt ./build/dpsdk_firmware 2>/dev/null &
sleep 10 && kill %1 && cat /tmp/out.txt
```

---

## Resource Consumption on Microcontrollers

All figures assume a 32-bit Cortex-M target with the CMSIS-OS2 backend.
They are split into:
- per-object structural overhead (OSAL itself), and
- whole-firmware measurements from the current dpsdk product profile.

### RAM — extra bytes per OSAL object instance

| Component | Direct OS handle | OSAL object | Extra RAM |
|-----------|-----------------|-------------|-----------|
| `OSALMutex` | 4 B (`osMutexId_t`) | 4 B | **+0 B** |
| `OSALSemaphore` | 4 B | 4 B | **+0 B** |
| `OSALThread` | 4 B (`osThreadId_t`) | ≥ 44 B | **+40 B** |
| `OSALConditionVariable` | ~12 B | 12 B | ~0 B |
| `OSALRWLock` | ~20 B | 20 B | ~0 B |
| `OSALMessageQueue<T>` | 4 B | 4 B | **+0 B** |
| `OSALLockGuard` | 0 B (stack) | 8 B (stack) | **+8 B** (stack) |
| `OSALSpinLock` | 4 B | 4 B | **+0 B** |

**`OSALThread` dominates the overhead (+40 B)** due to `std::function` (16–32 B
in-object buffer; captures > ~16 B spill to heap), `exitSemaphore` for `join()`,
and two `std::atomic<bool>` fields.

HAL interfaces add **zero overhead** — CRTP methods inline away entirely.

### Flash — code-size additions

| Source | Estimated size |
|--------|---------------|
| vtable per OSAL component | **0 B** (CRTP removes virtual dispatch) |
| `std::function` per lambda signature | 200–500 B each |
| C++ runtime (ctors, dtors) | **2–4 KB** |
| **Total OSAL overhead (typical, `-Os`)** | **~4–8 KB Flash** |

### Measured footprint (`dp_stm32f427_dev`, 2026-04-05)

Build profile:
- `PRODUCT=dp_stm32f427_dev`
- `PRODUCT_APP=all_tests`
- overlays include `test_all` (debug-oriented, not production trimming)
- tool output: `arm-none-eabi-size build_stm32_full/dpsdk_firmware.elf`

Result:
- `text=952480`, `data=49996`, `bss=186440`
- Flash estimate (`text+data`): `1,002,476 B` / `2,097,152 B` = **47.8%**
- Main SRAM estimate (`.data + .bss + heap/stack`): `187,184 B` / `196,608 B` = **95.2%**
- CCMRAM (`.ccmram`): `49,152 B` / `65,536 B` = **75.0%**

Production-oriented baseline (`dp_stm32f427_dev_interactive`, Release):
- `text=93,232`, `data=204`, `bss=27,488`
- Flash estimate (`text+data`): `93,436 B` / `2,097,152 B` = **4.5%**
- Main SRAM estimate (`.data + .bss + heap/stack`): `27,656 B` / `196,608 B` = **14.1%**
- CCMRAM (`.ccmram`): `0 B` / `65,536 B` = **0.0%**

### MCU Feasibility (re-evaluated)

| Target | Feasibility | Notes |
|--------|-------------|-------|
| Cortex-M4 / M7, RAM ≥ 192 KB (+ CCM preferred) | ✅ **Feasible in current full test profile** | `dp_stm32f427_dev + all_tests` has been built and validated |
| Cortex-M3 / small M4, RAM 64–128 KB | ⚠️ **Feasible only with aggressive trimming** | Must disable `test_all`, reduce features via overlays, and re-measure |
| Cortex-M0 / M0+, RAM ≤ 64 KB | ❌ **Not feasible with current profile** | Requires dedicated tiny profile and measurement evidence before claiming support |

**Optimization priorities:**
- Ship builds should not use `all_tests + test_all`; use product app + minimal overlays.
- Enable `-Os -fno-exceptions -fno-rtti` and LTO for release firmware.
- Keep `OSALThread` lambda captures < 16 B to avoid heap fallback.
- Prefer `OSALMutex` over `OSALRWLock` unless concurrent readers are required.
- If targeting M3/M0 class, prioritize replacing `std::function` in thread entry with a fixed-capacity callable wrapper.

**Guidelines:**
- Enable `-Os -fno-exceptions -fno-rtti` (saves 1–3 KB Flash).
- Keep lambda captures < 16 B to avoid heap allocation in `OSALThread`.
- Prefer `OSALMutex` over `OSALRWLock` unless concurrent readers are needed.
- `OSALRWLock`: one `readLock()` issues up to 5 CMSIS-OS2 calls vs 1–2 direct.
