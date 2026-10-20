/**
 * @file interface_thread.h
 * @brief 线程入口、上下文所有权及生命周期操作的 CRTP 约定。
 * @details 兼容raw入口可复用普通启动包装；owned入口另保留checked/noexcept与成功后的恰好一次cleanup。
 * 基类只转发，不统一后端优先级、栈分配、停止与挂起能力；对象及借用上下文必须
 * 在后端最后一次访问之前存活，不能把 detach 或 isRunning 当作释放内存的同步证明。
 */
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#ifndef DP_OSAL_ENABLE_LEGACY_COMPAT
#define DP_OSAL_ENABLE_LEGACY_COMPAT 0
#endif

#include <functional>
#include <type_traits>
#include <utility>

namespace dp::osal {

/**
 * @brief 原始线程入口，参数为调用者提供的上下文指针。
 */
using ThreadEntry = void (*)(void *);
/**
 * @brief owned 上下文清理入口，不允许抛出异常，在后端不再访问 Thread 后执行。
 */
#if DP_OSAL_ENABLE_LEGACY_COMPAT
using ThreadCleanup = void (*)(void *) noexcept;
#endif

/** @brief 将线程操作静态绑定到具体后端的基类。
 * @tparam Impl 实现 doStart/doStop 等操作且授予基类访问权限的线程类型。 */
template <typename Impl>
class ThreadBase {
public:
    /**
     * @brief 使用函数指针及借用上下文启动线程，不移交上下文释放责任。
     * @param[in] name 线程名称；所需存活期/复制方式由后端定义。
     * @param taskFunction 有效原始入口。
     * @param taskArgument 借用参数，至少存活至线程最后一次使用。
     * @param priority 后端优先级编码，零值含义由后端定义。
     * @param stack_size 请求栈大小，单位及零默认由后端规定，当前 CMSIS 使用字节。
     * @param pstack 可选调用者栈存储，使用方式及对齐/存活期由后端规定。
     * @return 原样转发后端整数结果，不统一错误编码或运行中重复 start 的语义。
     */
#if DP_OSAL_ENABLE_LEGACY_COMPAT
    int startRaw(const char *name, ThreadEntry taskFunction, void *taskArgument, int priority = 0,
                 int stack_size = 0, void *pstack = nullptr) {
        return impl().doStartRaw(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }
#endif

    /**
     * @brief 启动带上下文清理责任转移的 raw 线程。
     * @param[in] name 后端线程名。
     * @param entry 原始任务入口。
     * @param context 待移交的上下文，可内嵌当前 Thread 对象。
     * @param cleanup 非抛出清理回调，成功后由后端在最后一次访问 Thread 之后恰好调用一次。
     * @param priority 后端优先级编码。
     * @param stack_size 后端约定的栈大小。
     * @param stack 可选外部栈存储，生命周期须满足后端要求。
     * @return 后端启动结果；失败时调用者仍拥有 context 并负责释放。
     * @note 如果后端没有匹配的 owned raw 能力，实例化走 static_assert，不能靠返回 -1 静默降级。
     */
#if DP_OSAL_ENABLE_LEGACY_COMPAT
    int startRawOwned(const char* name, ThreadEntry entry, void* context, ThreadCleanup cleanup,
                      int priority = 0, int stack_size = 0, void* stack = nullptr) noexcept {
        if constexpr (decltype(supportsOwned<Impl>(0))::value) {
            return impl().doStartRawOwned(name, entry, context, cleanup, priority, stack_size, stack);
        } else {
            static_assert(!std::is_same_v<Impl, Impl>, "该OSAL线程后端未实现owned raw cleanup；请显式适配后启用");
            return -1;
        }
    }
#endif

    /**
     * @brief 使用 std::function 任务包装启动线程。
     * @param[in] name 线程名，复制和存活期依后端约定。
     * @param taskFunction 可调用任务包装；构造/复制可能涉及动态资源。
     * @param taskArgument 借用的任务参数。
     * @param priority 后端优先级编码。
     * @param stack_size 后端栈大小请求。
     * @param pstack 可选外部栈，后端决定是否使用。
     * @return 后端结果，不能跨后端假定所有错误码和重复调用语义相同。
     */
    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority = 0,
              int stack_size = 0, void *pstack = nullptr) {
        return impl().doStart(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    /**
     * @brief 请求后端停止线程，等待/协作/终止策略由具体实现决定。
     * @note 本操作只管理软件线程，不会自动取消机械臂动作或失能电机。
     */
    void stop() { impl().doStop(); }
    /**
     * @brief 由后端等待线程结束；可连接状态及不能自连接等要求由调用者遵守。
     */
    void join() { impl().doJoin(); }
    /**
     * @brief 按后端规则分离线程资源，不代表任务已结束或上下文可立即释放。
     */
    void detach() { impl().doDetach(); }
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
     * @brief 优先探测后端是否存在匹配 owned raw 签名，只用于编译期类型判断。
     * @tparam Backend 被探测的后端类型。
     * @details 未命名 int 参数仅用于优先重载选择，不执行运行时调用。
     * @return 可匹配时为 std::true_type。
     */
#if DP_OSAL_ENABLE_LEGACY_COMPAT
    template <typename Backend>
    static auto supportsOwned(int) -> decltype(std::declval<Backend&>().doStartRawOwned(
        static_cast<const char*>(nullptr), ThreadEntry{}, static_cast<void*>(nullptr), ThreadCleanup{},
        int{}, int{}, static_cast<void*>(nullptr)), std::true_type{});
    /**
     * @brief owned raw 探测失败时的省略号后备重载，只用于未求值的类型判断。
     * @return std::false_type，触发显式不支持诊断。
     */
    template <typename> static std::false_type supportsOwned(...);
#endif

    /**
     * @brief 静态转回具体线程后端。
     * @return 当前 Impl 可写引用。
     */
    Impl &impl() { return *static_cast<Impl *>(this); }
    /**
     * @brief 静态转回具体线程后端的只读视图。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_THREAD_H_
