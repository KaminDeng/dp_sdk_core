# CLAUDE.md -- kernel/hal (dp_hal)

## Overview

Zero-cost C++ CRTP device abstraction layer for embedded peripherals.
All interfaces are header-only templates; concrete implementations live in
port directories.

## Directory Structure

```
kernel/hal/
  include/          -- CRTP interface headers (dp_uart.h, dp_gpio.h, ...)
  port/
    template/       -- Port template with #error sentinels
    mock/           -- Mock implementations for host GTest
  test/             -- GTest suites (#included in dp_hal_test_main.cpp)
  CMakeLists.txt    -- INTERFACE library + port resolution + test toggle
```

## Key Interfaces

| Header | Interface | Description |
|--------|-----------|-------------|
| `dp_uart.h` | `UartBase<Impl>` | UART: configure, read, write, flush, async callbacks |
| `dp_gpio.h` | `GpioPinBase<Impl>` | GPIO: setMode, read, write, toggle, IRQ |
| `dp_spi.h` | `SpiBusBase<Impl>` + `SpiDevice<Bus,Cs>` | SPI bus + slave device with CS toggle |
| `dp_i2c.h` | `I2cBusBase<Impl>` + `I2cDevice<Bus>` | I2C bus + register read/write helpers |
| `dp_adc.h` | `AdcBase<Impl>` | ADC: configure, read, continuous mode |
| `dp_dac.h` | `DacBase<Impl>` | DAC: configure, write |
| `dp_timer.h` | `TimerBase<Impl>` | Timer: start, stop, callback, counter |
| `dp_hal_power.h` | `PowerManageable<Impl>` | Optional power management mixin |
| `dp_hal_virtual.h` | `IUart`, `IGpioPin`, ... | Optional virtual wrappers for GMock |

## CRTP Pattern

Every interface uses the Curiously Recurring Template Pattern:

```cpp
template <typename Impl>
class UartBase {
public:
    Status write(const uint8_t* buf, size_t len) { return impl().doWrite(buf, len); }
private:
    Impl& impl() { return *static_cast<Impl*>(this); }
};
```

Implementations provide private `do*` methods and declare `friend class UartBase<Self>`.

## Port System

Port resolution (3-tier fallback):
1. `platform/<KERNEL_PORT>/hal_port/` -- product-specific
2. `platform/common/hal_port/` -- shared default
3. `kernel/hal/port/mock/` -- built-in mock

Each port provides:
- `dp_hal_port_impl.cpp` -- port functions (time, log, assert)
- Device implementation headers (e.g., `mock_uart.h`)
- `hal_port.h` -- typedef bindings (`using DpUart = ...`)
- `CMakeLists.txt` -- `dp_hal_port_impl` STATIC target

## Testing

- **Test option**: `-DDP_HAL_BUILD_TESTS=ON`
- **GTest filter**: `DpHalUart*:DpHalGpio*:DpHalSpiBus*:DpHalSpiDevice*:DpHalI2cBus*:DpHalI2cDevice*:DpHalAdc*:DpHalDac*:DpHalTimer*:DpHalPower*`
- **Pattern**: all test .cpp files #included in `dp_hal_test_main.cpp` (single TU)
- **Total**: 10 suites covering all interfaces + power mixin

## Cross-Platform Module Compliance

- `dp_hal_compat.h` with `DP_HAL_` prefix macros
- Port isolation: `include/` has zero platform API calls
- Fixed-width types throughout (`uint8_t*`, `size_t`, `uint32_t`)
- No VLA (I2cDevice uses fixed 33-byte buffer with DP_HAL_ASSERT)
- CMakeLists.txt has zero platform flags
