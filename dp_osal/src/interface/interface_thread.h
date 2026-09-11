
#ifndef DP_OSAL_INTERFACE_THREAD_H_
#define DP_OSAL_INTERFACE_THREAD_H_

#ifndef DP_OSAL_ENABLE_LEGACY_COMPAT
#define DP_OSAL_ENABLE_LEGACY_COMPAT 0
#endif

#include <functional>
#include <type_traits>
#include <utility>

namespace dp::osal {


using ThreadEntry = void (*)(void *);

#if DP_OSAL_ENABLE_LEGACY_COMPAT
#endif


template <typename Impl>
class ThreadBase {
public:

#if DP_OSAL_ENABLE_LEGACY_COMPAT
    int startRaw(const char *name, ThreadEntry taskFunction, void *taskArgument, int priority = 0,
                 int stack_size = 0, void *pstack = nullptr) {
        return impl().doStartRaw(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }
#endif


#if DP_OSAL_ENABLE_LEGACY_COMPAT

#endif


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
