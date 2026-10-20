/**
 * @file dp_osal_host_io.h
 * @brief 当前Linux主机字节、文件及输入/时钟/进程操作的CRTP后端。
 * @details 文件位于POSIX目录，但显式要求Linux，依赖proc描述符重开等能力。
 * 每个对象独占管理一个自持descriptor；借用的原描述符由原owner管理。
 * 返回状态与字节数需一起检查，普通文件I/O不承诺实时延迟；本文件不自动重试短传输。
 * 静态信号注册和立即退出是进程级行为，仅由合适的owner/故障路径使用。
 */
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif

#if !defined(__linux__)
#error "当前Host I/O后端要求Linux；其他平台必须提供显式实现"
#endif

#include "interface_host_io.h"
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>

namespace dp::osal {
/**
 * @brief 以descriptor_记录自持句柄的Linux I/O对象，不可复制，析构尽力close。
 * @note 本对象不加互斥锁，open/close与读写生命周期由调用者协调。
 */
class OSALHostIo : public HostIoBase<OSALHostIo> {
    friend class HostIoBase<OSALHostIo>;
public:
    /**
     * @brief 构造关闭态对象，不执行open或修改借用描述符。
     */
    OSALHostIo() noexcept = default;
    /**
     * @brief 尽力关闭自持句柄，析构不报告关闭失败。
     */
    ~OSALHostIo() { (void)doClose(); }
    /**
     * @brief 禁止复制描述符所有权，避免双重关闭。
     */
    OSALHostIo(const OSALHostIo&) = delete;
    /**
     * @brief 禁止复制赋值覆盖当前句柄。
     */
    OSALHostIo& operator=(const OSALHostIo&) = delete;
private:
    /**
     * @brief 从现有输出描述符取得自持句柄，不改变借用者的文件状态flags。
     * @param descriptor 由外部owner持有的非负描述符，本对象当前必须关闭。
     * @return 检查类型并取得自持句柄为true，否则false。
     * @details 普通文件用F_DUPFD_CLOEXEC，与原描述符共享open-file-description及offset；
     * 其它类型经/proc/self/fd重开O_NONBLOCK独立句柄，避免dup后设置flags影响借用者。
     * @note 普通文件路径不提供磁盘时延上界，重开是否允许也依赖实际描述符及系统环境。
     */
    bool doOpenOutput(int descriptor) noexcept {
        if (descriptor_ >= 0 || descriptor < 0) return false;
        struct stat info{};
        if (::fstat(descriptor, &info) != 0) return false;
        if (S_ISREG(info.st_mode)) {
            // 常规文件共享原offset，避免独立OFD从零覆盖调用者已有日志。
            descriptor_ = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
            return descriptor_ >= 0;
        }
#if defined(__linux__)
        // dup共享open-file-description；重开proc句柄才不改变调用者的flags。
        char path[64] = "/proc/self/fd/";
        const auto result = std::to_chars(path + 14U, path + sizeof(path) - 1U, descriptor);
        if (result.ec != std::errc{}) return false;
        *result.ptr = '\0';
        descriptor_ = ::open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        return descriptor_ >= 0;
#else
        // 未适配独立非阻塞句柄的后端明确失败，不修改借用fd的共享flags。
        (void)descriptor;
        return false;
#endif
    }
    /**
     * @brief 尝试一次write，临时在线程内屏蔽SIGPIPE并恢复原信号mask。
     * @param[in] bytes 待写缓冲，size非零时不能为空。
     * @param size 请求字节数，调用者负责合理预算。
     * @param[out] written 参数有效时先置零，成功正写入才报告实际数量。
     * @return 正写入为kOk，零写入映射kClosed；EAGAIN/EWOULDBLOCK/EINTR为kWouldBlock，
     * EPIPE/EBADF为kClosed，其余错误为kError。
     * @details 保留已有pending SIGPIPE；仅在本次EPIPE且原本无pending时尝试消费本次信号。
     * @note 参数校验失败可能不改written；恢复signal mask失败可在write已发生后返回kError且数量仍零。
     * 因此失败不是“肯定未写任何字节”的证明，调用者不能据此无条件重发非幂等记录。
     */
    HostIoResult doTryWrite(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        if (written == nullptr || (bytes == nullptr && size != 0U)) return HostIoResult::kError;
        *written = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        if (size == 0U) return HostIoResult::kOk;
        // 仅保护调用线程，保留原有pending SIGPIPE，不更改进程signal策略。
        sigset_t blocked{}, previous{}, pending{};
        if (::sigemptyset(&blocked) != 0 || ::sigaddset(&blocked, SIGPIPE) != 0 ||
            ::pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) return HostIoResult::kError;
        if (::sigpending(&pending) != 0) {
            (void)::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
            return HostIoResult::kError;
        }
        const bool already_pending = ::sigismember(&pending, SIGPIPE) == 1;
        const ssize_t count = ::write(descriptor_, bytes, size);
        const int error = errno;
        if (count < 0 && error == EPIPE && !already_pending) {
            const timespec immediate{};
            (void)::sigtimedwait(&blocked, nullptr, &immediate);
        }
        if (::pthread_sigmask(SIG_SETMASK, &previous, nullptr) != 0) return HostIoResult::kError;
        if (count > 0) { *written = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (count == 0) return HostIoResult::kClosed;
        if (error == EAGAIN || error == EINTR
#if EWOULDBLOCK != EAGAIN
            || error == EWOULDBLOCK
#endif
        )
            return HostIoResult::kWouldBlock;
        if (error == EPIPE || error == EBADF) return HostIoResult::kClosed;
        return HostIoResult::kError;
    }
    /**
     * @brief 以0600权限请求独占创建绝对新路径，拒绝最终路径符号链接。
     * @param[in] path 非空且以/开头的NUL结尾路径。
     * @return 当前关闭且O_CREAT/O_EXCL/O_RDWR打开成功时为true。
     * @note 实际权限还受umask影响；成功不代表内容或目录项已持久化。
     */
    bool doOpenExclusive(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        return descriptor_ >= 0;
    }
    /**
     * @brief 以可读写方式打开已有绝对路径，fstat确认普通文件后保留句柄。
     * @param[in] path 非空绝对路径，最终路径不跟随符号链接。
     * @return 打开且类型检查成功时为true；类型/查询失败时关闭刚取得句柄。
     * @note 类型检查发生在open后，不承诺打开任意输入路径的延迟上界，只用于合适准备阶段。
     */
    bool doOpenExisting(const char* path) noexcept {
        if (descriptor_ >= 0 || path == nullptr || path[0] != '/') return false;
        descriptor_ = ::open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
        if (descriptor_ < 0) return false;
        struct stat info{};
        if (::fstat(descriptor_, &info) == 0 && S_ISREG(info.st_mode)) return true;
        (void)doClose(); return false;
    }
    /**
     * @brief 一次pread，从指定字节偏移读取且不移动共享offset。
     * @param[out] bytes 接收缓冲，size非零时不能为空。
     * @param size 最大读取字节数。
     * @param offset 须可表示为off_t的非负文件偏移。
     * @param[out] read 参数有效时先清零，kOk时写实际数量。
     * @return 非负返回值为kOk（零可为EOF）；中断/暂不可读为kWouldBlock，EBADF为kClosed，其余kError。
     * @note 允许短读，不循环；参数失败时输出数量可能保持旧值。
     */
    HostIoResult doReadAt(uint8_t* bytes, size_t size, uint64_t offset, size_t* read) noexcept {
        if (read == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return HostIoResult::kError;
        *read = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        const ssize_t count = ::pread(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) { *read = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return HostIoResult::kWouldBlock;
        return errno == EBADF ? HostIoResult::kClosed : HostIoResult::kError;
    }
    /**
     * @brief 一次pwrite，从指定偏移写入且不改变共享offset。
     * @param[in] bytes 待写缓冲，size非零时不能为空。
     * @param size 请求字节数。
     * @param offset 须可表示为off_t的字节偏移。
     * @param[out] written 参数有效时先清零，非负写入结果写出实际数量。
     * @return kOk允许零或短写；中断/暂不可写为kWouldBlock，EBADF为kClosed，其余kError。
     * @note 不重试、不fsync；参数失败不保证清空数量输出。
     */
    HostIoResult doWriteAt(const uint8_t* bytes, size_t size, uint64_t offset, size_t* written) noexcept {
        if (written == nullptr || (bytes == nullptr && size != 0U) ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) return HostIoResult::kError;
        *written = 0U;
        if (descriptor_ < 0) return HostIoResult::kClosed;
        const ssize_t count = ::pwrite(descriptor_, bytes, size, static_cast<off_t>(offset));
        if (count >= 0) { *written = static_cast<size_t>(count); return HostIoResult::kOk; }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return HostIoResult::kWouldBlock;
        return errno == EBADF ? HostIoResult::kClosed : HostIoResult::kError;
    }
    /**
     * @brief 对当前自持句柄执行fsync。
     * @return 句柄有效且fsync成功时为true；不替调用者同步其它文件或父目录。
     */
    bool doSync() noexcept { return descriptor_ >= 0 && ::fsync(descriptor_) == 0; }
    /**
     * @brief 检查路径非空后调用一次rename。
     * @param[in] from 原路径。
     * @param[in] to 目标路径。
     * @return 系统调用成功为true，不额外做所有权验证或目录fsync。
     */
    static bool doRenameFile(const char* from, const char* to) noexcept {
        return from != nullptr && to != nullptr && ::rename(from, to) == 0;
    }
    /**
     * @brief 给出此后端要求的路径缓冲容量。
     * @return PATH_MAX字节，供canonical_directory容量检查使用。
     */
    static size_t doMaxPathBytes() noexcept { return PATH_MAX; }
    /**
     * @brief realpath规范化后用stat确认已有目录，不取得持续目录句柄。
     * @param[in] path 要解析的路径，可相对或绝对，可经符号链接。
     * @param[out] output 调用者路径缓冲。
     * @param capacity 至少为PATH_MAX，否则拒绝。
     * @return 参数、realpath及目录类型检查均通过为true；失败不能使用输出当作有效绑定。
     * @note 规范化不是目录身份认证，也不锁定后续路径替换，准备阶段使用。
     */
    static bool doCanonicalDirectory(const char* path, char* output, size_t capacity) noexcept {
        if (path == nullptr || output == nullptr || capacity < doMaxPathBytes()) return false;
        if (::realpath(path, output) == nullptr) return false;
        struct stat info{};
        return ::stat(output, &info) == 0 && S_ISDIR(info.st_mode);
    }
    /**
     * @brief 非阻塞open后确认普通文件，尝试一次有界read并显式检查close结果。
     * @param[in] path 文件路径，不要求绝对；此入口没有O_NOFOLLOW。
     * @param[out] output capacity字节的调用者缓冲。
     * @param capacity 须非零且不超过ssize_t最大值。
     * @param[out] used 有效指针先清零，读及关闭均成功后写实际数量。
     * @return 全部成功为true，零字节/短前缀仍可成功，不代表已读完整文件。
     * @note FIFO在类型检查前不会因等writer而挂起，普通磁盘I/O仍可等待；无EINTR重试循环。
     */
    static bool doReadRegularPrefix(const char* path, uint8_t* output, size_t capacity, size_t* used) noexcept {
        if (used == nullptr) return false;
        *used = 0U;
        if (path == nullptr || output == nullptr || capacity == 0U ||
            capacity > static_cast<size_t>(std::numeric_limits<ssize_t>::max())) return false;
        // 非阻塞open确保FIFO不会在类型检查前挂住；磁盘文件读取仍可能等待I/O。
        /**
         * @brief 前缀读取的局部RAII守卫，错误早退时尽力关闭尚持有的描述符。
         */
        struct Descriptor {
            int value; ///< 局部拥有的描述符；显式close后设-1避免析构重复关闭。
            /**
             * @brief 尚未显式关闭时尝试一次close，忽略失败，避免早退泄露正常句柄。
             */
            ~Descriptor() { if (value >= 0) (void)::close(value); }
        } descriptor{::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
        if (descriptor.value < 0) return false;
        struct stat info{};
        if (::fstat(descriptor.value, &info) != 0 || !S_ISREG(info.st_mode)) return false;
        const ssize_t count = ::read(descriptor.value, output, capacity);
        const int closed = ::close(descriptor.value);
        descriptor.value = -1;
        if (count < 0 || closed != 0) return false;
        *used = static_cast<size_t>(count);
        return true;
    }
    /**
     * @brief 读取CLOCK_MONOTONIC并换算纳秒。
     * @return 成功时为秒乘10^9加纳秒，失败返回0；调用方按约定拒绝无效零时间。
     */
    static uint64_t doSteadyNs() noexcept {
        timespec value{};
        if (::clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0U;
        return static_cast<uint64_t>(value.tv_sec) * UINT64_C(1000000000) + static_cast<uint64_t>(value.tv_nsec);
    }
    // 单一stdin消费者；先零等待poll，只有POLLIN/HUP才读一个字节，不改变fd flags。
    /**
     * @brief 零超时poll stdin，只有POLLIN/POLLHUP才尝试一次单字节read。
     * @param[out] byte 成功时写出字节，其余路径不改。
     * @param[out] info 必须有效，先清零后记录revents、errno和read_attempted。
     * @return poll中断或暂不可读为kWouldBlock，失效描述符/POLLERR为kError；read零为kClosed，
     * read错误为kError，成功字节为kOk。可读/HUP同时出现时先交付缓冲字节。
     * @pre stdin必须只有一个消费者，避免poll就绪后字节被其它读者取走。
     * @note 此实现对read的EINTR与poll的EINTR分类不同，应以实际返回和info解释。
     */
    static HostIoResult doPollInput(uint8_t* byte, HostInputInfo* info) noexcept {
        if (info == nullptr) return HostIoResult::kError;
        *info = {};
        if (byte == nullptr) {
            info->error = EINVAL;
            return HostIoResult::kError;
        }
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1U, 0);
        info->events = descriptor.revents;
        if (ready < 0) {
            info->error = errno;
            return errno == EINTR ? HostIoResult::kWouldBlock : HostIoResult::kError;
        }
        if ((descriptor.revents & POLLNVAL) != 0) {
            info->error = EBADF;
            return HostIoResult::kError;
        }
        if ((descriptor.revents & POLLERR) != 0) {
            info->error = EIO;
            return HostIoResult::kError;
        }
        if (ready == 0 || (descriptor.revents & (POLLIN | POLLHUP)) == 0) return HostIoResult::kWouldBlock;
        info->read_attempted = true;
        uint8_t value{};
        const ssize_t count = ::read(STDIN_FILENO, &value, 1U);
        if (count > 0) {
            *byte = value;
            return HostIoResult::kOk;
        }
        if (count == 0) return HostIoResult::kClosed;
        info->error = errno;
        return HostIoResult::kError;
    }
    // 与原进程级signal注册一致；仅启动owner安装，失败回滚已安装的SIGINT。
    /**
     * @brief 由启动owner安装进程级终止处理函数，第二步失败回滚SIGINT。
     * @param handler 非空信号回调。
     * @param include_interrupt 为false只安装SIGTERM，为true先SIGINT再SIGTERM。
     * @return 所需安装均成功为true，不保存供close自动恢复的注册状态。
     * @note 不可由多个worker并发重设进程信号策略，回调须满足信号上下文限制。
     */
    static bool doInstallTerminationHandler(HostSignalHandler handler, bool include_interrupt) noexcept {
        if (handler == nullptr) return false;
        if (!include_interrupt) return ::signal(SIGTERM, handler) != SIG_ERR;
        const auto previous = ::signal(SIGINT, handler);
        if (previous == SIG_ERR) return false;
        if (::signal(SIGTERM, handler) != SIG_ERR) return true;
        (void)::signal(SIGINT, previous);
        return false;
    }
    /**
     * @brief 读取指定clock，检查timespec范围及uint64_t微秒换算溢出。
     * @param clock CLOCK_REALTIME等系统clockid。
     * @param[out] output 成功时写微秒，失败时保持原值。
     * @return 指针、系统调用、秒/纳秒及整数范围均合法为true。
     */
    static bool clock_us(clockid_t clock, uint64_t* output) noexcept {
        if (output == nullptr) return false;
        timespec value{};
        if (::clock_gettime(clock, &value) != 0 || value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L)
            return false;
        const auto seconds = static_cast<uint64_t>(value.tv_sec);
        if (seconds > UINT64_MAX / UINT64_C(1000000)) return false;
        const uint64_t whole = seconds * UINT64_C(1000000);
        const uint64_t fraction = static_cast<uint64_t>(value.tv_nsec) / 1000U;
        if (whole > UINT64_MAX - fraction) return false;
        *output = whole + fraction;
        return true;
    }
    /**
     * @brief 借clock_us读取CLOCK_REALTIME。
     * @param[out] value 成功时接收Unix墙钟微秒。
     * @return 读取和范围检查成功为true；可跳变，不能代替单调控制期限。
     */
    static bool doWallTimeUs(uint64_t* value) noexcept { return clock_us(CLOCK_REALTIME, value); }
    /**
     * @brief 在工具链提供线程CPU时钟时读取当前线程消耗的CPU微秒。
     * @param[out] value 成功时写出，失败保持旧值。
     * @return 支持且读取合法为true；不支持直接false，不以墙钟伪造CPU时间。
     */
    static bool doThreadCpuTimeUs(uint64_t* value) noexcept {
#if defined(CLOCK_THREAD_CPUTIME_ID)
        return clock_us(CLOCK_THREAD_CPUTIME_ID, value);
#else
        (void)value; return false;
#endif
    }
    /**
     * @brief 对非空非零字节缓冲只尝试一次stderr write并忽略结果。
     * @param[in] bytes 借用诊断缓冲。
     * @param size 请求字节数。
     * @note 使用信号上下文可用的write，但不保证短写重试、成功或慢设备延迟上界。
     */
    static void doSignalSafeError(const uint8_t* bytes, size_t size) noexcept {
        if (bytes != nullptr && size != 0U) {
            const ssize_t ignored = ::write(STDERR_FILENO, bytes, size);
            (void)ignored;
        }
    }
    /**
     * @brief 用_exit立即结束进程，不执行C++析构或普通退出处理器。
     * @param code 退出状态，具体可观察范围按系统退出约定。
     */
    [[noreturn]] static void doExitImmediately(int code) noexcept { ::_exit(code); }
    /**
     * @brief 先将对象状态设为关闭，再尽力close此前自持描述符。
     * @return 原本已关闭或close成功为true；错误时对象也不再保存旧描述符以重试。
     * @note 调用者须先停止所有使用者；不会关闭open_output借用的原始描述符。
     */
    bool doClose() noexcept {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        return descriptor < 0 || ::close(descriptor) == 0;
    }
    int descriptor_ = -1; ///< 当前自持描述符，-1代表关闭态，原借用fd不在此存储。
};
}  // namespace dp::osal
