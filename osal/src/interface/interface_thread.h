/** @file interface_thread.h
 *  @brief Abstract thread interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ITHREAD_H_
#define ITHREAD_H_

namespace osal {

/** @brief Abstract OS thread interface.
 *
 *  Wraps the underlying threading primitive (@c pthread_t on POSIX or an
 *  RTOS task handle on CMSIS-OS2) behind a uniform API covering creation,
 *  lifecycle management, and priority control. */
class IThread {
public:
    virtual ~IThread() = default;

    /** @brief Creates and starts a new thread.
     *  @param name          Null-terminated thread name (used by OS debuggers).
     *  @param taskFunction  Thread entry point accepting a @c void* argument.
     *  @param taskArgument  Opaque argument passed to @p taskFunction.
     *  @param priority      OS-specific thread priority (0 = default).
     *  @param stack_size    Stack size in bytes (0 = use port default).
     *  @param pstack        Pre-allocated stack buffer, or @c nullptr for dynamic allocation.
     *  @return 0 on success, non-zero on error. */
    virtual int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority = 0,
                      int stack_size = 0, void *pstack = nullptr) = 0;

    /** @brief Requests the thread to stop and waits for termination. */
    virtual void stop() = 0;

    /** @brief Blocks the caller until this thread finishes execution. */
    virtual void join() = 0;

    /** @brief Detaches the thread, allowing it to run independently. */
    virtual void detach() = 0;

    /** @brief Suspends the thread (platform-dependent availability).
     *  @return 0 on success, non-zero on error. */
    virtual int suspend() = 0;

    /** @brief Resumes a previously suspended thread.
     *  @return 0 on success, non-zero on error. */
    virtual int resume() = 0;

    /** @brief Checks whether the thread is currently running.
     *  @return @c true if the thread is active, @c false otherwise. */
    [[nodiscard]] virtual bool isRunning() const = 0;

    /** @brief Sets the thread scheduling priority.
     *  @param priority  New OS-specific priority value. */
    virtual void setPriority(int priority) = 0;

    /** @brief Returns the current thread scheduling priority.
     *  @return OS-specific priority value. */
    [[nodiscard]] virtual int getPriority() const = 0;
};

}  // namespace osal
#endif  // ITHREAD_H_
