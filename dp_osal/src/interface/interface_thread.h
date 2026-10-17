/** @file interface_thread.h
 *  @brief CRTP thread interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace dp::osal {

using ThreadEntry = void (*)(void *);
using ThreadCleanup = void (*)(void *) noexcept;

/** @brief CRTP base for OS thread implementations. */
template <typename Impl>
class ThreadBase {
public:
    int startRaw(const char *name, ThreadEntry taskFunction, void *taskArgument, int priority = 0,
                 int stack_size = 0, void *pstack = nullptr) {
        return impl().doStartRaw(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    // 成功后backend负责恰好一次cleanup；失败时调用者保留context所有权。
    // cleanup在backend最后一次访问Thread对象之后执行，可销毁内嵌Thread的context。
    int startRawOwned(const char* name, ThreadEntry entry, void* context, ThreadCleanup cleanup,
                      int priority = 0, int stack_size = 0, void* stack = nullptr) noexcept {
        if constexpr (decltype(supportsOwned<Impl>(0))::value) {
            return impl().doStartRawOwned(name, entry, context, cleanup, priority, stack_size, stack);
        } else {
            static_assert(!std::is_same_v<Impl, Impl>, "该OSAL线程后端未实现owned raw cleanup；请显式适配后启用");
            return -1;
        }
    }

    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority = 0,
              int stack_size = 0, void *pstack = nullptr) {
        return impl().doStart(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    void stop() { impl().doStop(); }
    void join() { impl().doJoin(); }
    void detach() { impl().doDetach(); }
    int suspend() { return impl().doSuspend(); }
    int resume() { return impl().doResume(); }
    [[nodiscard]] bool isRunning() const { return impl().doIsRunning(); }
    void setPriority(int priority) { impl().doSetPriority(priority); }
    [[nodiscard]] int getPriority() const { return impl().doGetPriority(); }

protected:
    ~ThreadBase() = default;

private:
    template <typename Backend>
    static auto supportsOwned(int) -> decltype(std::declval<Backend&>().doStartRawOwned(
        static_cast<const char*>(nullptr), ThreadEntry{}, static_cast<void*>(nullptr), ThreadCleanup{},
        int{}, int{}, static_cast<void*>(nullptr)), std::true_type{});
    template <typename> static std::false_type supportsOwned(...);

    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_THREAD_H_
