/**
 * @file dp_osal_realtime.h
 * @brief Bounded-entry thread facade for realtime loops.
 *
 * The caller supplies the execution stack. Backends may allocate operating
 * system control objects during start/join; the entry loop itself receives a
 * raw function pointer and does not require further allocation by this API.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

#include "dp_osal_port.h"
#include "dp_osal_thread.h"

namespace dp::osal {

using RealtimeEntry = void (*)(void*);

enum class RealtimeStatus : uint8_t {
    kOk = 0,
    kInvalidArgument,
    kInvalidState,
    kAlreadyStarted,
    kStartFailed,
};

class RealtimeThread {
public:
    RealtimeThread() = default;
    RealtimeThread(const RealtimeThread&) = delete;
    RealtimeThread& operator=(const RealtimeThread&) = delete;

    RealtimeStatus start(RealtimeEntry entry, void* context, int priority,
                         void* caller_stack, size_t stack_size) {
        if (entry == nullptr || caller_stack == nullptr ||
            stack_size < static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE) ||
            stack_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return RealtimeStatus::kInvalidArgument;
        }
        if (started_) {
            return RealtimeStatus::kAlreadyStarted;
        }
        if (consumed_) {
            return RealtimeStatus::kInvalidState;
        }
        const int result = thread_.start("dp_realtime", std::function<void(void*)>(entry), context,
                                         priority, static_cast<int>(stack_size), caller_stack);
        if (result != 0) {
            return RealtimeStatus::kStartFailed;
        }
        started_ = true;
        consumed_ = true;
        return RealtimeStatus::kOk;
    }

    RealtimeStatus join() {
        if (!started_) {
            return RealtimeStatus::kInvalidState;
        }
        thread_.join();
        started_ = false;
        return RealtimeStatus::kOk;
    }

private:
    Thread thread_;
    bool started_ = false;
    bool consumed_ = false;
};

}  // namespace dp::osal
