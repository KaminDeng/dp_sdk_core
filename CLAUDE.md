# CLAUDE.md — kernel/osal

Portable C++17 OS Abstraction Layer (OSAL) for embedded systems.

- **Git remote**: git@gitee.com:kamin_self/osal.git
- **Role in parent project**: submodule at `kernel/osal`; integrated by `kernel/CMakeLists.txt`

## Directory Layout

```
kernel/osal/
├── src/
│   ├── interface/          # 13 abstract C++ interfaces (one header per component)
│   ├── impl/
│   │   ├── posix/          # POSIX backend — pthreads, no RTOS kernel
│   │   └── cmsis_os/       # CMSIS-OS2 backend — maps to FreeRTOS or RT-Thread
│   └── osal.h              # Top-level include
├── port/
│   ├── builtin_posix/      # Ready-to-use POSIX port (osal_port.h)
│   └── template/           # Port template with #error sentinels
├── example/                # Example port configs (freertos, sim_freertos, zephyr)
└── test/
    ├── gtest/              # One .cpp per component (gtest_<component>.cpp)
    ├── osal_test_main.cpp
    └── osal_test_main.h
```

## 13 OSAL Components

Chrono, ConditionVariable, LockGuard, MemoryManager, Mutex, Queue, RWLock, Semaphore, SpinLock, Thread, ThreadPool, Timer

Each has a corresponding `src/interface/interface_<name>.h` and `test/gtest/gtest_<name>.cpp`.

## Backend Selection

Backend is controlled by the consuming port's `osal_port.h`:
- `#define OSAL_BACKEND_POSIX` — selects `src/impl/posix/`
- `#define OSAL_BACKEND_CMSIS_OS` — selects `src/impl/cmsis_os/`

Parent project ports live in `platform/<port-name>/kernel_port/osal_port.h`.

## Adding a New RTOS Port

1. Create `platform/<new-port>/kernel_port/osal_port.h` in the parent project
2. Set `OSAL_BACKEND_POSIX` or `OSAL_BACKEND_CMSIS_OS` based on which backend fits
3. Add a corresponding `products/<new-port>.cmake`
4. For a new RTOS with custom primitives: extend `src/impl/` with a new backend

## Tests

- Location: `test/gtest/` — one file per component
- Run via parent project: set `USER_DEMO "OSAL_TEST"` in root `CMakeLists.txt`
- All test `.cpp` files are `#include`d in `osal_test_main.cpp` (prevents linker dead-stripping)
- GTest filter set in `osal_test_main.cpp` restricts to OSAL suites only

## Build and Run

This submodule is not built standalone — it is consumed by the parent project (`freertos_dpcpp`).
Refer to the parent project's `CLAUDE.md` for full build commands and CMake integration details.

```bash
# Parent project — run OSAL tests (from freertos_dpcpp root)
# Edit CMakeLists.txt: set(USER_DEMO "OSAL_TEST")
cmake -B build -DPRODUCT=posix_demo -DCMAKE_BUILD_TYPE=Debug && cmake --build build
./build/dpsdk_firmware > /tmp/t.txt 2>&1 & PID=$!
for i in $(seq 1 60); do sleep 1; ! kill -0 $PID 2>/dev/null && break
  grep -q "test suites ran." /tmp/t.txt 2>/dev/null && break; done
kill $PID 2>/dev/null; wait $PID 2>/dev/null
grep -q "test suites ran." /tmp/t.txt && echo "COMPLETED" || echo "TIMEOUT/DEADLOCK"
```

## Memory

See `docs/claude-memory/MEMORY.md` for known fixes and architecture notes.

## Cross-Platform Module 约束

> **本库遵循 `cross-platform-module` skill 规范。新建/修改代码前必须加载该 skill。**

关键约束：compat header、port 隔离（src/ 内零平台 API）、固定宽度类型、CMake 分层。
完整检查清单通过 skill 加载获取。
