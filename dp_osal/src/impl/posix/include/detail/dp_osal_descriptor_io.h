/** @file dp_osal_descriptor_io.h
 * @brief POSIX单次写与自持描述符释放；不重试短写或中断。 */
#pragma once
#if !defined(__linux__)
#error "受检SIGPIPE写端口尚未适配此POSIX平台"
#endif
#include "interface_io.h"
#include <cerrno>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
namespace dp::osal::detail {
inline IoResult writeDescriptor(int descriptor_, const uint8_t* bytes, size_t size, size_t* written) noexcept {
    if (written == nullptr || (bytes == nullptr && size != 0U)) return IoResult::kError;
    *written = 0U;
    if (descriptor_ < 0) return IoResult::kClosed;
    if (size == 0U) return IoResult::kOk;
    // 仅保护调用线程，保留原有pending SIGPIPE，不更改进程signal策略。
    sigset_t blocked{}, previous{}, pending{};
    if (::sigemptyset(&blocked) != 0 || ::sigaddset(&blocked, SIGPIPE) != 0 ||
        ::pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0)
        return IoResult::kError;
    if (::sigpending(&pending) != 0) {
        (void)::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        return IoResult::kError;
    }
    const bool already_pending = ::sigismember(&pending, SIGPIPE) == 1;
    const ssize_t count = ::write(descriptor_, bytes, size);
    const int error = errno;
    if (count < 0 && error == EPIPE && !already_pending) {
        const timespec immediate{};
        (void)::sigtimedwait(&blocked, nullptr, &immediate);
    }
    if (::pthread_sigmask(SIG_SETMASK, &previous, nullptr) != 0) return IoResult::kError;
    if (count > 0) {
        *written = static_cast<size_t>(count);
        return IoResult::kOk;
    }
    if (count == 0) return IoResult::kClosed;
    if (error == EAGAIN || error == EINTR
#if EWOULDBLOCK != EAGAIN
        || error == EWOULDBLOCK
#endif
    )
        return IoResult::kWouldBlock;
    if (error == EPIPE || error == EBADF) return IoResult::kClosed;
    return IoResult::kError;
}
}  // namespace dp::osal::detail
