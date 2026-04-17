// core/dp_sdk_core/osal/src/impl/linux/include/osal_semaphore.h
// Linux OSAL semaphore — delegates to posix impl (std::mutex + condition_variable).
//
// Linux-specific upgrade path (eventfd):
//   On Linux, semaphore signaling can use eventfd(2) instead of condition variables
//   for lower latency in high-throughput scenarios:
//
//     #include <sys/eventfd.h>
//     int efd = eventfd(initial_count, EFD_SEMAPHORE | EFD_NONBLOCK);
//     // signal:  write(efd, &one, sizeof(one))
//     // wait:    read(efd, &val, sizeof(val))  — blocks if count == 0
//     // timedwait: poll(fds, 1, timeout_ms)   — then read()
//
//   This approach avoids mutex overhead and is more efficient when crossing
//   process boundaries (e.g., via epoll integration).
//   EFD_SEMAPHORE is Linux-only (not available on macOS/BSD).
//
// Current: reuse posix (std::condition_variable) for cross-platform correctness.
#pragma once
#include "../../posix/include/osal_semaphore.h"
