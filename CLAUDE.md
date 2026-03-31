# CLAUDE.md -- dp_sdk_core

Unified core abstraction layer for dpsdk, containing three modules:

- **osal** -- C++17 OS Abstraction Layer (POSIX / CMSIS-OS2 backends)
- **hal** -- Zero-cost CRTP hardware abstraction (header-only interfaces)
- **device** -- Device management framework (registration, lifecycle, shell integration)

## Git Remote

- **URL**: git@gitee.com:kamin_self/osal.git (will be renamed to dp_sdk_core)
- **Role in parent**: submodule at `core/dp_sdk_core`

## Directory Layout

```
dp_sdk_core/
├── osal/                  # OS Abstraction Layer
│   ├── src/interface/     # 13 abstract C++ interfaces
│   ├── src/impl/posix/    # POSIX backend
│   ├── src/impl/cmsis_os/ # CMSIS-OS2 backend
│   ├── port/              # builtin_posix + template
│   ├── example/           # Port config examples
│   └── test/gtest/        # GTest suites
├── hal/                   # Hardware Abstraction Layer
│   ├── include/           # CRTP interfaces (dp_uart.h, dp_gpio.h, ...)
│   ├── port/mock/         # GMock implementations
│   ├── port/template/     # Port template
│   └── test/              # GTest suites
├── device/                # Device Management
│   ├── include/           # Device, DeviceManager, adapters
│   ├── src/               # Implementation
│   └── test/              # GTest suites
└── CMakeLists.txt         # Unified build entry
```

## Build

Not built standalone -- consumed by dpsdk via `add_subdirectory(core/dp_sdk_core)`.

## Cross-Platform Module Compliance

All three modules follow cross-platform-module skill conventions:
- Compat headers with module-specific prefixes
- Port isolation (zero platform API in src/)
- Fixed-width types throughout
