#pragma once

#include <cstddef>
#include <cstdint>

namespace dp::osal {
enum class HostIoResult : uint8_t { kOk, kWouldBlock, kClosed, kError };
// 捕获本次输入观测；仅诊断保留后端事件/错误码，不作为控制授权。
struct HostInputInfo { int32_t events{}; int32_t error{}; bool read_attempted{}; };
using HostSignalHandler = void (*)(int);

// 仅Host有界字节输出；构造不取得资源，关闭与错误均可观察。
template <typename Impl>
class HostIoBase {
public:
    /** @brief 取得独立输出句柄；失败不改变借用descriptor的flags。 */
    bool open_output(int descriptor) noexcept { return impl().doOpenOutput(descriptor); }
    /** @brief 尝试写入字节；written为实际数量，调用者处理短写与背压。 */
    HostIoResult try_write(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return impl().doTryWrite(bytes, size, written);
    }
    /** @brief 独占创建绝对路径的新文件；对象持有并在close/析构释放句柄。 */
    bool open_exclusive(const char* path) noexcept { return impl().doOpenExclusive(path); }
    /** @brief 打开已有普通文件；失败不授予可用句柄。 */
    bool open_existing(const char* path) noexcept { return impl().doOpenExisting(path); }
    /** @brief 从固定偏移读取，read返回实际字节数；不改变共享文件偏移。 */
    HostIoResult read_at(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        return impl().doReadAt(bytes, size, offset, read);
    }
    /** @brief 向固定偏移写入，written返回实际字节数；不改变共享文件偏移。 */
    HostIoResult write_at(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        return impl().doWriteAt(bytes, size, offset, written);
    }
    /** @brief 同步文件持久化；仅writer/准备阶段使用，可能等待磁盘I/O。 */
    bool sync() noexcept { return impl().doSync(); }
    /** @brief 重命名文件，失败返回false；路径借用仅在调用期间有效。 */
    static bool rename_file(const char* from, const char* to) noexcept { return Impl::doRenameFile(from, to); }
    /** @brief 路径缓冲所需字节数，包含终止符；由具体Host后端定义。 */
    static size_t max_path_bytes() noexcept { return Impl::doMaxPathBytes(); }
    /** @brief 在调用方存储中规范化已有目录；失败返回false，不取得持久资源。
     * @param capacity 须不小于max_path_bytes()；仅启动/准备阶段调用，可能访问文件系统。 */
    static bool canonical_directory(const char* path, char* output, size_t capacity) noexcept {
        return Impl::doCanonicalDirectory(path, output, capacity);
    }
    /** @brief 同步读取普通文件的有界前缀，最多capacity字节；不接受目录/FIFO。
     * @param used 成功返回实际字节数，失败置零；缓冲由调用者持有。
     * @return 文件打开、读取、关闭均成功时为true。仅准备/等待阶段调用，不承诺磁盘延迟上界。 */
    static bool read_regular_prefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        return Impl::doReadRegularPrefix(path, output, capacity, used);
    }
    /** @brief 读取单调纳秒；时钟失败返回零，调用方须拒绝无效时间。 */
    static uint64_t steady_ns() noexcept { return Impl::doSteadyNs(); }
    /** @brief 零等待观测单一stdin消费者输入；info保留本次后端事件。 */
    static HostIoResult poll_input(uint8_t* byte, HostInputInfo* info) noexcept {
        return Impl::doPollInput(byte, info);
    }
    /** @brief 启动owner安装终止处理器；失败回滚此前安装的SIGINT。 */
    static bool install_termination_handler(HostSignalHandler handler, bool include_interrupt = true) noexcept {
        return Impl::doInstallTerminationHandler(handler, include_interrupt);
    }
    /** @brief 写入墙钟微秒；失败返回false，不可用于控制单调期限。 */
    static bool wall_time_us(uint64_t* value) noexcept { return Impl::doWallTimeUs(value); }
    /** @brief 写入当前线程CPU微秒；失败返回false。 */
    static bool thread_cpu_time_us(uint64_t* value) noexcept { return Impl::doThreadCpuTimeUs(value); }
    /** @brief 信号处理器紧急字节输出；仅故障路径，可能受设备背压影响。 */
    static void signal_safe_error(const uint8_t* bytes, size_t size) noexcept { Impl::doSignalSafeError(bytes, size); }
    /** @brief 立即结束进程，不执行C++析构；只用于已决定退出的故障路径。 */
    [[noreturn]] static void exit_immediately(int code) noexcept { Impl::doExitImmediately(code); }
    /** @brief 释放自持句柄；可重复调用，返回关闭结果。 */
    bool close() noexcept { return impl().doClose(); }
protected:
    ~HostIoBase() = default;
private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
}  // namespace dp::osal
