#pragma once
#if !defined(__linux__)
#error "当前Host I/O后端要求Linux；其他平台必须提供显式实现"
#endif

#include "interface_host_io.h"
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <limits.h>
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
    static size_t doMaxPathBytes() noexcept { return PATH_MAX; }
    static bool doCanonicalDirectory(const char* path, char* output, size_t capacity) noexcept {
        if (path == nullptr || output == nullptr || capacity < doMaxPathBytes()) return false;
        if (::realpath(path, output) == nullptr) return false;
        struct stat info{};
        return ::stat(output, &info) == 0 && S_ISDIR(info.st_mode);
    }
    static bool doReadRegularPrefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        if (used == nullptr) return false;
        *used = 0U;
        if (path == nullptr || output == nullptr || capacity == 0U ||
            capacity > static_cast<size_t>(std::numeric_limits<ssize_t>::max())) return false;
        // 非阻塞open确保FIFO不会在类型检查前挂住；磁盘文件读取仍可能等待I/O。
        struct Descriptor {
            int value;
            ~Descriptor() { if (value >= 0) (void)::close(value); }
        } descriptor{::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
        if (descriptor.value < 0) return false;
        struct stat info{};
        if (::fstat(descriptor.value, &info) != 0 || !S_ISREG(info.st_mode)) return false;
        const ssize_t count = ::read(descriptor.value, output, capacity);
        const int closed = ::close(descriptor.value);
        descriptor.value = -1;
        if (count < 0 || closed != 0) return false;
        *used = static_cast<size_t>(count);
        return true;
    }
    static uint64_t doSteadyNs() noexcept {
        timespec value{};
        if (::clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0U;
        return static_cast<uint64_t>(value.tv_sec) * UINT64_C(1000000000) + static_cast<uint64_t>(value.tv_nsec);
    }
    // 单一stdin消费者；先零等待poll，只有POLLIN/HUP才读一个字节，不改变fd flags。
    static HostIoResult doPollInput(uint8_t* byte, HostInputInfo* info) noexcept {
        if (info == nullptr) return HostIoResult::kError;
        *info = {};
        if (byte == nullptr) { info->error = EINVAL; return HostIoResult::kError; }
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1U, 0);
        info->events = descriptor.revents;
        if (ready < 0) {
            info->error = errno;
            return errno == EINTR ? HostIoResult::kWouldBlock : HostIoResult::kError;
        }
        if (ready == 0 || (descriptor.revents & (POLLIN | POLLHUP)) == 0)
            return HostIoResult::kWouldBlock;
        info->read_attempted = true;
        uint8_t value{};
        const ssize_t count = ::read(STDIN_FILENO, &value, 1U);
        if (count > 0) { *byte = value; return HostIoResult::kOk; }
        if (count == 0) return HostIoResult::kClosed;
        info->error = errno;
        return HostIoResult::kError;
    }
    // 与原进程级signal注册一致；仅启动owner安装，失败回滚已安装的SIGINT。
    static bool doInstallTerminationHandler(HostSignalHandler handler, bool include_interrupt) noexcept {
        if (handler == nullptr) return false;
        if (!include_interrupt) return ::signal(SIGTERM, handler) != SIG_ERR;
        const auto previous = ::signal(SIGINT, handler);
        if (previous == SIG_ERR) return false;
        if (::signal(SIGTERM, handler) != SIG_ERR) return true;
        (void)::signal(SIGINT, previous);
        return false;
    }
    static bool clock_us(clockid_t clock, uint64_t* output) noexcept {
        if (output == nullptr) return false;
        timespec value{};
        if (::clock_gettime(clock, &value) != 0 || value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L)
            return false;
        const auto seconds = static_cast<uint64_t>(value.tv_sec);
        if (seconds > UINT64_MAX / UINT64_C(1000000)) return false;
        const uint64_t whole = seconds * UINT64_C(1000000);
        const uint64_t fraction = static_cast<uint64_t>(value.tv_nsec) / 1000U;
        if (whole > UINT64_MAX - fraction) return false;
        *output = whole + fraction;
        return true;
    }
    static bool doWallTimeUs(uint64_t* value) noexcept { return clock_us(CLOCK_REALTIME, value); }
    static bool doThreadCpuTimeUs(uint64_t* value) noexcept {
#if defined(CLOCK_THREAD_CPUTIME_ID)
        return clock_us(CLOCK_THREAD_CPUTIME_ID, value);
#else
        (void)value; return false;
#endif
    }
    static void doSignalSafeError(const uint8_t* bytes, size_t size) noexcept {
        if (bytes != nullptr && size != 0U) {
            const ssize_t ignored = ::write(STDERR_FILENO, bytes, size);
            (void)ignored;
        }
    }
    [[noreturn]] static void doExitImmediately(int code) noexcept { ::_exit(code); }
    bool doClose() noexcept {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        return descriptor < 0 || ::close(descriptor) == 0;
    }
    int descriptor_ = -1;
};
}  // namespace dp::osal
