#pragma once
#if !defined(__linux__)
#error "当前Host I/O后端要求Linux；其他平台必须提供显式实现"
#endif

#include "interface_host_io.h"
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <limits>
#include <cstdio>
#include <unistd.h>

namespace dp::osal {
class OSALHostIo : public HostIoBase<OSALHostIo> {
    friend class HostIoBase<OSALHostIo>;
public:
    OSALHostIo() noexcept = default;
    ~OSALHostIo() { (void)doClose(); }
    OSALHostIo(const OSALHostIo&) = delete;
    OSALHostIo& operator=(const OSALHostIo&) = delete;
private:
    bool doOpenOutput(int descriptor) noexcept {
        if (descriptor_ >= 0 || descriptor < 0) return false;
        struct stat info{};
        if (::fstat(descriptor, &info) != 0) return false;
        if (S_ISREG(info.st_mode)) {
            // 常规文件共享原offset，避免独立OFD从零覆盖调用者已有日志。
            descriptor_ = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
            return descriptor_ >= 0;
        }
#if defined(__linux__)
        // dup共享open-file-description；重开proc句柄才不改变调用者的flags。
        char path[64] = "/proc/self/fd/";
        const auto result = std::to_chars(path + 14U, path + sizeof(path) - 1U, descriptor);
        if (result.ec != std::errc{}) return false;
        *result.ptr = '\0';
        descriptor_ = ::open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        return descriptor_ >= 0;
#else
        // 未适配独立非阻塞句柄的后端明确失败，不修改借用fd的共享flags。
        (void)descriptor;
        return false;
#endif
    }
    HostIoResult doTryWrite(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        if (written == nullptr || (bytes == nullptr && size != 0U)) return HostIoResult::kError;
        *written = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        if (size == 0U) return HostIoResult::kOk;
        // 仅保护调用线程，保留原有pending SIGPIPE，不更改进程signal策略。
        sigset_t blocked{}, previous{}, pending{};
        if (::sigemptyset(&blocked) != 0 || ::sigaddset(&blocked, SIGPIPE) != 0 ||
            ::pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) return HostIoResult::kError;
        if (::sigpending(&pending) != 0) {
            (void)::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
            return HostIoResult::kError;
        }
        const bool already_pending = ::sigismember(&pending, SIGPIPE) == 1;
        const ssize_t count = ::write(descriptor_, bytes, size);
        const int error = errno;
        if (count < 0 && error == EPIPE && !already_pending) {
            const timespec immediate{};
            (void)::sigtimedwait(&blocked, nullptr, &immediate);
        }
        if (::pthread_sigmask(SIG_SETMASK, &previous, nullptr) != 0) return HostIoResult::kError;
        if (count > 0) { *written = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (count == 0) return HostIoResult::kClosed;
        if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR) return HostIoResult::kWouldBlock;
        if (error == EPIPE || error == EBADF) return HostIoResult::kClosed;
        return HostIoResult::kError;
    }
    bool doOpenExclusive(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        return descriptor_ >= 0;
    }
    bool doOpenExisting(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (descriptor_ < 0) return false;
        struct stat info{};
        if (::fstat(descriptor_, &info) == 0 && S_ISREG(info.st_mode)) return true;
        (void)doClose(); return false;
    }
    HostIoResult doReadAt(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        if (read == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return HostIoResult::kError;
        *read = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        const ssize_t count = ::pread(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) { *read = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return HostIoResult::kWouldBlock;
        return errno == EBADF ? HostIoResult::kClosed : HostIoResult::kError;
    }
    HostIoResult doWriteAt(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        if (written == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return HostIoResult::kError;
        *written = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        const ssize_t count = ::pwrite(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) { *written = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return HostIoResult::kWouldBlock;
        return errno == EBADF ? HostIoResult::kClosed : HostIoResult::kError;
    }
    bool doSync() noexcept { return descriptor_ >= 0 && ::fsync(descriptor_) == 0; }
    static bool doRenameFile(const char* from, const char* to) noexcept {
        return from != nullptr && to != nullptr && ::rename(from, to) == 0;
    }
    static uint64_t doSteadyNs() noexcept {
        timespec value{};
        if (::clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0U;
        return static_cast<uint64_t>(value.tv_sec) * UINT64_C(1000000000) + static_cast<uint64_t>(value.tv_nsec);
    }
    bool doClose() noexcept {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        return descriptor < 0 || ::close(descriptor) == 0;
    }
    int descriptor_ = -1;
};
}  // namespace dp::osal
