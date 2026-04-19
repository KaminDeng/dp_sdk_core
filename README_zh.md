[English](./README.md) | [中文](./README_zh.md)

# dp_sdk_core

`dp_sdk_core` 是一套面向嵌入式系统的统一核心抽象层。  
它基于 **C++17** 设计，面向 Linux / POSIX 主机环境与多种 RTOS 环境，提供统一的系统原语抽象、硬件外设抽象以及设备生命周期管理能力。

它的目标不是单纯包一层接口，而是建立一套适合长期演进的工程基础：

- 在 Linux 与多种 RTOS 下尽量保持一致的编程接口风格
- 将平台差异收敛到可替换、可移植的适配层
- 在保留 C++ 接口表达力的同时控制抽象带来的资源成本
- 为多板卡、多 RTOS、多产品形态的演进提供稳定基础

---

## 整体架构

项目由三个核心模块组成：

| 模块 | 作用 | 设计重点 |
|---|---|---|
| **dp_osal** | 统一线程、互斥锁、队列、定时器、信号量等 OS 原语 | CRTP 接口 + POSIX / CMSIS-OS 后端 |
| **dp_hal** | 统一 UART、GPIO、SPI、I2C、ADC、DAC、CAN、PWM、Timer 等外设接口 | header-only CRTP + 可选虚接口包装 |
| **dp_device** | 统一设备注册、查找、打开/关闭与生命周期管理 | Adapter + 固定容量管理器 |

依赖关系如下：

```text
device  ->  hal   (public)
device  ->  osal  (private)
hal     ->  independent
osal    ->  POSIX / CMSIS-OS backend
```

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-arch.png" width="860" />
</div>

这种结构的意义在于：

- **OS 差异**收敛到 `dp_osal`
- **硬件差异**收敛到 `dp_hal`
- **设备生命周期管理**收敛到 `dp_device`

上层业务逻辑不必直接依赖具体 RTOS API 或厂商 HAL，从而让平台替换和产品扩展的改动面保持可控。

---

## 工程演进方式

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-migration.png" width="860" />
</div>

`dp_sdk_core` 适合的不是“所有代码都从零重写”的开发方式，而是把系统逐步整理到统一核心层中的方式：

- 业务逻辑依赖稳定接口，而不是平台细节
- OS 差异通过 POSIX / CMSIS-OS 路径收敛
- 硬件差异通过 HAL port 收敛
- 设备管理通过 Device 层统一收口

这使它非常适合多板卡、多 RTOS、多产品配置并行演进的项目。

---

## 项目特点

### 1. 用一套接口拉齐 Linux 与多种 RTOS 的编程方式

`dp_osal` 为上层提供统一的系统编程接口：

- 在 Linux / macOS 等主机环境下，使用 POSIX 后端；
- 在 RTOS 环境下，通过 **CMSIS-OS 接口**收敛不同系统的差异。

这意味着：

- 业务层主要面向统一 OSAL 接口编写；
- RTOS 替换或扩展时，主要改动集中在适配层；
- 只要能够适配到 CMSIS-OS 接口的 RTOS，理论上都可以纳入这一模型。

项目已在 **FreeRTOS、RT-Thread、Zephyr** 等环境中完成适配与验证。  
这也是 `dp_sdk_core` 的一个核心特征：**通过 CMSIS-OS 适配多种 RTOS，拉齐 Linux 与不同 RTOS 下的编程接口。**

### 2. C++ 接口设计强调表达力与可控成本并重

`dp_osal` 与 `dp_hal` 的核心接口大量采用 **CRTP** 组织，而不是默认使用全虚接口。

这种设计带来几个直接收益：

- 高频调用路径更容易被编译器内联；
- 抽象层不会天然引入 vtable 成本；
- 生产路径与 host 测试路径可以使用不同抽象形式；
- 更适合在 C++17 嵌入式工程中长期维护与复用。

对于依赖注入、GMock 或 host 测试场景，项目也提供：

- `dp_osal_virtual.h`
- `dp_hal_virtual.h`

这类虚接口包装，用于测试和注入，而不是默认生产抽象路径。

### 3. 资源成本是可控的，而不是回避不谈

`dp_sdk_core` 并不把抽象层描述为“没有成本”。  
它通过以下方式控制成本：

- HAL 接口以 CRTP 为主，尽量在编译期消解抽象成本；
- Device 层采用固定容量设备表，避免默认堆分配；
- 类型管理尽量不依赖 RTTI；
- 对线程、线程池、`std::function` 等额外成本给出明确边界与建议。

这使项目既保留了现代 C++ 的工程表达力，又保持了对 MCU 资源边界的尊重。

---

## Getting Started

### 1. 接入方式

`dp_sdk_core` 通过父项目以 `add_subdirectory()` 的方式接入。

```cmake
# 顶层 CMakeLists.txt
set(DP_OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)   # 可选，默认 builtin_posix
add_subdirectory(core/dp_sdk_core)

target_link_libraries(my_app PRIVATE dp_osal)    # OS 抽象
target_link_libraries(my_app PRIVATE dp_hal)     # 硬件抽象
target_link_libraries(my_app PRIVATE dp_device)  # 设备管理
```

典型构建方式：

```sh
cmake -B build -DPRODUCT=posix_demo
cmake --build build
```

### 2. 推荐工作流

推荐的使用路径如下：

1. 在 Linux / POSIX 环境先把核心业务逻辑跑通
2. 使用统一的 OSAL / HAL 接口保持上层编程方式稳定
3. 通过 CMSIS-OS 适配把不同 RTOS 纳入统一模型
4. 最后切换到具体硬件实现，在 MCU 上完成联调与资源测量

这种工作流非常适合先在 host 侧做验证，再逐步落到 RTOS 与目标硬件平台。

---

## 模块细节

## 模块一：dp_osal

`dp_osal` 负责统一系统原语接口。主要接口包括：

| 接口头文件 | 主要能力 |
|---|---|
| `interface_thread.h` | 线程启动、停止、join、detach、优先级控制 |
| `interface_mutex.h` | 互斥锁、tryLock、超时获取 |
| `interface_condition_variable.h` | 条件变量 |
| `interface_lockguard.h` | RAII 锁守卫 |
| `interface_rwlock.h` | 读写锁 |
| `interface_semaphore.h` | 计数信号量 |
| `interface_spin_lock.h` | 自旋锁 |
| `interface_queue.h` | 队列发送 / 接收 / 超时接收 |
| `interface_timer.h` | 单次 / 周期定时器 |
| `interface_thread_pool.h` | 线程池 |
| `interface_chrono.h` | 单调时钟与时间转换 |
| `interface_system.h` | 调度器、sleep、critical section、系统信息 |
| `dp_osal_virtual.h` | host 测试用虚接口包装 |

### 后端模型

```text
src/impl/posix/     ← DP_OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← DP_OSAL_BACKEND_CMSIS_OS
```

CMake 会根据 `dp_osal_port.h` 中启用的宏自动选择后端。

| 后端 | 适用场景 |
|---|---|
| `DP_OSAL_BACKEND_POSIX` | Linux、macOS、POSIX 主机环境 |
| `DP_OSAL_BACKEND_CMSIS_OS` | 通过 CMSIS-OS 对接的 RTOS 环境 |

### 为什么选择 CMSIS-OS 作为 RTOS 收敛层

在 RTOS 侧，项目并不要求上层业务直接面对每一种系统的原生 API，而是通过 CMSIS-OS 做统一抽象收敛。

这样做的价值在于：

- 上层业务接口更稳定；
- RTOS 切换时不需要重写业务逻辑；
- Linux / POSIX 与多 RTOS 环境之间可以共享更一致的系统编程模型。

### `dp_osal_port.h` 端口模板

端口模板主要包含五部分：

1. 后端选择
2. 平台头文件
3. 平台常量
4. 调试输出
5. 测试开关

典型用法是复制模板到项目自己的 port 目录中，并通过 `DP_OSAL_PORT_DIR` 指向该目录。

---

## 模块二：dp_hal

`dp_hal` 负责统一硬件外设接口，其特点包括：

- 核心接口采用 **header-only + CRTP**
- 生产路径尽量避免额外运行时分发成本
- 测试 / 注入场景可通过虚接口包装实现

当前主要接口包括：

| 头文件 | 说明 |
|---|---|
| `dp_uart.h` | UART |
| `dp_gpio.h` | GPIO |
| `dp_spi.h` | SPI 总线与设备 |
| `dp_i2c.h` | I2C 总线与寄存器访问辅助 |
| `dp_adc.h` | ADC |
| `dp_dac.h` | DAC |
| `dp_can.h` | CAN |
| `dp_timer.h` | Timer |
| `dp_pwm.h` | PWM |
| `dp_hal_power.h` | 电源管理 mixin |
| `dp_hal_virtual.h` | 测试 / 注入用虚接口 |

### HAL 端口解析顺序

`dp_hal` 的移植目录按以下顺序解析：

1. `platform/<KERNEL_PORT>/hal_port/`
2. `platform/common/hal_port/`
3. `hal/port/mock/`

这使其能够同时支持：

- 板级 / 产品级固件移植
- 通用共享移植
- host 测试用 mock 实现

---

## 模块三：dp_device

`dp_device` 负责统一设备层管理，而不是直接承担底层驱动实现。

核心能力包括：

- `Device`：统一设备名称、类型与 open/close 引用计数
- `DeviceManager`：统一设备注册、查找、遍历、注销
- 适配器类：将 HAL 实现桥接为统一管理的设备对象

### 设计特点

- 设备名称长度上限明确
- 使用固定容量注册表，避免默认堆分配
- 管理器具备线程安全能力
- 通过 `DeviceType` 管理类型，不依赖 RTTI

这种设计使设备生命周期不再分散在各个业务模块中，而是进入统一框架。

---

## 测试方式

项目测试采用“模块静态库 + 宿主入口函数”的组织方式，不依赖独立测试二进制。

入口包括：

```c
extern "C" void dp_osal_test_main(void);
extern "C" void dp_hal_test_main(void);
extern "C" void dp_device_test_main(void);
```

测试相关特点：

- OSAL 支持全量或按组件启用测试
- host 场景可通过 `dp_osal_virtual.h` / `dp_hal_virtual.h` 做依赖注入
- 测试入口采用单翻译单元模式，避免 GTest 静态注册被链接器裁掉

---

## 资源成本与 MCU 可行性

项目对资源成本的处理方式是：**明确边界、支持裁剪、基于实测判断可行性**。

### 已知成本特征

- HAL 接口以 CRTP 为主，目标是在编译期消解大部分抽象成本
- OSAL 中的部分对象，尤其是 `Thread`，会因 `std::function` 和状态管理带来额外 RAM / Flash 开销
- 虚接口包装主要用于 host 测试，不建议作为量产默认抽象路径

### 单片机可行性参考

| 目标芯片 | 可行性 | 说明 |
|---|---|---|
| Cortex-M4 / M7，RAM ≥ 192 KB（建议带 CCM） | ✅ 当前全量测试配置可行 | 已有构建与验证基础 |
| Cortex-M3 / 小内存 M4，RAM 64–128 KB | ⚠️ 仅在强裁剪下可行 | 需要关闭 `test_all`、减少功能并重新量化 |
| Cortex-M0 / M0+，RAM ≤ 64 KB | ❌ 当前配置不可直接宣称可用 | 需要 tiny 配置与实测数据支撑 |

### 使用建议

- Release 构建建议启用 `-Os -fno-exceptions -fno-rtti` 与 LTO
- 小内存目标应控制 `Thread` 使用方式，谨慎评估 `std::function` 捕获成本
- 无明确并发读需求时，优先使用 `Mutex` 而不是 `RWLock`
- 将测试配置与量产配置分离，不要直接将全量测试配置视为量产资源基线

---

## 适用场景

`dp_sdk_core` 适合以下类型的项目：

- 业务逻辑需要同时在 Linux 主机和 MCU 上验证
- 产品需要支持多个 RTOS 或多个硬件平台
- 团队已经为平台胶水代码付出较高维护成本
- 希望在 C++17 工程中建立长期可复用的基础层
- 需要更一致的 Linux / RTOS 编程接口体验

---

## License

[MIT](./LICENSE)
