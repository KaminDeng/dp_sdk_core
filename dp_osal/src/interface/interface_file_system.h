/** @file interface_file_system.h
 * @brief 调用方缓冲中的路径/文件系统操作；仅准备或writer阶段调用，允许磁盘等待。 */
#pragma once
#include "interface_io.h"
namespace dp::osal {
template <typename Impl>
class FileSystemBase {
public:
    static bool rename(const char* from, const char* to) noexcept { return Impl::doRenameFile(from, to); }
    static size_t maxPathBytes() noexcept { return Impl::doMaxPathBytes(); }
    static bool canonicalDirectory(const char* path, char* output, size_t capacity) noexcept {
        return Impl::doCanonicalDirectory(path, output, capacity);
    }
    static bool readRegularPrefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        return Impl::doReadRegularPrefix(path, output, capacity, used);
    }

protected:
    ~FileSystemBase() = default;
};
}  // namespace dp::osal
