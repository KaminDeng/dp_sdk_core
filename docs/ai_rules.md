# ai_rules.md — dp_sdk_core

## Overview

**dp_sdk_core** is the unified core abstraction layer for dpsdk, containing three modules:

- **osal** -- C++17 OS Abstraction Layer (POSIX / CMSIS-OS2 backends)
- **hal** -- Zero-cost CRTP hardware abstraction (header-only interfaces)
- **device** -- Device management framework (registration, lifecycle, shell integration)

**Git Remote**: `git@gitee.com:kamin_self/osal.git` (submodule at `core/dp_sdk_core`)

## Build

Not built standalone -- consumed by dpsdk via `add_subdirectory(core/dp_sdk_core)`.

Build from the **parent dpsdk directory**:
```sh
# Default POSIX host build (Debug, ASAN on)
cmake -B build -DPRODUCT=posix_demo && cmake --build build

# With HAL/Device tests enabled
cmake -B build -DPRODUCT=posix_demo -DDP_HAL_BUILD_TESTS=ON -DDP_DEVICE_BUILD_TESTS=ON && cmake --build build

# TSAN build (ASAN and TSAN are mutually exclusive)
cmake -B build -DPRODUCT=posix_demo -DASAN=OFF -DTSAN=ON && cmake --build build
```

## Running Tests

Tests are compiled into static libraries and invoked from the host application task context (not standalone binaries). Entry points are extern "C" functions:

```c
extern "C" void osal_test_main(void);     // OSAL GTest suites
extern "C" void dp_hal_test_main(void);   // HAL GTest suites (requires -DDP_HAL_BUILD_TESTS=ON)
extern "C" void dp_device_test_main(void);// Device GTest suites (requires -DDP_DEVICE_BUILD_TESTS=ON)
```

From parent dpsdk: use `/dpsdk-test` skill or `docs/ai_docs/workflow/dpsdk-test.md` for the full test workflow (build + run + ASAN + TSAN).

### Test Controls (OSAL)

In `osal_port.h`:
- `OSAL_TEST_ALL = 1` -- run all 12 suites
- `OSAL_TEST_ALL = 0` -- per-component flags: `OSAL_TEST_THREAD_ENABLED`, `OSAL_TEST_MUTEX_ENABLED`, etc.

### Single Translation Unit Pattern

All test `.cpp` files are `#include`d into a single `*_test_main.cpp` per module to prevent linker dead-stripping of GTest static registrations. Never add test `.cpp` files to CMake sources directly -- add `#include` in the corresponding test_main.

## Architecture

### Module Dependency Chain

```
device  -->  hal  (public)
device  -->  osal (private, for DeviceManager mutex)
hal     -->  (independent, header-only CRTP)
osal    -->  (POSIX or CMSIS-OS2 backend)
```

### OSAL Design

Virtual base class interfaces (`src/interface/`: IThread, IMutex, IQueue<T>, ISemaphore, IRWLock, ISpinLock, IConditionVariable, ITimer, IThreadPool, IChrono, IMemoryManager, ISystem) with two backend implementations:

- **POSIX** (`src/impl/posix/`) -- pthreads, std::thread, std::mutex
- **CMSIS-OS2** (`src/impl/cmsis_os/`) -- maps to FreeRTOS/Zephyr/RT-Thread

Backend selected at CMake time by scanning `osal_port.h` for `#define OSAL_BACKEND_CMSIS_OS` (POSIX is default).

### HAL Design

CRTP interfaces (UartBase, GpioPinBase, SpiBusBase, I2cBusBase, AdcBase, DacBase, CanBase, TimerBase, PwmBase, PowerManageable) -- zero runtime overhead. Implementations provide private `do*` methods and `friend class XxxBase<Self>`. See `hal/CLAUDE.md` for full interface list.

### Device Design

Adapter pattern: `SerialDevice<HalImpl>`, `PinDevice<HalImpl>` etc. wrap HAL implementations behind `Device` base class. `DeviceManager` singleton provides thread-safe registry (fixed array, no heap, OSAL Mutex). See `device/CLAUDE.md`.

## Port System

Both OSAL and HAL use a port mechanism to isolate platform-specific code.

### OSAL Port

Set `OSAL_PORT_DIR` before `add_subdirectory(osal)`. Defaults to `port/builtin_posix/`.

Port directory must contain `osal_port.h` with:
1. Backend selection macro (`OSAL_BACKEND_POSIX` or `OSAL_BACKEND_CMSIS_OS`)
2. Platform includes (CMSIS-OS: `cmsis_os2.h`)
3. Constants (`OSAL_PORT_THREAD_MIN_STACK_SIZE`, `OSAL_PORT_THREAD_DEFAULT_PRIORITY`)
4. Debug output function (`osal_port_debug_write`)
5. Test feature flags (`OSAL_TEST_ALL` or per-component)

### HAL Port

3-tier fallback resolution:
1. `platform/<KERNEL_PORT>/hal_port/` (product-specific)
2. `platform/common/hal_port/` (shared default)
3. `hal/port/mock/` (built-in mock for host testing)

Port provides: `hal_port.h` (typedef bindings), implementation headers (e.g. `mock_uart.h`), `dp_hal_port_impl.cpp` (port functions), `CMakeLists.txt` (builds `dp_hal_port_impl`).

## Known Implementation Notes

- **RWLock.isWriteLocked()**: uses `writeLocked_` atomic flag (readers also hold writeSemaphore)
- **SpinLock.isLocked()**: uses `atomic<bool> locked_` (osMutexGetOwner deadlocks on POSIX sim)
- **Semaphore**: constructor `max_count=16` for counting semaphore support
- **OSALThread stop**: cooperative via atomic flag + osDelay chunks in CMSIS-OS backend

## Cross-Platform Module Compliance

All three modules follow cross-platform-module skill conventions:
- Compat headers with module-specific prefixes (`DP_HAL_*`, `DP_DEV_*`)
- Port isolation (zero platform API in `src/` or `include/`)
- Fixed-width types throughout; no VLAs