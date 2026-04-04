/** @file interface_thread_pool.h
 *  @brief CRTP thread-pool interface for OSAL. */
#ifndef OSAL_INTERFACE_THREAD_POOL_H_
#define OSAL_INTERFACE_THREAD_POOL_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#if OSAL_ENABLE_THREAD_POOL

namespace osal {

template <typename Impl>
class ThreadPoolBase {
public:
    void start(uint32_t numThreads, int priority = 0, int stack_size = 0) {
        impl().doStart(numThreads, priority, stack_size);
    }
    void stop() { impl().doStop(); }
    int suspend() { return impl().doSuspend(); }
    int resume() { return impl().doResume(); }
    [[nodiscard]] bool isStarted() const { return impl().doIsStarted(); }
    [[nodiscard]] bool isSuspended() const { return impl().doIsSuspended(); }
    uint32_t submit(std::function<void(void *)> taskFunction, void *taskArgument, int priority) {
        return impl().doSubmit(taskFunction, taskArgument, priority);
    }
    void setPriority(int priority) { impl().doSetPriority(priority); }
    [[nodiscard]] int getPriority() const { return impl().doGetPriority(); }
    size_t getTaskQueueSize() { return impl().doGetTaskQueueSize(); }
    [[nodiscard]] uint32_t getActiveThreadCount() const { return impl().doGetActiveThreadCount(); }
    bool cancelTask(uint32_t taskId) { return impl().doCancelTask(taskId); }
    bool cancelTask(std::function<void(void *)> &taskFunction) { return impl().doCancelTask(taskFunction); }
    void setTaskFailureCallback(std::function<void(void *)> callback) { impl().doSetTaskFailureCallback(callback); }
    void setMaxThreads(uint32_t maxThreads) { impl().doSetMaxThreads(maxThreads); }
    [[nodiscard]] uint32_t getMaxThreads() const { return impl().doGetMaxThreads(); }
    void setMinThreads(uint32_t minThreads) { impl().doSetMinThreads(minThreads); }
    [[nodiscard]] uint32_t getMinThreads() const { return impl().doGetMinThreads(); }

protected:
    ~ThreadPoolBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif /* OSAL_ENABLE_THREAD_POOL */

#endif  // OSAL_INTERFACE_THREAD_POOL_H_
