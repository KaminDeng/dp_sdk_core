# osal — 操作系统抽象层

为线程、同步、内存和定时原语提供可移植、与 RTOS 无关的统一接口的C++库。
针对 13 个抽象接口编写一次应用代码，通过修改一个配置文件即可切换后端
（POSIX 或 CMSIS-OS2）。

---

## 快速开始

### 第一步 — 复制移植模板

```sh
cp osal/port/template/osal_port.h  my_project/my_port/osal_port.h
```

### 第二步 — 填写 `osal_port.h`

打开 `my_port/osal_port.h`，按照下方参考说明填写五个节（section）。

### 第三步 — 在 CMake 中指定移植目录

在 `CMakeLists.txt` 中，**在** `add_subdirectory(osal)` **之前**：

```cmake
set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
add_subdirectory(path/to/osal)
target_link_libraries(my_app PRIVATE osal)
```

完成。不需要其他文件。

---

## `osal_port.h` 参考

`osal_port.h` 是为目标平台配置库的唯一文件，共有五个节。

### 第一节 — 后端选择

取消注释**恰好一行**。同时定义两个宏是未定义行为。

```cpp
#define OSAL_BACKEND_POSIX      // std::thread, pthread, std::mutex, std::chrono
// #define OSAL_BACKEND_CMSIS_OS  // osThreadNew, osMutexNew, osTimerNew, …
```

| 后端 | 适用场景 |
|------|----------|
| `OSAL_BACKEND_POSIX` | Linux、macOS、任何 POSIX 主机 |
| `OSAL_BACKEND_CMSIS_OS` | FreeRTOS+CMSIS-OS2、Zephyr、ThreadX 等 |

### 第二节 — 平台头文件

```cpp
// POSIX — 留空；osal 内部使用 std:: 和 POSIX 接口。

// CMSIS-OS2 — 包含 RTOS 提供的头文件：
#include "cmsis_os2.h"   // FreeRTOS+CMSIS-OS2（来自 BSP）
// #include <cmsis_os2.h>  // Zephyr 内置兼容层
// #include "tx_cmsis.h"   // ThreadX CMSIS-OS2
```

### 第三节 — 平台常量

```cpp
// 线程最小栈大小（字节）。
//   POSIX:    PTHREAD_STACK_MIN（Linux 通常为 8192–16384）
//   FreeRTOS: configMINIMAL_STACK_SIZE * sizeof(StackType_t)
//   Zephyr:   512
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#define OSAL_PORT_THREAD_MIN_STACK_SIZE  4096
#endif

// 默认线程优先级。
//   POSIX:    0（无 root 权限时优先级无实际效果）
//   CMSIS-OS: osPriorityNormal
#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY  0
#endif
```

### 第四节 — 调试输出

将 `osal_port_debug_write` 实现为 **inline** 函数（防止被多个翻译单元包含时
产生 ODR 链接错误）：

```cpp
#include <cstdint>

// POSIX / Linux / macOS — 输出到 stdout：
#include <unistd.h>
inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// STM32 HAL UART — 根据 BSP 调整句柄和头文件：
// #include "usart.h"
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     HAL_UART_Transmit(&huart6, reinterpret_cast<const uint8_t*>(buf),
//                       static_cast<uint16_t>(len), HAL_MAX_DELAY);
// }

// Zephyr printk：
// #include <zephyr/sys/printk.h>
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     printk("%.*s", static_cast<int>(len), buf);
// }
```

### 第五节 — 测试功能开关

```cpp
// OSAL_TEST_ALL = 1  → 启用所有测试组件（首次移植推荐）
// OSAL_TEST_ALL = 0  → 使用下方的逐组件开关进行精细控制
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

## 内置 POSIX 移植（零配置）

若**未设置** `OSAL_PORT_DIR`，osal 自动使用
`port/builtin_posix/osal_port.h`——一个开箱即用的 POSIX 移植，调试输出写入
stdout，并启用全部测试。无需任何配置。

```cmake
# Linux / macOS 最简集成（无需 OSAL_PORT_DIR）：
add_subdirectory(path/to/osal)
target_link_libraries(my_app PRIVATE osal)
```

---

## 移植示例

`example/` 目录包含三个完整的可用移植：

| 目录 | 平台 | 后端 | 调试输出 |
|------|------|------|----------|
| `osal_port_freertos/` | STM32 裸机 + FreeRTOS | CMSIS-OS2 | `HAL_UART_Transmit`（UART6）|
| `osal_port_sim_freertos/` | FreeRTOS-on-POSIX 模拟器 | CMSIS-OS2 | `write(1, …)`（stdout）|
| `osal_port_zephyr/` | Zephyr RTOS | CMSIS-OS2（内置）| `printk` |

每个示例目录只包含 `osal_port.h`（模拟器示例另有 `cmsis_os2_freertos/` 适配器）。
选取最接近目标平台的示例，复制后按需修改即可。

---

## 单片机资源消耗

本节对 OSAL 抽象层与直接调用 FreeRTOS / CMSIS-OS2 接口相比的 RAM、Flash
及运行时额外开销进行量化说明。所有数据以 32-bit Cortex-M + CMSIS-OS2 后端为基准。

### RAM — 每个对象实例的额外字节

| 组件 | 直接 OS 句柄 | OSAL 对象 | 额外 RAM |
|------|------------|-----------|---------|
| `OSALMutex` | 4 B（`osMutexId_t`）| 8 B | **+4 B**（vtable 指针）|
| `OSALSemaphore` | 4 B | 8 B | **+4 B** |
| `OSALThread` | 4 B（`osThreadId_t`）| ≥ 48 B | **+44 B** |
| `OSALConditionVariable` | ~12 B（手写实现）| 12 B | ~0 B |
| `OSALRWLock` | ~20 B（手写实现）| 20 B | ~0 B |
| `OSALMessageQueue<T>` | 4 B | 8 B | **+4 B** |
| `OSALLockGuard` | 0 B（栈）| 12 B（栈）| **+12 B**（栈）|
| `OSALSpinLock` | 4 B | 8 B | **+4 B** |

**`OSALThread` 的额外开销最大（+44 B），来源如下：**
- `std::function<void(void*)>` 任务函子：对象内有 16–32 B 的小缓冲区；若 lambda
  捕获列表超过 ~16 B，会触发**堆分配**（在堆空间紧张的 MCU 上存在碎片化风险）。
- 每个 `OSALThread` 实例额外持有一个 `osSemaphoreId_t`（`exitSemaphore`），用于
  `join()` 功能——会在堆中分配一个 FreeRTOS 信号量对象。
- 两个 `std::atomic<bool>` 字段（共 8 B）。

### Flash — 代码体积增量

| 来源 | 估算大小 |
|------|---------|
| 每个组件的 vtable（12 个组件）| 约 60 B/个 → **合计 ~720 B** |
| `std::function` 每种 lambda 签名的模板实例化 | 200–500 B/种 |
| C++ 运行时（构造/析构函数）| **2–4 KB** |
| **OSAL 总额外开销（典型值，`-Os` 优化）** | **约 4–8 KB Flash** |

### 运行时 — 每次 API 调用的额外开销

每次 OSAL 调用经过一层虚函数派发：
1. 从对象加载 vtable 指针。
2. 从 vtable 数组加载函数指针。
3. 执行间接跳转（BLX）。

每次调用额外约 **3–5 个 CPU 周期**。对于 mutex / 信号量操作（OS 调度器通常
耗费数百周期），该开销 **< 1%**，实际可忽略不计。

**例外——`OSALRWLock`：** 一次 `readLock()` 最多产生 5 次 CMSIS-OS2 调用
（1 次 mutex + 2 次信号量操作 + 2 次释放），而直接实现通常只需 1–2 次。
在资源受限的 MCU 上读多写少的场景，建议改用 `OSALMutex`。

### 单片机可行性总结

| 目标芯片 | 可行性 | 说明 |
|---------|--------|------|
| Cortex-M4 / M7，RAM ≥ 64 KB | ✅ **推荐** | 全功能可用；`std::function` 开销可接受 |
| Cortex-M3，RAM 32–64 KB | ⚠️ **谨慎使用** | 控制 `OSALThread` 数量；避免 lambda 捕获过多变量 |
| Cortex-M0 / M0+，RAM ≤ 16 KB | ❌ **不推荐** | `std::function` + C++ 运行时占用 Flash 和 RAM 的比例过大 |

**MCU 使用注意事项：**
- 启用 `-Os`（大小优化）以及 `-fno-exceptions -fno-rtti`，可减少
  C++ 异常和 RTTI 开销（节省 1–3 KB Flash）。
- lambda 捕获列表应保持 < 16 B，避免 `OSALThread` 触发堆分配。
- 若不需要并发读，用 `OSALMutex` 代替 `OSALRWLock`；后者每个实例占用三个 OS 对象。
- `OSALSemaphore::init()` 只能在**没有线程阻塞**于该信号量时调用；对活跃信号量调用
  是未定义行为（MCU 上将导致 HardFault）。

---

| 接口头文件 | 抽象类 | 主要操作 |
|-----------|--------|----------|
| `interface_thread.h` | `IThread` | `start`、`stop`、`join`、`detach`、`suspend`、`resume`、`setPriority` |
| `interface_mutex.h` | `IMutex` | `lock`、`unlock`、`tryLock`、`tryLockFor` |
| `interface_condition_variable.h` | `IConditionVariable` | `wait`、`waitFor`、`notifyOne`、`notifyAll` |
| `interface_lockguard.h` | `ILockGuard` | RAII 互斥锁守卫 |
| `interface_rwlock.h` | `IRWLock` | 读写锁，含 try 和超时变体 |
| `interface_semaphore.h` | `ISemaphore` | 计数信号量 |
| `interface_spin_lock.h` | `ISpinLock` | 自旋锁（atomic_flag）|
| `interface_queue.h` | `MessageQueue<T>` | `send`、`receive`、`tryReceive`、`receiveFor`、`size`、`clear` |
| `interface_memory_manager.h` | `IMemoryManager` | 块内存池：`allocate`、`deallocate`、`allocateAligned` |
| `interface_timer.h` | `ITimer` | 单次和周期定时器 |
| `interface_thread_pool.h` | `IThreadPool` | 动态线程池，含任务队列 |
| `interface_chrono.h` | `IChrono` | 单调时钟：`elapsed`、`to_time_t`、`from_time_t` |
| `interface_system.h` | `ISystem` | `schedulerStart`、`sleep_ms`、`sleep_s` |

所有接口均为纯虚类（包含时无实现依赖）。具体实现在链接阶段由
`osal_port.h` 中选择的后端决定。

---

## 后端选择机制

```
src/impl/posix/     ← OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← OSAL_BACKEND_CMSIS_OS
```

CMake 通过扫描 `osal_port.h` 中未注释的 `#define OSAL_BACKEND_CMSIS_OS`
行自动检测当前后端，并编译对应的源码树。每次构建只编译一个后端。

对于依赖额外平台库的 CMSIS-OS 移植（如 `freertos_kernel`），需在外部手动链接：

```cmake
target_link_libraries(osal PUBLIC freertos_kernel cmsis_os2_impl)
```

---

## 运行测试

测试代码编译进 `osal` 静态库本身（Google Test，12 个测试套件）。测试在宿主
应用的任务/线程上下文中运行，没有独立的测试可执行文件。

在 POSIX 主机上运行全部测试：

```sh
cmake -B build -S path/to/host_project
cmake --build build
./build/posix_demo          # Linux / 直接终端
```

在 **macOS** 上，stdout 在非 TTY 时会被全量缓冲，使用 `script(1)` 分配伪
终端以确保输出被及时刷新：

```sh
script -q /tmp/osal_out.txt ./build/posix_demo 2>/dev/null &
sleep 10 && kill %1
cat /tmp/osal_out.txt
```

若需跳过某个测试套件，在 `osal_port.h` 中将对应的
`OSAL_TEST_*_ENABLED` 标志设为 `0`（需同时设置 `OSAL_TEST_ALL 0`）。
