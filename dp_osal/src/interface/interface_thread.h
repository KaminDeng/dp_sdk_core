/** @file interface_thread.h
 *  @brief CRTP thread interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#include <functional>

namespace dp::osal {

/** @brief CRTP base for OS thread implementations. */
template <typename Impl>
class ThreadBase {
public:
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
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_THREAD_H_
