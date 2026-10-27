/** @file dp_osal_file.h
 * @brief POSIX File自持句柄；不可复制，关闭须先停止所有使用者。 */
#pragma once
#include "interface_file.h"
#include "detail/dp_osal_descriptor_io.h"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
namespace dp::osal {
class File : public FileBase<File> {
    friend class FileBase<File>;

public:
    File() noexcept = default;
    ~File() { (void)doClose(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;

private:
    bool doOpenExclusive(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        return descriptor_ >= 0;
    }
    bool doOpenExisting(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (descriptor_ < 0) return false;
        struct stat info {};
        if (::fstat(descriptor_, &info) == 0 && S_ISREG(info.st_mode)) return true;
        (void)doClose();
        return false;
    }
    IoResult doReadAt(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        if (read == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
            return IoResult::kError;
        *read = 0U;
        if (descriptor_ < 0) return IoResult::kClosed;
        const ssize_t count = ::pread(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) {
            *read = static_cast<size_t>(count);
            return IoResult::kOk;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return IoResult::kWouldBlock;
        return errno == EBADF ? IoResult::kClosed : IoResult::kError;
    }
    IoResult doWriteAt(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        if (written == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
            return IoResult::kError;
        *written = 0U;
        if (descriptor_ < 0) return IoResult::kClosed;
        const ssize_t count = ::pwrite(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) {
            *written = static_cast<size_t>(count);
            return IoResult::kOk;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return IoResult::kWouldBlock;
        return errno == EBADF ? IoResult::kClosed : IoResult::kError;
    }
    bool doSync() noexcept { return descriptor_ >= 0 && ::fsync(descriptor_) == 0; }
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
