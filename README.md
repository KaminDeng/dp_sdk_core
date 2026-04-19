[English](./README.md) | [中文](./README_zh.md)

# dp_sdk_core

`dp_sdk_core` is a unified core abstraction layer for embedded systems.  
Built on **C++17**, it provides a common foundation for Linux / POSIX host environments and multiple RTOS targets, covering system primitives, peripheral abstractions, and device lifecycle management.

Its goal is not simply to wrap APIs, but to establish a foundation suited for long-term embedded evolution:

- keep the programming interface as consistent as possible across Linux and multiple RTOSes
- confine platform differences to replaceable, portable adaptation layers
- preserve the expressive value of C++ interfaces while controlling abstraction cost
- provide a stable base for multiple boards, RTOSes, and product variants

---

## Overall architecture

The project consists of three main modules:

| Module | Role | Design focus |
|---|---|---|
| **dp_osal** | Unifies threads, mutexes, queues, timers, semaphores, and related OS primitives | CRTP interfaces + POSIX / CMSIS-OS backends |
| **dp_hal** | Unifies UART, GPIO, SPI, I2C, ADC, DAC, CAN, PWM, Timer, and related peripheral interfaces | header-only CRTP + optional virtual wrappers |
| **dp_device** | Unifies device registration, lookup, open/close control, and lifecycle handling | Adapter pattern + fixed-capacity manager |

Dependency layout:

```text
device  ->  hal   (public)
device  ->  osal  (private)
hal     ->  independent
osal    ->  POSIX / CMSIS-OS backend
```

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-arch.png" width="860" />
</div>

This structure matters because it gives change a clear home:

- **OS differences** are confined to `dp_osal`
- **hardware differences** are confined to `dp_hal`
- **device lifecycle concerns** are confined to `dp_device`

The application layer does not need to depend directly on RTOS-specific APIs or vendor HAL code, which keeps platform changes more manageable.

---

## Engineering evolution model

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-migration.png" width="860" />
</div>

`dp_sdk_core` is designed for progressive system organization rather than all-at-once rewrites:

- business logic depends on stable interfaces rather than platform details
- OS differences are converged through POSIX / CMSIS-OS paths
- hardware differences are converged through HAL ports
- device management is unified in the Device layer

That makes it especially suitable for projects evolving across multiple boards, RTOSes, and product configurations.

---

## Key characteristics

### 1. One interface model across Linux and multiple RTOSes

`dp_osal` provides a unified system programming interface:

- on Linux / macOS hosts, it uses the POSIX backend;
- on RTOS targets, it converges system differences through the **CMSIS-OS interface**.

That means:

- upper-layer code is primarily written against one OSAL interface,
- RTOS replacement or expansion stays concentrated in the adaptation layer,
- and any RTOS that can be adapted to CMSIS-OS can, in principle, fit this model.

The project has already been adapted and validated on **FreeRTOS, RT-Thread, and Zephyr**.  
This is one of the defining characteristics of `dp_sdk_core`: **it uses CMSIS-OS adaptation to support multiple RTOSes and align the programming interface across Linux and different RTOS environments.**

### 2. C++ interface design balances expressiveness and controlled cost

The core interfaces in `dp_osal` and `dp_hal` rely heavily on **CRTP**, rather than defaulting to all-virtual interfaces.

This gives several direct benefits:

- hot paths are easier for the compiler to inline,
- the abstraction layer does not automatically introduce vtable cost,
- production and host-test paths can use different abstraction styles,
- and the design is better suited for long-term reuse in C++17 embedded projects.

For host-side testing, dependency injection, and GMock-based verification, the project also provides:

- `dp_osal_virtual.h`
- `dp_hal_virtual.h`

These virtual wrappers are meant for testing and injection scenarios, not as the default production abstraction path.

### 3. Resource cost is treated as controllable, not ignored

`dp_sdk_core` does not present abstraction as “free.”  
Instead, it controls cost through several design choices:

- HAL interfaces are primarily CRTP-based so most abstraction cost can be absorbed at compile time
- the Device layer uses fixed-capacity registries rather than default heap allocation
- type handling avoids RTTI where practical
- thread, thread pool, and `std::function` overhead are treated explicitly with defined usage boundaries

This allows the project to retain modern C++ engineering value while respecting MCU resource limits.

---

## Getting Started

### 1. Integration

`dp_sdk_core` is integrated through a parent project via `add_subdirectory()`.

```cmake
# Top-level CMakeLists.txt
set(DP_OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)   # optional, defaults to builtin_posix
add_subdirectory(core/dp_sdk_core)

target_link_libraries(my_app PRIVATE dp_osal)    # OS abstraction
target_link_libraries(my_app PRIVATE dp_hal)     # hardware abstraction
target_link_libraries(my_app PRIVATE dp_device)  # device management
```

Typical build flow:

```sh
cmake -B build -DPRODUCT=posix_demo
cmake --build build
```

### 2. Recommended workflow

A practical usage path is:

1. validate core business logic in Linux / POSIX first
2. keep upper-layer programming stable through unified OSAL / HAL interfaces
3. bring different RTOSes into the same model through CMSIS-OS adaptation
4. finally switch to target-specific hardware implementations and complete MCU integration and measurement

This workflow is well suited to validating on the host first and then landing on RTOS and hardware targets.

---

## Module details

## Module 1: dp_osal

`dp_osal` provides unified interfaces for system primitives. Main interfaces include:

| Header | Main capability |
|---|---|
| `interface_thread.h` | thread start/stop/join/detach/priority control |
| `interface_mutex.h` | mutex, tryLock, timed lock |
| `interface_condition_variable.h` | condition variables |
| `interface_lockguard.h` | RAII lock guard |
| `interface_rwlock.h` | reader/writer lock |
| `interface_semaphore.h` | counting semaphore |
| `interface_spin_lock.h` | spin lock |
| `interface_queue.h` | queue send/receive/timed receive |
| `interface_timer.h` | one-shot / periodic timers |
| `interface_thread_pool.h` | thread pool |
| `interface_chrono.h` | monotonic clock and time conversion |
| `interface_system.h` | scheduler, sleep, critical sections, system info |
| `dp_osal_virtual.h` | host-side virtual wrappers |

### Backend model

```text
src/impl/posix/     ← DP_OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← DP_OSAL_BACKEND_CMSIS_OS
```

CMake selects the backend automatically according to the macro enabled in `dp_osal_port.h`.

| Backend | Use case |
|---|---|
| `DP_OSAL_BACKEND_POSIX` | Linux, macOS, POSIX host environments |
| `DP_OSAL_BACKEND_CMSIS_OS` | RTOS environments adapted through CMSIS-OS |

### Why CMSIS-OS is used as the RTOS convergence layer

On the RTOS side, the project does not expose each system’s native API directly to the application layer.  
Instead, it uses CMSIS-OS as the convergence layer.

This gives several benefits:

- upper-layer interfaces stay more stable,
- RTOS replacement does not require rewriting business logic,
- Linux / POSIX and multi-RTOS environments can share a more consistent system programming model.

### `dp_osal_port.h` port template

The port template is organized into five parts:

1. backend selection
2. platform includes
3. platform constants
4. debug output
5. test switches

Typical usage is to copy the template into a project-specific port directory and point `DP_OSAL_PORT_DIR` to it.

---

## Module 2: dp_hal

`dp_hal` provides unified peripheral interfaces. Its main characteristics are:

- core interfaces use **header-only + CRTP**
- production paths avoid unnecessary runtime dispatch overhead
- test and injection scenarios can use virtual wrappers

Main interfaces include:

| Header | Description |
|---|---|
| `dp_uart.h` | UART |
| `dp_gpio.h` | GPIO |
| `dp_spi.h` | SPI bus and device |
| `dp_i2c.h` | I2C bus and register helper |
| `dp_adc.h` | ADC |
| `dp_dac.h` | DAC |
| `dp_can.h` | CAN |
| `dp_timer.h` | Timer |
| `dp_pwm.h` | PWM |
| `dp_hal_power.h` | power-management mixin |
| `dp_hal_virtual.h` | test / injection virtual interfaces |

### HAL port resolution order

`dp_hal` resolves ports in the following order:

1. `platform/<KERNEL_PORT>/hal_port/`
2. `platform/common/hal_port/`
3. `hal/port/mock/`

This supports:

- board- or product-specific firmware ports
- shared common ports
- mock-based host testing

---

## Module 3: dp_device

`dp_device` is responsible for unified device-level management rather than direct low-level driver implementation.

Core responsibilities include:

- `Device`: unified device name, type, and open/close reference counting
- `DeviceManager`: centralized registration, lookup, iteration, and removal
- adapter classes: bridging HAL implementations into managed device objects

### Design characteristics

- explicit device name length limits
- fixed-capacity registry instead of default heap allocation
- thread-safe management
- type handling through `DeviceType` rather than RTTI

This keeps device lifecycle rules inside the framework rather than scattering them across application modules.

---

## Testing model

Tests are organized as “module static libraries + host entry points” rather than standalone test executables.

Entry points include:

```c
extern "C" void dp_osal_test_main(void);
extern "C" void dp_hal_test_main(void);
extern "C" void dp_device_test_main(void);
```

Testing characteristics:

- OSAL supports full or per-component test enablement
- host scenarios can use `dp_osal_virtual.h` / `dp_hal_virtual.h` for dependency injection
- test entry uses a single-translation-unit pattern to prevent GTest static registrations from being stripped by the linker

---

## Resource cost and MCU feasibility

The project treats resource cost as something to be **bounded, trimmed, and validated**.

### Known cost characteristics

- HAL interfaces are primarily CRTP-based so most abstraction cost can be absorbed at compile time
- some OSAL objects, especially `Thread`, add RAM / Flash overhead because of `std::function` and state handling
- virtual wrappers are mainly intended for host-side testing, not as the default production abstraction path

### MCU feasibility reference

| Target MCU | Feasibility | Notes |
|---|---|---|
| Cortex-M4 / M7, RAM ≥ 192 KB (CCM recommended) | ✅ current full-test profile is feasible | already has a practical build-and-validate basis |
| Cortex-M3 / small-memory M4, RAM 64–128 KB | ⚠️ feasible only with aggressive trimming | requires disabling `test_all`, reducing features, and re-measuring |
| Cortex-M0 / M0+, RAM ≤ 64 KB | ❌ not something the current profile should claim directly | requires a dedicated tiny profile and measured validation |

### Usage guidance

- release builds should enable `-Os -fno-exceptions -fno-rtti` and LTO
- for smaller-memory targets, thread usage and `std::function` capture cost should be evaluated carefully
- prefer `Mutex` over `RWLock` unless concurrent-reader behavior is clearly needed
- keep test profiles and shipping profiles separate; do not use the full-test profile as the production resource baseline

---

## Best fit

`dp_sdk_core` is especially suitable when:

- business logic must be validated on both Linux hosts and MCU targets
- the product spans multiple RTOSes or multiple hardware platforms
- the team is already paying significant maintenance cost for platform glue code
- a reusable C++17 foundation layer is desired
- a more consistent Linux / RTOS programming experience is valuable

---

## License

[MIT](./LICENSE)
