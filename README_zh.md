# dp_sdk_core

嵌入式系统统一核心抽象层。使用 C++17 编写一次可移植的应用代码，通过修改配置文件即可切换 OS 后端和硬件目标。

包含三个模块：

| 模块 | 职责 | 设计模式 |
|------|------|----------|
| **osal** | OS 原语（线程、互斥锁、队列、定时器等） | 零开销 CRTP + 后端选择 |
| **hal** | 硬件外设（UART、GPIO、SPI、I2C 等） | 零开销 CRTP 模板 |
| **device** | 设备注册与生命周期 | 适配器模式 + 单例管理器 |

**依赖链：** `device → hal（public）+ osal（private）` · `hal` 与 `osal` 互不依赖。

---

## 快速开始

dp_sdk_core 由父项目（dpsdk）通过 `add_subdirectory()` 引入，不独立构建。

```cmake
# 在顶层 CMakeLists.txt 中：
set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)   # 可选，默认使用 builtin_posix
add_subdirectory(core/dp_sdk_core)

target_link_libraries(my_app PRIVATE osal)       # OS 抽象
target_link_libraries(my_app PRIVATE dp_hal)     # 硬件抽象
target_link_libraries(my_app PRIVATE dp_device)  # 设备管理
```

从父目录 dpsdk 构建：

```sh
cmake -B build -DPRODUCT=posix_demo && cmake --build build
```

---

## 模块一 — osal（OS 抽象层）

### 接口一览

| 接口头文件 | 抽象类 | 主要操作 |
|-----------|--------|----------|
| `interface_thread.h` | `ThreadBase<Impl>` | `start`、`stop`、`join`、`detach`、`suspend`、`resume`、`setPriority` |
| `interface_mutex.h` | `MutexBase<Impl>` | `lock`、`unlock`、`tryLock`、`tryLockFor` |
| `interface_condition_variable.h` | `ConditionVariableBase<Impl>` | `wait`、`waitFor`、`notifyOne`、`notifyAll` |
| `interface_lockguard.h` | `LockGuard<MutexType>` | RAII 互斥锁守卫 |
| `interface_rwlock.h` | `RWLockBase<Impl>` | 读写锁，含 try 和超时变体 |
| `interface_semaphore.h` | `SemaphoreBase<Impl>` | 计数信号量 |
| `interface_spin_lock.h` | `SpinLockBase<Impl>` | 自旋锁（atomic_flag）|
| `interface_queue.h` | `QueueBase<Impl, T>` | `send`、`receive`、`tryReceive`、`receiveFor`、`size`、`clear` |
| `interface_memory_manager.h` | `MemoryManagerBase<Impl>` | 块内存池：`allocate`、`deallocate`、`allocateAligned` |
| `interface_timer.h` | `TimerBase<Impl>` | 单次和周期定时器 |
| `interface_thread_pool.h` | `ThreadPoolBase<Impl>` | 动态线程池，含任务队列 |
| `interface_chrono.h` | `ChronoBase<Impl>` | 单调时钟：`elapsed`、`to_time_t`、`from_time_t` |
| `interface_system.h` | `SystemBase<Impl>` | `schedulerStart`、`sleep_ms`、`sleep_s` |
| `osal_virtual.h` | `IMutex` + `MutexVirtual<T>`（及其他） | 仅 host 测试的可选虚包装（注入 / GMock） |

CRTP 接口不引入 vtable。具体实现在编译/链接阶段由
`osal_port.h` 中选择的后端决定。

### 后端选择

```
src/impl/posix/     ← OSAL_BACKEND_POSIX
src/impl/cmsis_os/  ← OSAL_BACKEND_CMSIS_OS
```

CMake 通过扫描 `osal_port.h` 中未注释的 `#define OSAL_BACKEND_CMSIS_OS`
行自动检测当前后端，并编译对应的源码树。每次构建只编译一个后端。

| 后端 | 适用场景 |
|------|----------|
| `OSAL_BACKEND_POSIX` | Linux、macOS、任何 POSIX 主机 |
| `OSAL_BACKEND_CMSIS_OS` | FreeRTOS+CMSIS-OS2、Zephyr、ThreadX 等 |

### OSAL 移植系统

在 `add_subdirectory(osal)` 之前设置 `OSAL_PORT_DIR`。默认使用
`port/builtin_posix/`（Linux/macOS 零配置）。

`osal_port.h` 共五节：

| 节 | 内容 |
|----|------|
| 1 — 后端选择 | `#define OSAL_BACKEND_POSIX` 或 `OSAL_BACKEND_CMSIS_OS`（二选一） |
| 2 — 平台头文件 | CMSIS-OS：`#include "cmsis_os2.h"`（POSIX：留空） |
| 3 — 平台常量 | `OSAL_PORT_THREAD_MIN_STACK_SIZE`、`OSAL_PORT_THREAD_DEFAULT_PRIORITY` |
| 4 — 调试输出 | `inline void osal_port_debug_write(const char* buf, uint32_t len)` |
| 5 — 测试开关 | `OSAL_TEST_ALL` 或逐组件 `OSAL_TEST_*_ENABLED` 标志 |

### 移植示例

| 目录 | 平台 | 后端 | 调试输出 |
|------|------|------|----------|
| `osal_port_freertos/` | STM32 裸机 + FreeRTOS | CMSIS-OS2 | `HAL_UART_Transmit`（UART6）|
| `osal_port_sim_freertos/` | FreeRTOS-on-POSIX 模拟器 | CMSIS-OS2 | `write(1, …)`（stdout）|
| `osal_port_zephyr/` | Zephyr RTOS | CMSIS-OS2（内置）| `printk` |

选取最接近目标平台的示例，复制后按需修改即可。

---

## 模块二 — hal（硬件抽象层）

### CRTP 接口

所有接口均为 header-only 模板，零运行时开销：

| 头文件 | 接口 | 描述 |
|--------|------|------|
| `dp_uart.h` | `UartBase<Impl>` | UART：配置、读写、flush、RX/TX 回调、DMA |
| `dp_gpio.h` | `GpioPinBase<Impl>` | GPIO：模式设置、读写、翻转、IRQ 边沿触发 |
| `dp_spi.h` | `SpiBusBase<Impl>` + `SpiDevice<Bus,Cs>` | SPI 总线 + 从设备（自动 CS 切换） |
| `dp_i2c.h` | `I2cBusBase<Impl>` + `I2cDevice<Bus>` | I2C 总线 + 寄存器读写辅助 |
| `dp_adc.h` | `AdcBase<Impl>` | ADC：配置、单次读取、连续模式 |
| `dp_dac.h` | `DacBase<Impl>` | DAC：配置、写入输出值 |
| `dp_can.h` | `CanBase<Impl>` | CAN 总线：配置、发送、接收、过滤器 |
| `dp_timer.h` | `TimerBase<Impl>` | 定时器：启停、周期回调、计数器 |
| `dp_pwm.h` | `PwmBase<Impl>` | PWM：启停、频率和占空比控制 |
| `dp_hal_power.h` | `PowerManageable<Impl>` | 可选电源管理 mixin |
| `dp_hal_virtual.h` | `IUart`、`IGpioPin` 等 | 虚接口封装，用于依赖注入 / GMock |

### CRTP 模式

```cpp
template <typename Impl>
class UartBase {
public:
    Status write(const uint8_t* buf, size_t len) { return impl().doWrite(buf, len); }
private:
    Impl& impl() { return *static_cast<Impl*>(this); }
};
```

实现类提供私有 `do*` 方法并声明 `friend class UartBase<Self>`。

### HAL 移植系统

端口解析（三层回退）：
1. `platform/<KERNEL_PORT>/hal_port/` — 产品定制
2. `platform/common/hal_port/` — 共享默认
3. `hal/port/mock/` — 内置 Mock（宿主机测试）

每个移植目录包含：
- `hal_port.h` — typedef 绑定（如 `using DpUart = mock::MockUart;`）
- 设备实现头文件（如 `mock_uart.h`、`posix_uart.h`）
- `dp_hal_port_impl.cpp` — 移植函数（`dp_hal_time_us`、`dp_hal_log`、`dp_hal_assert_fail`）
- `CMakeLists.txt` — 构建 `dp_hal_port_impl` 静态库

---

## 模块三 — device（设备管理）

### 架构

- **Device** 基类：名称（最长 32 字符）+ 类型 + open/close 引用计数
- **适配器** 将 HAL 实现桥接到 Device 并提供虚接口：

| 适配器类 | HAL 接口 | DeviceType |
|---------|----------|------------|
| `SerialDevice<HalImpl>` | `IUart` | `kSerial` |
| `PinDevice<HalImpl>` | `IGpioPin` | `kPin` |
| `SpiBusDevice<HalImpl>` | `ISpi` | `kSpiBus` |
| `I2cBusDevice<HalImpl>` | `II2c` | `kI2cBus` |
| `AdcDevice<HalImpl>` | `IAdc` | `kAdc` |
| `DacDevice<HalImpl>` | `IDac` | `kDac` |
| `TimerDevice<HalImpl>` | `ITimer` | `kTimer` |

- **DeviceManager** 单例：线程安全注册表（OSAL Mutex 保护），固定数组（最多 32 个设备，无堆分配），支持名称和类型查找。无 RTTI——使用 `DeviceType` 枚举 + `static_cast`。

---

## 运行测试

测试编译进静态库，在宿主应用的任务上下文中运行（无独立测试二进制文件）。入口函数：

```c
extern "C" void osal_test_main(void);      // OSAL 测试套件入口
extern "C" void dp_hal_test_main(void);    // 10 个 HAL 套件（需 -DDP_HAL_BUILD_TESTS=ON）
extern "C" void dp_device_test_main(void); // 5 个 Device 套件（需 -DDP_DEVICE_BUILD_TESTS=ON）
```

从父目录 dpsdk 构建并运行：

```sh
cmake -B build -DPRODUCT=posix_demo -DDP_HAL_BUILD_TESTS=ON -DDP_DEVICE_BUILD_TESTS=ON
cmake --build build
./build/dpsdk_firmware
```

### 测试控制（OSAL）

在 `osal_port.h` 中：
- `OSAL_TEST_ALL = 1` — 运行全部 OSAL 套件（首次移植推荐）
- `OSAL_TEST_ALL = 0` — 使用逐组件开关（`OSAL_TEST_THREAD_ENABLED` 等）

### Host 注入示例（OSAL）

`osal/src/interface/osal_virtual.h` 提供了 CRTP 对象上的虚接口包装，便于 host
测试做依赖注入：

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
CounterService svc(injected);  // 业务只依赖 IMutex 抽象
```

### 单翻译单元模式

所有测试 `.cpp` 文件通过 `#include` 引入对应模块的 `*_test_main.cpp`，防止链接器死代码剥离 GTest 静态注册。**不要**将测试文件直接加入 CMake 源文件列表。

在 **macOS** 上，stdout 在非 TTY 时会被全量缓冲，使用 `script(1)` 分配伪终端：

```sh
script -q /tmp/out.txt ./build/dpsdk_firmware 2>/dev/null &
sleep 10 && kill %1 && cat /tmp/out.txt
```

---

## 单片机资源消耗

以下数据基于 32-bit Cortex-M + CMSIS-OS2 后端，并区分为：
- OSAL 结构性开销（对象级）
- 当前 dpsdk 产品配置的整机实测占用

### RAM — 每个 OSAL 对象实例的额外字节

| 组件 | 直接 OS 句柄 | OSAL 对象 | 额外 RAM |
|------|------------|-----------|---------|
| `OSALMutex` | 4 B（`osMutexId_t`）| 4 B | **+0 B** |
| `OSALSemaphore` | 4 B | 4 B | **+0 B** |
| `OSALThread` | 4 B（`osThreadId_t`）| ≥ 44 B | **+40 B** |
| `OSALConditionVariable` | ~12 B | 12 B | ~0 B |
| `OSALRWLock` | ~20 B | 20 B | ~0 B |
| `OSALMessageQueue<T>` | 4 B | 4 B | **+0 B** |
| `OSALLockGuard` | 0 B（栈）| 8 B（栈）| **+8 B**（栈）|
| `OSALSpinLock` | 4 B | 4 B | **+0 B** |

**`OSALThread` 额外开销最大（+40 B）**：`std::function` 16–32 B 内缓冲区（捕获 > ~16 B 触发堆分配）、`exitSemaphore` 用于 `join()`、两个 `std::atomic<bool>` 字段。

HAL 接口 **零额外开销** — CRTP 方法完全内联消除。

### Flash — 代码体积增量

| 来源 | 估算大小 |
|------|---------|
| OSAL 组件 vtable | **0 B**（CRTP 消除虚分发） |
| `std::function` 每种 lambda 签名实例化 | 200–500 B/种 |
| C++ 运行时（构造/析构函数）| **2–4 KB** |
| **OSAL 总额外开销（典型值，`-Os`）** | **约 4–8 KB Flash** |

### 实测占用（`dp_stm32f427_dev`，2026-04-05）

构建配置：
- `PRODUCT=dp_stm32f427_dev`
- `PRODUCT_APP=all_tests`
- overlays 包含 `test_all`（调试/验证配置，不是量产裁剪配置）
- 统计命令：`arm-none-eabi-size build_stm32_full/dpsdk_firmware.elf`

结果：
- `text=952480`，`data=49996`，`bss=186440`
- Flash 估算（`text+data`）：`1,002,476 B` / `2,097,152 B` = **47.8%**
- 主 SRAM 估算（`.data + .bss + heap/stack`）：`187,184 B` / `196,608 B` = **95.2%**
- CCMRAM（`.ccmram`）：`49,152 B` / `65,536 B` = **75.0%**

量产向基线（`dp_stm32f427_dev_interactive`，Release）：
- `text=93,232`，`data=204`，`bss=27,488`
- Flash 估算（`text+data`）：`93,436 B` / `2,097,152 B` = **4.5%**
- 主 SRAM 估算（`.data + .bss + heap/stack`）：`27,656 B` / `196,608 B` = **14.1%**
- CCMRAM（`.ccmram`）：`0 B` / `65,536 B` = **0.0%**

### 单片机可行性（重评）

| 目标芯片 | 可行性 | 说明 |
|---------|--------|------|
| Cortex-M4 / M7，RAM ≥ 192 KB（建议带 CCM） | ✅ **当前全量测试配置可行** | `dp_stm32f427_dev + all_tests` 已完成构建与验证 |
| Cortex-M3 / 小内存 M4，RAM 64–128 KB | ⚠️ **仅在强裁剪下可行** | 必须关闭 `test_all`，叠加最小化 overlays，并重新量化 |
| Cortex-M0 / M0+，RAM ≤ 64 KB | ❌ **当前配置不可行** | 在没有 tiny 专用配置与实测前，不应宣称可支持 |

**优化优先级：**
- 量产构建不要使用 `all_tests + test_all`，应切到产品 app + minimal overlays。
- Release 启用 `-Os -fno-exceptions -fno-rtti` + LTO。
- `OSALThread` 的 lambda 捕获控制在 16 B 内，避免堆回退。
- 无并发读需求时优先 `OSALMutex`，不要默认使用 `OSALRWLock`。
- 面向 M3/M0 目标时，优先评估将线程入口从 `std::function` 改为定长可调用包装。

**使用建议：**
- 启用 `-Os -fno-exceptions -fno-rtti`（节省 1–3 KB Flash）。
- lambda 捕获列表保持 < 16 B，避免 `OSALThread` 触发堆分配。
- 不需要并发读时用 `OSALMutex` 代替 `OSALRWLock`。
- `OSALRWLock` 单次 `readLock()` 产生最多 5 次 CMSIS-OS2 调用（直接实现通常 1–2 次）。
