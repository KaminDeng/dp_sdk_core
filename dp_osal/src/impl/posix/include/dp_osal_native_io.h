/** @file dp_osal_native_io.h
 * @brief Linux原生描述符与正式字节流的绑定端口；不改变借用fd的flags。 */
#pragma once
#if !defined(__linux__)
#error "独立非阻塞描述符绑定尚未适配此平台"
#endif
#include "dp_osal_byte_output.h"
#include <charconv>
namespace dp::osal {
class NativeIo {
public:
    static bool openOutput(ByteOutput& output, int descriptor) noexcept {
        if (output.descriptor_ >= 0 || descriptor < 0) return false;
        struct stat info {};
        if (::fstat(descriptor, &info) != 0) return false;
        if (S_ISREG(info.st_mode)) {
            // 常规文件共享原offset，避免独立OFD从零覆盖调用者已有日志。
            output.descriptor_ = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
            return output.descriptor_ >= 0;
        }
        // dup共享open-file-description；重开proc句柄才不改变调用者的flags。
        char path[64] = "/proc/self/fd/";
        const auto result = std::to_chars(path + 14U, path + sizeof(path) - 1U, descriptor);
        if (result.ec != std::errc{}) return false;
        *result.ptr = '\0';
        output.descriptor_ = ::open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        return output.descriptor_ >= 0;
    }
};
}  // namespace dp::osal
