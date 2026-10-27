/** @file interface_file.h
 * @brief 自持普通文件；显式取得/释放，偏移读写不改变当前位置，sync不保证目录持久化。 */
#pragma once
#include "interface_io.h"
namespace dp::osal {
template <typename Impl>
class FileBase {
public:
    /** @brief 独占创建绝对新路径；拒绝覆盖已有对象，失败不取得句柄。 */
    bool openExclusive(const char* path) noexcept { return impl().doOpenExclusive(path); }
    /** @brief 以读写方式打开已有绝对路径并检查普通文件；拒绝最终路径符号链接。 */
    bool openExisting(const char* path) noexcept { return impl().doOpenExisting(path); }
    /** @brief 当前位置单次写，实际数量通过written返回；短写/背压由调用方处理。 */
    IoResult tryWrite(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return impl().doTryWrite(bytes, size, written);
    }
    /** @brief 单次固定偏移读，不改变当前位置；kOk且read=0表示EOF。 */
    IoResult readAt(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        return impl().doReadAt(bytes, size, offset, read);
    }
    /** @brief 单次固定偏移写，不改变当前位置；不重试短写，不隐式sync。 */
    IoResult writeAt(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        return impl().doWriteAt(bytes, size, offset, written);
    }
    /** @brief 同步本文件数据；可能等待磁盘，不同步父目录。 */
    bool sync() noexcept { return impl().doSync(); }
    /** @brief 无并发使用者时释放句柄；即使失败也撤销本地所有权，不能重试旧句柄。 */
    bool close() noexcept { return impl().doClose(); }

protected:
    ~FileBase() = default;

private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
}  // namespace dp::osal
