#pragma once
#include <cstdint>
namespace dp::osal {
enum class HostWakeResult : uint8_t { kSignaled, kTimeout, kError };
template <typename Impl>
class HostWakeBase {
public:
    bool open() noexcept { return impl().doOpen(); }
    bool signal() noexcept { return impl().doSignal(); }
    HostWakeResult wait_for_ms(uint32_t milliseconds) noexcept { return impl().doWaitForMs(milliseconds); }
    bool close() noexcept { return impl().doClose(); }
protected:
    ~HostWakeBase() = default;
private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
} // namespace dp::osal
