/** @file dp_osal_byte_output.h
 * @brief POSIX ByteOutput自持句柄；不可复制，关闭须先停止所有使用者。 */
#pragma once
#include "interface_byte_output.h"
#include "detail/dp_osal_descriptor_io.h"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
namespace dp::osal {
class NativeIo;
class ByteOutput : public ByteOutputBase<ByteOutput> {
    friend class ByteOutputBase<ByteOutput>;
    friend class NativeIo;

public:
    ByteOutput() noexcept = default;
    ~ByteOutput() { (void)doClose(); }
    ByteOutput(const ByteOutput&) = delete;
    ByteOutput& operator=(const ByteOutput&) = delete;

private:
    IoResult doTryWrite(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return detail::writeDescriptor(descriptor_, bytes, size, written);
    }
    bool doClose() noexcept {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        return descriptor < 0 || ::close(descriptor) == 0;
    }
    int descriptor_ = -1;
};
}  // namespace dp::osal
