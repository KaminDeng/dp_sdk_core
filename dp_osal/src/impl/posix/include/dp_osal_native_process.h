/** @file dp_osal_native_process.h
 * @brief POSIX标准输入和进程信号端口；仅启动owner及明确故障路径使用。
 * 不在通用核心接口暴露poll事件、fd或信号编号。 */
#pragma once
#include "interface_io.h"
#include <cerrno>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
namespace dp::osal {
struct NativeInputInfo {
    int32_t events{};
    int32_t error{};
    bool read_attempted{};
};
using NativeSignalHandler = void (*)(int);
class NativeProcess {
public:
    static IoResult pollInput(uint8_t* byte, NativeInputInfo* info) noexcept {
        if (info == nullptr) return IoResult::kError;
        *info = {};
        if (byte == nullptr) {
            info->error = EINVAL;
            return IoResult::kError;
        }
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1U, 0);
        info->events = descriptor.revents;
        if (ready < 0) {
            info->error = errno;
            return errno == EINTR ? IoResult::kWouldBlock : IoResult::kError;
        }
        if ((descriptor.revents & POLLNVAL) != 0) {
            info->error = EBADF;
            return IoResult::kError;
        }
        if ((descriptor.revents & POLLERR) != 0) {
            info->error = EIO;
            return IoResult::kError;
        }
        if (ready == 0 || (descriptor.revents & (POLLIN | POLLHUP)) == 0) return IoResult::kWouldBlock;
        info->read_attempted = true;
        uint8_t value{};
        const ssize_t count = ::read(STDIN_FILENO, &value, 1U);
        if (count > 0) {
            *byte = value;
            return IoResult::kOk;
        }
        if (count == 0) return IoResult::kClosed;
        info->error = errno;
        return IoResult::kError;
    }
    static bool installTerminationHandler(NativeSignalHandler handler, bool include_interrupt = true) noexcept {
        if (handler == nullptr) return false;
        if (!include_interrupt) return ::signal(SIGTERM, handler) != SIG_ERR;
        const auto previous = ::signal(SIGINT, handler);
        if (previous == SIG_ERR) return false;
        if (::signal(SIGTERM, handler) != SIG_ERR) return true;
        (void)::signal(SIGINT, previous);
        return false;
    }
    static void signalSafeError(const uint8_t* bytes, size_t size) noexcept {
        if (bytes != nullptr && size != 0U) {
            const ssize_t ignored = ::write(STDERR_FILENO, bytes, size);
            (void)ignored;
        }
    }
    [[noreturn]] [[noreturn]] static void exitImmediately(int code) noexcept { ::_exit(code); }
};
}  // namespace dp::osal
