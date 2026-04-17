# dp_device

## Overview
Device management framework on top of dp_hal. Provides device registration,
name-based lookup, lifecycle (open/close/refcount), and dp_shell shell commands.

## Build
- Part of dpsdk: `cmake -B build -DPRODUCT=posix_demo && cmake --build build`
- Tests: `-DDP_DEVICE_BUILD_TESTS=ON` (via `test_all` overlay)

## Architecture
- Device base class: name + type + open/close refcount (no read/write)
- Per-class adapters: SerialDevice<Impl>, PinDevice<Impl>, etc.
- DeviceManager: singleton registry with OSAL Mutex protection
- Each adapter inherits Device + dp_hal virtual interface (IUart, IGpioPin, etc.)
- No RTTI: DeviceType enum + static_cast for type-safe lookup

## Cross-Platform
- dp_device_compat.h with DP_DEV_ prefix
- src/ uses OSAL for OS primitives, no direct RTOS/POSIX calls
- Fixed-width types throughout
