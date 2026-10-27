/** @file dp_osal_file_system.h
 * @brief POSIX文件命名空间；rename不会自动同步目录。 */
#pragma once
#include "interface_file_system.h"
#include <limits.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
namespace dp::osal {
class FileSystem : public FileSystemBase<FileSystem> {
    friend class FileSystemBase<FileSystem>;

private:
    static bool doRenameFile(const char* from, const char* to) noexcept {
        return from != nullptr && to != nullptr && ::rename(from, to) == 0;
    }
    static size_t doMaxPathBytes() noexcept { return PATH_MAX; }
    static bool doCanonicalDirectory(const char* path, char* output, size_t capacity) noexcept {
        if (path == nullptr || output == nullptr || capacity < doMaxPathBytes()) return false;
        if (::realpath(path, output) == nullptr) return false;
        struct stat info {};
        return ::stat(output, &info) == 0 && S_ISDIR(info.st_mode);
    }
    static bool doReadRegularPrefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        if (used == nullptr) return false;
        *used = 0U;
        if (path == nullptr || output == nullptr || capacity == 0U ||
            capacity > static_cast<size_t>(std::numeric_limits<ssize_t>::max()))
            return false;
        // 非阻塞open确保FIFO不会在类型检查前挂住；磁盘文件读取仍可能等待I/O。
        /**
         * @brief 前缀读取的局部RAII守卫，错误早退时尽力关闭尚持有的描述符。
         */
        struct Descriptor {
            int value;  ///< 局部拥有的描述符；显式close后设-1避免析构重复关闭。
            /**
             * @brief 尚未显式关闭时尝试一次close，忽略失败，避免早退泄露正常句柄。
             */
            ~Descriptor() {
                if (value >= 0) (void)::close(value);
            }
        } descriptor{::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
        if (descriptor.value < 0) return false;
        struct stat info {};
        if (::fstat(descriptor.value, &info) != 0 || !S_ISREG(info.st_mode)) return false;
        const ssize_t count = ::read(descriptor.value, output, capacity);
        const int closed = ::close(descriptor.value);
        descriptor.value = -1;
        if (count < 0 || closed != 0) return false;
        *used = static_cast<size_t>(count);
        return true;
    }
};
}  // namespace dp::osal
