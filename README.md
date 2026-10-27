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
device  ->  osal  (public)
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
| `interface_thread.h` | explicit start, cooperative stop, true join, optional timed join |
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

### 正式接口与平台能力

`Thread` 只空构造，禁止复制/移动，默认创建 joinable 线程；一个 owner 串行执行生命周期及挂起/恢复/优先级操作；仅 requestStop、stopRequested、joinable、isRunning 四项允许跨线程调用，仍须由 owner 保证包装对象寿命。推荐 `start(entry, context, attributes)`，也保留 `start(name, entry, context, priority, stack_bytes, stack)` 便捷形式。函数指针及可转换的无捕获 lambda 不为入口包装额外分配，捕获 callable 仍可能由 std::function 分配；不公开任务存储或线程自销毁回调。

`requestStop()` 幂等发布协作停止请求，业务循环显式查询 `stopRequested()` 并唤醒自身等待；请求不代表已停止。`join()` 成功才确认原生线程结束及资源回收，允许释放借用 context、栈与控制块。自然返回后 `joinable()` 仍为 true，`isRunning()==false` 不是释放屏障。已有回收责任时 start 返回 kBusy；join 失败/超时保留尚未完成的责任，禁止覆盖旧线程。原生线程已回收但同步资源清理失败时，后续 join 仅重试剩余清理，不能重复回收原生句柄；joinable 为 false 也不能抹掉已报告的清理失败。自身 join 返回 kWouldDeadlock。

析构前必须完成 join 和必要清理；违反约束在 Release 也终止进程/触发平台 fatal，不自动无限等待、不强杀、不静默分离。显式 `stop()` 是 requestStop 加无限 join 的便捷操作，可能一直等待业务退出，不能拿它实现有界关闭。所有后端 `detach()` 返回 kNotSupported。

有限等待通过 `supportsTimedJoin()`（后端 `kSupportsTimedJoin`）查询；`tryJoinFor(timeout_ms)` 使用毫秒相对期限，超时保留全部资源。当前 Linux/glibc >= 2.31 的 GNU 接口和 Zephyr 原生 join 支持，其他 POSIX、FreeRTOS、RT-Thread 与 ESP-IDF 明确返回 kNotSupported，不用“完成通知加无限回收”伪装有界 join。ThreadResult 用 ok()/status/native_error 检查；错误包括 Timeout、NotSupported、InvalidState、Busy、WouldDeadlock 和资源/原生错误。挂起/恢复/优先级是后端扩展，不是通用同步安全点。

静态资源通过`ThreadResources`和后端资源存储表达，`supportsControlBlockStorage()` 反映实际能力；栈容量统一字节。完成通知不等于原生线程真正退出，借用资源须一直保留到成功 join。FreeRTOS/ESP 的 owner 回收与 SMP 在核运行状态由端口处理；不支持的资源配置显式拒绝。

`Semaphore`是唯一计数唤醒原语。`DeferredInit`构造不取得资源，open/release/close反馈错误，waitForMs区分kAcquired/kTimeout/kError，maxCount给出实际配置上限；close/init须在无并发使用者时执行。Linux后端使用原生sem与单调期限，其他POSIX维持可移植等待；CMSIS/ESP按实际内核能力实现。ISR资格与测量资格仍独立检查。

`Chrono`的TimePoint仍为32位后端tick，绝对等待要求同域及半圈约束。steadyNs单独提供真实64位单调纳秒，失败返回0；wallTimeUs/threadCpuTimeUs以bool反馈读取结果，失败不覆盖输出。未具备可靠来源的后端报告不支持，不能通过tick乘常数伪造精度。原有频率、epoch和measurement有效性规则保持。

字节输出、文件与文件系统分别使用`ByteOutput`、`File`、`FileSystem`，数量及IoResult一起检查；时钟不再属于I/O对象。描述符绑定、标准输入、信号与进程退出放在明确native port中，不进入通用接口。短写、背压和部分失败需要调用方处理；文件sync不等于同步父目录。

`DP_OSAL_ENABLE_MEASUREMENT`默认OFF，通过唯一dp_osal_build_config传播0/1；同一进程所有对象配置一致，不在单TU手写宏。编译关闭时不读取采样时钟，诊断返回disabled/invalid；开启后仍检查时间域、频率、epoch、溢出与ISR覆盖。线程快照返回0表示本次无样本，不能解释为无线程或无界重试。

CORE不保留旧实现切换、旧门面或旧头转发。HAL仅提供硬件机制；通信Profile/provider/会话身份合同由dp_comm_lite拥有。Device公开头使用OSAL Mutex，因此OSAL依赖公开传播；注册表保存借用指针，对象寿命和操作串行化由使用方保证。

详见[正式资源与I/O契约](dp_osal/docs/formal_contracts.md)。SDK分发者必须记录正式合同revision=3和measurement配置，拒绝旧前缀/旧归档混用；archive-only SDK不安装OSAL源码头。

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
