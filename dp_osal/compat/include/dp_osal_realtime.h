
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif


#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "dp_osal_port.h"
#include "dp_osal_thread.h"

namespace dp::osal {

using RealtimeEntry = ThreadEntry;

enum class RealtimeStatus : uint8_t {
    kOk = 0,
    kInvalidArgument,
    kInvalidState,
    kAlreadyStarted,
    kStartFailed,
};

template <typename ThreadType = Thread>
class RealtimeThreadWithBackend {
public:
    RealtimeThreadWithBackend() = default;
    RealtimeThreadWithBackend(const RealtimeThreadWithBackend&) = delete;
    RealtimeThreadWithBackend& operator=(const RealtimeThreadWithBackend&) = delete;

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
#if defined(DP_OSAL_PORT_PERSISTENT_REALTIME_THREAD)
        if constexpr (std::is_same<ThreadType, Thread>::value) {
            if (dp_osal_port_start_persistent_realtime_thread(entry, context, priority, caller_stack,
                                                               static_cast<uint32_t>(stack_size)) == nullptr) {
                return RealtimeStatus::kStartFailed;
            }
            started_ = true;
            consumed_ = true;
            persistent_ = true;
            return RealtimeStatus::kOk;
        }
#endif
        const int result = thread_.startRaw("dp_realtime", entry, context, priority,
                                            static_cast<int>(stack_size), caller_stack);
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
        if (persistent_) return RealtimeStatus::kInvalidState;
        thread_.join();
        started_ = false;
        return RealtimeStatus::kOk;
    }

private:
    ThreadType thread_;
    bool started_ = false;
    bool consumed_ = false;
    bool persistent_ = false;
};

using RealtimeThread = RealtimeThreadWithBackend<>;

}  // namespace dp::osal
