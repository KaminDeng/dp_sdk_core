/**
 * @file interface_thread.h
 * @brief 线程入口、上下文所有权及生命周期操作的 CRTP 约定。
 * @details 函数指针、callable与最终清理共用启动/派发；成功清理一次，失败不转移上下文。
 * 基类只转发，不统一后端优先级、栈分配、停止与挂起能力；对象及借用上下文必须
 * 在后端最后一次访问之前存活，不能把 detach 或 isRunning 当作释放内存的同步证明。
 */
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dp::osal {

using ThreadEntry = void (*)(void*);
using ThreadCleanup = void (*)(void*) noexcept;

/** @brief 成功启动后移交的最终清理；失败仍由调用者持有上下文。
 * cleanup只在后端最后一次对象访问之后执行，不能释放仍执行中的外部栈或TCB。
 * 入口须正常返回或遵守后端协作停止约定；未捕获的用户异常仍按C++线程规则终止进程。
 */
struct ThreadFinalizer {
    ThreadCleanup cleanup;
};

/** @brief 原生控制资源视图；存储由调用者持有，不由Thread释放。
 * Detached不创建连接信号量，调用者负责保证Thread及静态资源覆盖任务整个存活期。
 * Joinable成功join后才可回收/复用栈和控制块。容量以字节表达。
 */
enum class ThreadCompletion : uint8_t { kJoinable, kDetached };
struct ThreadResources {
    void* control_block = nullptr;
    uint32_t control_block_size = 0;
    void* completion_control_block = nullptr;
    uint32_t completion_control_block_size = 0;
    ThreadCompletion completion = ThreadCompletion::kJoinable;
};

/** @brief 唯一任务存储；函数指针和callable共享存储，不重复维护入口状态。
 * std::function构造允许分配/抛异常；函数指针及finalizer构造不分配且不抛异常。
 */
class ThreadTask {
public:
    using Function = std::function<void(void*)>;
    ThreadTask() noexcept = default;
    explicit ThreadTask(ThreadEntry entry) noexcept : value_(entry) {}
    explicit ThreadTask(Function function) noexcept : value_(std::move(function)) {}
    ThreadTask(ThreadEntry entry, ThreadFinalizer finalizer) noexcept
        : value_(FinalizedEntry{entry, finalizer.cleanup}) {}

    bool valid() const noexcept {
        if (const auto* entry = std::get_if<ThreadEntry>(&value_)) return *entry != nullptr;
        if (const auto* entry = std::get_if<FinalizedEntry>(&value_))
            return entry->entry != nullptr && entry->cleanup != nullptr;
        const auto* function = std::get_if<Function>(&value_);
        return function != nullptr && static_cast<bool>(*function);
    }
    ThreadCleanup finalizer() const noexcept {
        const auto* entry = std::get_if<FinalizedEntry>(&value_);
        return entry != nullptr ? entry->cleanup : nullptr;
    }
    void run(void* context) {
        if (auto* entry = std::get_if<ThreadEntry>(&value_))
            (*entry)(context);
        else if (auto* finalized = std::get_if<FinalizedEntry>(&value_))
            finalized->entry(context);
        else if (auto* function = std::get_if<Function>(&value_)) {
            if (*function) (*function)(context);
        }
    }
    void reset() noexcept { value_.emplace<std::monostate>(); }

private:
    struct FinalizedEntry {
        ThreadEntry entry;
        ThreadCleanup cleanup;
    };
    std::variant<std::monostate, ThreadEntry, Function, FinalizedEntry> value_;
};

/** @brief Thread CRTP正式契约。生命周期操作由同一owner串行调用。
 * isRunning仅是观测；对象、借用上下文和外部栈至少存活至join/stop完成。
 */
template <typename Impl>
class ThreadBase {
public:
    /** @brief 启动普通任务。函数指针直接存储；其它callable显式保存为std::function。
     * 栈长度以字节表达；零使用后端默认；外部栈须满足端口容量/对齐要求。
     * 返回0表示成功，非零不接管上下文；重复启动未结束任务返回失败。
     */
    template <typename Callable>
    int start(const char* name, Callable&& callable, void* context = nullptr, int priority = 0, int stack_size = 0,
              void* stack = nullptr) noexcept(std::is_convertible_v<Callable, ThreadEntry>) {
        if constexpr (std::is_convertible_v<Callable, ThreadEntry>) {
            return impl().doStart(name, ThreadTask(static_cast<ThreadEntry>(callable)), context, priority, stack_size,
                                  stack);
        } else {
            return impl().doStart(name, ThreadTask(ThreadTask::Function(std::forward<Callable>(callable))), context,
                                  priority, stack_size, stack);
        }
    }

    /** @brief 在同一启动机制中移交最终清理责任，不进行std::function包装。
     * 不支持此能力的后端返回失败；外部栈/TCB禁止由finalizer回收。
     * 成功后的runner可以立即结束，故调用者须在启动前完成自身所有权准备。
     * 此模式自动回收，join返回不支持；cleanup完成由独立观察对象同步，不依赖可能已销毁的Thread。
     */
    int start(const char* name, ThreadEntry entry, void* context, ThreadFinalizer finalizer, int priority = 0,
              int stack_size = 0, void* stack = nullptr) noexcept {
        return impl().doStart(name, ThreadTask(entry, finalizer), context, priority, stack_size, stack);
    }

    /** @brief 使用显式原生控制资源启动；失败不占用调用者资源。 */
    template <typename Callable>
    int start(const char* name, Callable&& callable, void* context, const ThreadResources& resources, int priority = 0,
              int stack_size = 0, void* stack = nullptr) noexcept(std::is_convertible_v<Callable, ThreadEntry>) {
        if constexpr (std::is_convertible_v<Callable, ThreadEntry>) {
            return impl().doStart(name, ThreadTask(static_cast<ThreadEntry>(callable)), context, priority, stack_size,
                                  stack, resources);
        } else {
            return impl().doStart(name, ThreadTask(ThreadTask::Function(std::forward<Callable>(callable))), context,
                                  priority, stack_size, stack, resources);
        }
    }

    static constexpr bool supportsFinalizer() noexcept { return Impl::kSupportsFinalizer; }
    static constexpr bool supportsControlBlockStorage() noexcept { return Impl::kSupportsControlBlockStorage; }
    static constexpr bool supportsDetachedResources() noexcept { return Impl::kSupportsDetachedResources; }

    /**
     * @brief 请求后端停止线程，等待/协作/终止策略由具体实现决定。
     * @note 本操作只管理软件线程，不会自动取消机械臂动作或失能电机。
     */
    void stop() { impl().doStop(); }
    /**
     * @brief 由后端等待线程结束；可连接状态及不能自连接等要求由调用者遵守。
     */
    int join() { return impl().doJoin(); }
    /**
     * @brief 按后端规则分离线程资源，不代表任务已结束或上下文可立即释放。
     */
    int detach() { return impl().doDetach(); }
    /**
     * @brief 请求挂起线程。
     * @return 后端整数结果；POSIX 当前为协作挂起标志，不等价于立即停止任意用户代码。
     */
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
    /**
     * @brief 受保护的非虚析构，具体 Thread 后端负责停止和资源清理。
     */
    ~ThreadBase() = default;

private:
    /**
     * @brief 静态转回具体线程后端。
     * @return 当前 Impl 可写引用。
     */
    Impl& impl() { return *static_cast<Impl*>(this); }
    /**
     * @brief 静态转回具体线程后端的只读视图。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl& impl() const { return *static_cast<const Impl*>(this); }
};

}  // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_THREAD_H_
