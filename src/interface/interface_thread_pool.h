/** @file interface_thread_pool.h
 *  @brief Abstract thread-pool interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ITHREAD_POOL_H_
#define ITHREAD_POOL_H_

namespace osal {

/** @brief Abstract fixed-size thread pool interface.
 *
 *  Manages a pool of worker threads that dequeue and execute submitted
 *  tasks.  Supports dynamic priority setting, suspend/resume, and
 *  task cancellation.  Concrete implementations target either POSIX
 *  pthreads or CMSIS-OS2 tasks. */
class IThreadPool {
public:
    virtual ~IThreadPool() = default;

    /** @brief Creates and starts worker threads.
     *  @param numThreads  Number of worker threads to spawn.
     *  @param priority    OS-specific thread priority (0 = default).
     *  @param stack_size  Stack size in bytes per thread (0 = port default). */
    virtual void start(uint32_t numThreads, int priority = 0, int stack_size = 0) = 0;

    /** @brief Stops all worker threads and waits for them to exit. */
    virtual void stop() = 0;

    /** @brief Suspends all worker threads.
     *  @return 0 on success, non-zero on error. */
    virtual int suspend() = 0;

    /** @brief Resumes all previously suspended worker threads.
     *  @return 0 on success, non-zero on error. */
    virtual int resume() = 0;

    /** @brief Checks whether the thread pool has been started.
     *  @return @c true if the pool is running, @c false otherwise. */
    [[nodiscard]] virtual bool isStarted() const = 0;

    /** @brief Checks whether all worker threads are suspended.
     *  @return @c true if suspended, @c false otherwise. */
    [[nodiscard]] virtual bool isSuspended() const = 0;

    /** @brief Submits a task for execution by any available worker thread.
     *  @param taskFunction  Function to execute.
     *  @param taskArgument  Opaque argument passed to @p taskFunction.
     *  @param priority      Scheduling priority hint for this task.
     *  @return              Unique task ID that can be passed to cancelTask(uint32_t). */
    virtual uint32_t submit(std::function<void(void *)> taskFunction, void *taskArgument, int priority) = 0;

    /** @brief Sets the scheduling priority for all worker threads.
     *  @param priority  New OS-specific priority value. */
    virtual void setPriority(int priority) = 0;

    /** @brief Returns the current scheduling priority of the worker threads.
     *  @return OS-specific priority value. */
    [[nodiscard]] virtual int getPriority() const = 0;

    /** @brief Returns the number of tasks currently waiting in the queue.
     *  @return Pending task count. */
    virtual size_t getTaskQueueSize() = 0;

    /** @brief Returns the number of worker threads currently executing tasks.
     *  @return Active thread count. */
    [[nodiscard]] virtual uint32_t getActiveThreadCount() const = 0;

    /** @brief Removes a pending task from the queue before it executes.
     *  @param taskId  The task ID returned by submit().
     *  @return @c true if the task was found and removed, @c false otherwise. */
    virtual bool cancelTask(uint32_t taskId) = 0;

    /** @brief Removes a pending task from the queue before it executes (RTTI variant).
     *  @param taskFunction  The task function to cancel.
     *  @return @c true if the task was found and removed, @c false otherwise.
     *  @note Only available when RTTI is enabled. Falls back to returning false. */
    virtual bool cancelTask(std::function<void(void *)> &taskFunction) = 0;

    /** @brief Registers a callback invoked when a task function throws or fails.
     *  @param callback  Error handler receiving the task argument. */
    virtual void setTaskFailureCallback(std::function<void(void *)> callback) = 0;

    /** @brief Sets the maximum number of concurrent worker threads.
     *  @param maxThreads  Upper thread-count limit. */
    virtual void setMaxThreads(uint32_t maxThreads) = 0;

    /** @brief Returns the configured maximum number of worker threads.
     *  @return Maximum thread count. */
    [[nodiscard]] virtual uint32_t getMaxThreads() const = 0;

    /** @brief Sets the minimum number of threads kept alive when idle.
     *  @param minThreads  Lower thread-count limit. */
    virtual void setMinThreads(uint32_t minThreads) = 0;

    /** @brief Returns the configured minimum number of worker threads.
     *  @return Minimum thread count. */
    [[nodiscard]] virtual uint32_t getMinThreads() const = 0;
};
}  // namespace osal
#endif  // ITHREAD_POOL_H_
