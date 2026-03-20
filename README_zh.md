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

## 组件列表

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
