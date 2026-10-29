/**
 * @file interface_thread.h
 * @brief POSIX风格线程生命周期：显式创建、协作停止、连接后回收。
 * @details 单owner串行操作生命周期及调度扩展；仅joinable/isRunning/stopRequested/requestStop允许跨线程。
 * 成功join才允许释放借用上下文、栈和控制块；超时/失败保留回收责任。
 */
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace dp::osal {
using ThreadEntry = void (*)(void*);

/** @brief 统一错误类别，native_error保留后端诊断值。 */
enum class ThreadStatus : uint8_t {
    kSuccess,
    kTimeout,
    kNotSupported,
    kInvalidState,
    kBusy,
    kWouldDeadlock,
    kResourceExhausted,
    kNativeError,
    kInvalidArgument
};
struct ThreadResult {
    ThreadStatus status;
    int native_error;
    constexpr ThreadResult(ThreadStatus code, int native = 0) noexcept : status(code), native_error(native) {}
    [[nodiscard]] constexpr bool ok() const noexcept { return status == ThreadStatus::kSuccess; }
    /** @brief 支持既有0成功判断；新调用者使用status/ok，不丢弃原生诊断。 */
    constexpr operator int() const noexcept { return ok() ? 0 : -static_cast<int>(status); }
};

/** @brief 借用的原生控制资源；只在成功join后允许回收，容量统一字节。 */
struct ThreadResources {
    void* control_block = nullptr;
    uint32_t control_block_size = 0;
    void* completion_control_block = nullptr;
    uint32_t completion_control_block_size = 0;
};
struct ThreadAttributes {
    const char* name = "dp_osal";
    int priority = 0;  ///< 后端编码；默认值由所选port解释，不统一实时调度等级。
    uint32_t stack_size_bytes = 0;
    void* stack = nullptr;
    ThreadResources resources{};
};

namespace detail {
/** @brief 内部入口存储，函数指针不额外分配；捕获callable可由std::function分配。
 * 不持有上下文，不包含最终清理/自销毁策略。
 */
class ThreadEntryStorage {
public:
    using Function = std::function<void(void*)>;
    ThreadEntryStorage() noexcept = default;
    explicit ThreadEntryStorage(ThreadEntry entry) noexcept : entry_(entry) {}
    explicit ThreadEntryStorage(Function function) noexcept : function_(std::move(function)) {}
    [[nodiscard]] bool valid() const noexcept { return entry_ != nullptr || static_cast<bool>(function_); }
    void run(void* context) {
        if (entry_ != nullptr)
            entry_(context);
        else if (function_)
            function_(context);
    }
    void reset() noexcept {
        entry_ = nullptr;
        function_ = nullptr;
    }

private:
    ThreadEntry entry_ = nullptr;
    Function function_{};
};

/** @brief 活线程/清理待办不能随包装对象析构；Release同样不可忽略。 */
[[noreturn]] inline void threadLifetimeViolation() noexcept { std::terminate(); }
}  // namespace detail

/** @brief 静态派发的线程合同；对象不可移动，默认创建joinable线程。
 * start失败不接管借用上下文；已有回收责任时返回Busy，不隐式连接旧线程。
 * 析构前由owner显式停止/连接；不会自动强杀、分离或无限等待。
 */
template <typename Impl>
class ThreadBase {
public:
    template <typename Callable>
    ThreadResult start(Callable&& callable, void* context = nullptr,
                       const ThreadAttributes& attributes = {}) noexcept(std::is_convertible_v<Callable, ThreadEntry>) {
        if (attributes.stack_size_bytes > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            return {ThreadStatus::kInvalidArgument};
        return start(attributes.name, std::forward<Callable>(callable), context, attributes.resources,
                     attributes.priority, static_cast<int>(attributes.stack_size_bytes), attributes.stack);
    }

    /** @brief 同一启动机制的具名便捷重载，栈参数单位字节。 */
    template <typename Callable>
    ThreadResult start(const char* name, Callable&& callable, void* context = nullptr, int priority = 0,
                       int stack_size = 0,
                       void* stack = nullptr) noexcept(std::is_convertible_v<Callable, ThreadEntry>) {
        return start(name, std::forward<Callable>(callable), context, ThreadResources{}, priority, stack_size, stack);
    }

    template <typename Callable>
    ThreadResult start(const char* name, Callable&& callable, void* context, const ThreadResources& resources,
                       int priority = 0, int stack_size = 0,
                       void* stack = nullptr) noexcept(std::is_convertible_v<Callable, ThreadEntry>) {
        if (stack_size < 0) return {ThreadStatus::kInvalidArgument};
        if constexpr (std::is_convertible_v<Callable, ThreadEntry>) {
            return impl().doStart(name, detail::ThreadEntryStorage(static_cast<ThreadEntry>(callable)), context,
                                  priority, stack_size, stack, resources);
        } else {
            return impl().doStart(name,
                                  detail::ThreadEntryStorage(
                                      typename detail::ThreadEntryStorage::Function(std::forward<Callable>(callable))),
                                  context, priority, stack_size, stack, resources);
        }
    }

    static constexpr bool supportsTimedJoin() noexcept { return Impl::kSupportsTimedJoin; }
    static constexpr bool supportsControlBlockStorage() noexcept { return Impl::kSupportsControlBlockStorage; }
    [[nodiscard]] bool joinable() const noexcept { return impl().doJoinable(); }
    bool requestStop() noexcept { return impl().doRequestStop(); }
    [[nodiscard]] bool stopRequested() const noexcept { return impl().doStopRequested(); }
    ThreadResult join() { return impl().doJoin(); }
    /** @brief 毫秒相对期限覆盖全部等待和回收；不支持时显式拒绝，不偷偷无限等。 */
    ThreadResult tryJoinFor(uint32_t timeout_ms) { return impl().doTryJoinFor(timeout_ms); }
    ThreadResult detach() { return impl().doDetach(); }
    /** @brief 显式的停止请求+无限连接便捷操作；不是析构策略。 */
    ThreadResult stop() {
        requestStop();
        return join();
    }
    // 挂起、恢复及优先级是后端扩展，不作为跨平台安全点。
    int suspend() { return impl().doSuspend(); }
    /**
     * @brief 请求恢复线程。
     * @return 后端整数结果；恢复请求成功不代表任务已经获得 CPU。
     */
    int resume() { return impl().doResume(); }
    /**
     * @brief 查询后端维护的运行状态。
     * @return 状态观测值；并发变化后可立即过时，不是线程结束的同步屏障。
     */
    [[nodiscard]] bool isRunning() const { return impl().doIsRunning(); }
    /**
     * @brief 请求调整线程优先级。
     * @param priority 后端编码；权限、范围和错误处理由实现决定。
     */
    void setPriority(int priority) { impl().doSetPriority(priority); }
    /**
     * @brief 获取后端维护或查询的优先级。
     * @return 后端优先级值，不跨平台统一调度含义。
     */
    [[nodiscard]] int getPriority() const { return impl().doGetPriority(); }

protected:
    ThreadBase() = default;
    ~ThreadBase() = default;
    ThreadBase(const ThreadBase&) = delete;
    ThreadBase& operator=(const ThreadBase&) = delete;
    ThreadBase(ThreadBase&&) = delete;
    ThreadBase& operator=(ThreadBase&&) = delete;

private:
    Impl& impl() { return *static_cast<Impl*>(this); }
    const Impl& impl() const { return *static_cast<const Impl*>(this); }
};
}  // namespace dp::osal
#endif  // DP_OSAL_INTERFACE_THREAD_H_
