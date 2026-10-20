/**
 * @file interface_host_io.h
 * @brief 主机字节流、文件、输入观测及时间/退出操作的 CRTP 边界。
 * @details 转发层不自行拥有文件描述符、不重试短写、不加锁；资源取得与释放由 Impl 管理。
 * noexcept 仅表示不通过 C++ 异常报告，不表示系统调用没有等待或磁盘延迟上界。
 * 实时消费端应处理 kWouldBlock/短写，把同步文件操作留在准备或 writer 阶段。
 */
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif


#include <cstddef>
#include <cstdint>

namespace dp::osal {
/**
 * @brief 本次主机 I/O 结果；kOk 也可能只有部分字节，必须同时检查实际数量。
 */
enum class HostIoResult : uint8_t {
    kOk,         ///< 本次操作返回成功，实际传输数量仍须单独检查。
    kWouldBlock, ///< 当前暂不可完成，是否重试及重试期限由调用者决定。
    kClosed,     ///< 所用句柄或流已关闭，当前操作不能继续使用它。
    kError       ///< 参数或后端错误，不能把缓冲区当作有效新结果。
};
/**
 * @brief 一次输入观测的诊断快照，不作为设备状态或运动授权。
 * @details events 为后端事件位，error 为后端错误码，read_attempted 标记是否真的尝试读字节。
 */
struct HostInputInfo {
    int32_t events{};       ///< 本次后端事件位，含义依后端，Linux 对应 poll revents。
    int32_t error{};        ///< 本次后端错误码；零值不能单独证明发生过成功读取。
    bool read_attempted{}; ///< 是否真正尝试读字节，与观测到就绪事件区分。
};
/**
 * @brief 终止信号回调；在信号上下文执行时只能调用相应上下文安全的操作。
 */
using HostSignalHandler = void (*)(int);

/**
 * @brief 主机 I/O 的编译期接口，具体对象负责句柄与后端操作。
 * @tparam Impl 实现所列 do* 操作并允许本基类访问的后端。
 */
template <typename Impl>
class HostIoBase {
public:
    /** @brief 取得独立输出句柄；失败不改变借用descriptor的flags。 */
    /**
     * @param descriptor 借用的现有输出描述符；借用者仍保留原描述符所有权。
     * @return 成功取得对象自持输出句柄时为 true。
     * @note 当前 Linux 实现对普通文件 dup，保留共享文件偏移；非普通文件经 proc 重开非阻塞句柄。
     */
    bool open_output(int descriptor) noexcept { return impl().doOpenOutput(descriptor); }
    /** @brief 尝试写入字节；written为实际数量，调用者处理短写与背压。 */
    /**
     * @param[in] bytes 调用期间有效的待写缓冲区。
     * @param size 请求字节数。
     * @param[out] written 本次实际写出数量，调用者负责剩余数据和状态处理。
     * @return 后端写入结果；一次 kOk 不代表整个缓冲已写完或已持久化。
     */
    HostIoResult try_write(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return impl().doTryWrite(bytes, size, written);
    }
    /** @brief 独占创建绝对路径的新文件；对象持有并在close/析构释放句柄。 */
    /**
     * @param[in] path 调用期间有效、以 NUL 结尾的绝对路径。
     * @return 新建并取得自持句柄时为 true，路径已存在等情况失败。
     */
    bool open_exclusive(const char* path) noexcept { return impl().doOpenExclusive(path); }
    /** @brief 打开已有普通文件；失败不授予可用句柄。 */
    /**
     * @param[in] path 已有普通文件的绝对路径。
     * @return 打开并通过后端类型检查时为 true；当前 Linux 实现要求可读写。
     */
    bool open_existing(const char* path) noexcept { return impl().doOpenExisting(path); }
    /** @brief 从固定偏移读取，read返回实际字节数；不改变共享文件偏移。 */
    /**
     * @param[out] bytes 接收数据的缓冲区。
     * @param size 最大读取字节数。
     * @param offset 从文件起点计算的字节偏移。
     * @param[out] read 实际读取字节数；kOk 且为零可表示文件末尾。
     * @return 本次读取结果，允许短读，调用者按需继续。
     */
    HostIoResult read_at(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        return impl().doReadAt(bytes, size, offset, read);
    }
    /** @brief 向固定偏移写入，written返回实际字节数；不改变共享文件偏移。 */
    /**
     * @param[in] bytes 待写缓冲区。
     * @param size 请求写入字节数。
     * @param offset 从文件起点计算的字节偏移。
     * @param[out] written 实际写入字节数。
     * @return 本次写入结果，允许短写，完成写入与持久化需分别处理。
     */
    HostIoResult write_at(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        return impl().doWriteAt(bytes, size, offset, written);
    }
    /** @brief 同步文件持久化；仅writer/准备阶段使用，可能等待磁盘I/O。 */
    /**
     * @return 后端同步成功时为 true；不会创建业务提交标志或证明目录项已同步。
     */
    bool sync() noexcept { return impl().doSync(); }
    /** @brief 重命名文件，失败返回false；路径借用仅在调用期间有效。 */
    /**
     * @param[in] from 原路径，调用期间有效。
     * @param[in] to 目标路径，调用期间有效。
     * @return 后端重命名成功时为 true；不会自动同步目标目录。
     */
    static bool rename_file(const char* from, const char* to) noexcept { return Impl::doRenameFile(from, to); }
    /** @brief 路径缓冲所需字节数，包含终止符；由具体Host后端定义。 */
    /**
     * @return 后端要求的路径缓冲字节数，包含结尾 NUL 空间。
     */
    static size_t max_path_bytes() noexcept { return Impl::doMaxPathBytes(); }
    /** @brief 在调用方存储中规范化已有目录；失败返回false，不取得持久资源。
     * @param capacity 须不小于max_path_bytes()；仅启动/准备阶段调用，可能访问文件系统。 */
    /**
     * @param[in] path 要解析的目录路径。
     * @param[out] output 容量为 capacity 的调用者缓冲区。
     * @return 成功解析且确认结果为已有目录时为 true；失败时输出不可作为有效路径使用。
     */
    static bool canonical_directory(const char* path, char* output, size_t capacity) noexcept {
        return Impl::doCanonicalDirectory(path, output, capacity);
    }
    /** @brief 同步读取普通文件的有界前缀，最多capacity字节；不接受目录/FIFO。
     * @param used 成功返回实际字节数，失败置零；缓冲由调用者持有。
     * @return 文件打开、读取、关闭均成功时为true。仅准备/等待阶段调用，不承诺磁盘延迟上界。 */
    /**
     * @param[in] path 要打开并检查的普通文件路径。
     * @param[out] output capacity 字节的调用者缓冲区。
     * @param capacity 最大读取字节数，当前 Linux 后端要求非零。
     * @note 前缀读取成功不表示读到了完整文件；超过 capacity 的内容不在本次返回中。
     */
    static bool read_regular_prefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        return Impl::doReadRegularPrefix(path, output, capacity, used);
    }
    /** @brief 读取单调纳秒；时钟失败返回零，调用方须拒绝无效时间。 */
    /**
     * @return 单调纳秒计数，零值按本接口约定视为时钟不可用。
     */
    static uint64_t steady_ns() noexcept { return Impl::doSteadyNs(); }
    /** @brief 零等待观测单一stdin消费者输入；info保留本次后端事件。 */
    /**
     * @param[out] byte 成功读取时接收一个字节。
     * @param[out] info 本次事件、错误及是否尝试读取的诊断信息。
     * @return 有字节、暂不可读、关闭或错误；当前后端要求 stdin 只有一个消费者，避免就绪后读竞态。
     */
    static HostIoResult poll_input(uint8_t* byte, HostInputInfo* info) noexcept {
        return Impl::doPollInput(byte, info);
    }
    /** @brief 启动owner安装终止处理器；失败回滚此前安装的SIGINT。 */
    /**
     * @param handler 有效的终止信号回调。
     * @param include_interrupt 为 true 时同时安装 SIGINT，否则仅安装 SIGTERM。
     * @return 安装成功时为 true；修改进程级信号策略，应由启动 owner 统一调用。
     */
    static bool install_termination_handler(HostSignalHandler handler, bool include_interrupt = true) noexcept {
        return Impl::doInstallTerminationHandler(handler, include_interrupt);
    }
    /** @brief 写入墙钟微秒；失败返回false，不可用于控制单调期限。 */
    /**
     * @param[out] value 成功时接收墙钟微秒值，失败时不能使用输出。
     * @return 后端读取及范围检查成功时为 true；墙钟可跳变。
     */
    static bool wall_time_us(uint64_t* value) noexcept { return Impl::doWallTimeUs(value); }
    /** @brief 写入当前线程CPU微秒；失败返回false。 */
    /**
     * @param[out] value 成功时接收当前线程累计 CPU 时间，单位微秒。
     * @return 后端支持且读取成功时为 true；不包含线程阻塞等待的墙钟时长。
     */
    static bool thread_cpu_time_us(uint64_t* value) noexcept { return Impl::doThreadCpuTimeUs(value); }
    /** @brief 信号处理器紧急字节输出；仅故障路径，可能受设备背压影响。 */
    /**
     * @param[in] bytes 待输出字节，借用到调用结束。
     * @param size 请求输出字节数。
     * @note 当前后端只尝试一次 stderr 写入且忽略短写/错误，不保证日志完整或实时返回。
     */
    static void signal_safe_error(const uint8_t* bytes, size_t size) noexcept { Impl::doSignalSafeError(bytes, size); }
    /** @brief 立即结束进程，不执行C++析构；只用于已决定退出的故障路径。 */
    /**
     * @param code 传给底层立即退出操作的退出状态。
     * @note 不等待普通退出清理，不保证尚未同步的证据持久化。
     */
    [[noreturn]] static void exit_immediately(int code) noexcept { Impl::doExitImmediately(code); }
    /** @brief 释放自持句柄；可重复调用，返回关闭结果。 */
    /**
     * @return 当前对象自持句柄释放成功或已关闭时为 true；借用原描述符不归本对象关闭。
     */
    bool close() noexcept { return impl().doClose(); }
protected:
    /**
     * @brief 受保护的非虚析构，具体后端负责真正的句柄清理。
     */
    ~HostIoBase() = default;
private:
    /**
     * @brief 静态回转到具体后端。
     * @return 当前 Impl 对象引用，不执行运行时类型检查。
     */
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
}  // namespace dp::osal
