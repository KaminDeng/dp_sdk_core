#pragma once

#include <cstddef>
#include <cstdint>

namespace dp::osal {
enum class HostIoResult : uint8_t { kOk, kWouldBlock, kClosed, kError };

// 仅Host有界字节输出；构造不取得资源，关闭与错误均可观察。
template <typename Impl>
class HostIoBase {
public:
    bool open_output(int descriptor) noexcept { return impl().doOpenOutput(descriptor); }
    HostIoResult try_write(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return impl().doTryWrite(bytes, size, written);
    }
    bool open_exclusive(const char* path) noexcept { return impl().doOpenExclusive(path); }
    bool open_existing(const char* path) noexcept { return impl().doOpenExisting(path); }
    HostIoResult read_at(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        return impl().doReadAt(bytes, size, offset, read);
    }
    HostIoResult write_at(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        return impl().doWriteAt(bytes, size, offset, written);
    }
    bool sync() noexcept { return impl().doSync(); }
    static bool rename_file(const char* from, const char* to) noexcept { return Impl::doRenameFile(from, to); }
    static uint64_t steady_ns() noexcept { return Impl::doSteadyNs(); }
    bool close() noexcept { return impl().doClose(); }
protected:
    ~HostIoBase() = default;
private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
}  // namespace dp::osal
