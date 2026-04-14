// core/dp_sdk_core/osal/src/impl/linux/include/osal_timer.h
// Linux OSAL timer — delegates to posix impl (std::thread + std::chrono).
//
// Linux-specific upgrade path (timerfd):
//   On Linux, high-precision timers can use timerfd_create(2) instead of a
//   dedicated std::thread per timer, allowing epoll integration:
//
//     #include <sys/timerfd.h>
//     int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
//     struct itimerspec its = { .it_interval = {0, interval_ns},
//                               .it_value    = {0, interval_ns} };
//     timerfd_settime(tfd, 0, &its, NULL);
//     // expiry: read(tfd, &expirations, sizeof(expirations))
//     // or integrate with epoll_wait for zero-thread timer dispatch
//
//   timerfd_create is Linux-only (not available on macOS/BSD).
//   Benefits: O(1) dispatch via epoll, no per-timer thread, sub-millisecond
//   precision without busy-wait.
//
// Current: reuse posix (std::thread + std::cv) for cross-platform correctness.
#pragma once
#include "../../posix/include/osal_timer.h"
