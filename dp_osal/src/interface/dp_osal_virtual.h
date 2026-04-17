/** @file dp_osal_virtual.h
 *  @brief Optional virtual wrappers for OSAL CRTP interfaces (host testing).
 *
 *  These wrappers add vtable indirection on top of CRTP implementations.
 *  Production code should use OSAL concrete types directly. */
#ifndef DP_OSAL_VIRTUAL_H_
#define DP_OSAL_VIRTUAL_H_

#include "interface_chrono.h"
#include "interface_condition_variable.h"
#include "interface_memory_manager.h"
#include "interface_mutex.h"
#include "interface_queue.h"
#include "interface_rwlock.h"
#include "interface_semaphore.h"
#include "interface_spin_lock.h"
#include "interface_system.h"
#include "interface_thread.h"
#include "interface_thread_pool.h"
#include "interface_timer.h"

namespace dp::osal {

class IMutex {
public:
    virtual ~IMutex() = default;
    virtual bool lock() = 0;
    virtual bool unlock() = 0;
    virtual bool tryLock() = 0;
    virtual bool tryLockFor(uint32_t timeout_ms) = 0;
};

template <typename Impl>
class MutexVirtual : public IMutex {
public:
    explicit MutexVirtual(Impl &impl) : impl_(impl) {}
    bool lock() override { return impl_.lock(); }
    bool unlock() override { return impl_.unlock(); }
    bool tryLock() override { return impl_.tryLock(); }
    bool tryLockFor(uint32_t timeout_ms) override { return impl_.tryLockFor(timeout_ms); }

private:
    Impl &impl_;
};

class ISemaphore {
public:
    virtual ~ISemaphore() = default;
    virtual void wait() = 0;
    virtual void signal() = 0;
    virtual bool tryWait() = 0;
    virtual bool tryWaitFor(uint32_t timeout_ms) = 0;
    virtual int getValue() const = 0;
    virtual void init(int initialValue) = 0;
};

template <typename Impl>
class SemaphoreVirtual : public ISemaphore {
public:
    explicit SemaphoreVirtual(Impl &impl) : impl_(impl) {}
    void wait() override { impl_.wait(); }
    void signal() override { impl_.signal(); }
    bool tryWait() override { return impl_.tryWait(); }
    bool tryWaitFor(uint32_t timeout_ms) override { return impl_.tryWaitFor(timeout_ms); }
    int getValue() const override { return impl_.getValue(); }
    void init(int initialValue) override { impl_.init(initialValue); }

private:
    Impl &impl_;
};

class IThread {
public:
    virtual ~IThread() = default;
    virtual int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority,
                      int stack_size, void *pstack) = 0;
    virtual void stop() = 0;
    virtual void join() = 0;
    virtual void detach() = 0;
    virtual int suspend() = 0;
    virtual int resume() = 0;
    virtual bool isRunning() const = 0;
    virtual void setPriority(int priority) = 0;
    virtual int getPriority() const = 0;
};

template <typename Impl>
class ThreadVirtual : public IThread {
public:
    explicit ThreadVirtual(Impl &impl) : impl_(impl) {}

    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority,
              int stack_size, void *pstack) override {
        return impl_.start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }
    void stop() override { impl_.stop(); }
    void join() override { impl_.join(); }
    void detach() override { impl_.detach(); }
    int suspend() override { return impl_.suspend(); }
    int resume() override { return impl_.resume(); }
    bool isRunning() const override { return impl_.isRunning(); }
    void setPriority(int priority) override { impl_.setPriority(priority); }
    int getPriority() const override { return impl_.getPriority(); }

private:
    Impl &impl_;
};

template <typename T>
class IQueue {
public:
    virtual ~IQueue() = default;
    virtual void send(const T &message) = 0;
    virtual T receive() = 0;
    virtual bool tryReceive(T &message) = 0;
    virtual bool receiveFor(T &message, uint32_t timeout_ms) = 0;
    virtual size_t size() const = 0;
    virtual void clear() = 0;
};

template <typename Impl, typename T>
class QueueVirtual : public IQueue<T> {
public:
    explicit QueueVirtual(Impl &impl) : impl_(impl) {}

    void send(const T &message) override { impl_.send(message); }
    T receive() override { return impl_.receive(); }
    bool tryReceive(T &message) override { return impl_.tryReceive(message); }
    bool receiveFor(T &message, uint32_t timeout_ms) override { return impl_.receiveFor(message, timeout_ms); }
    size_t size() const override { return impl_.size(); }
    void clear() override { impl_.clear(); }

private:
    Impl &impl_;
};

#if DP_OSAL_ENABLE_RW_LOCK
class IRWLock {
public:
    virtual ~IRWLock() = default;
    virtual void readLock() = 0;
    virtual bool tryReadLock() = 0;
    virtual bool readLockFor(uint32_t timeout_ms) = 0;
    virtual void readUnlock() = 0;
    virtual void writeLock() = 0;
    virtual bool tryWriteLock() = 0;
    virtual bool writeLockFor(uint32_t timeout_ms) = 0;
    virtual void writeUnlock() = 0;
    virtual size_t getReadLockCount() const = 0;
    virtual bool isWriteLocked() const = 0;
};

template <typename Impl>
class RWLockVirtual : public IRWLock {
public:
    explicit RWLockVirtual(Impl &impl) : impl_(impl) {}

    void readLock() override { impl_.readLock(); }
    bool tryReadLock() override { return impl_.tryReadLock(); }
    bool readLockFor(uint32_t timeout_ms) override { return impl_.readLockFor(timeout_ms); }
    void readUnlock() override { impl_.readUnlock(); }
    void writeLock() override { impl_.writeLock(); }
    bool tryWriteLock() override { return impl_.tryWriteLock(); }
    bool writeLockFor(uint32_t timeout_ms) override { return impl_.writeLockFor(timeout_ms); }
    void writeUnlock() override { impl_.writeUnlock(); }
    size_t getReadLockCount() const override { return impl_.getReadLockCount(); }
    bool isWriteLocked() const override { return impl_.isWriteLocked(); }

private:
    Impl &impl_;
};
#endif

#if DP_OSAL_ENABLE_SPIN_LOCK
class ISpinLock {
public:
    virtual ~ISpinLock() = default;
    virtual void lock() = 0;
    virtual bool tryLock() = 0;
    virtual bool lockFor(uint32_t timeout_ms) = 0;
    virtual void unlock() = 0;
    virtual bool isLocked() const = 0;
};

template <typename Impl>
class SpinLockVirtual : public ISpinLock {
public:
    explicit SpinLockVirtual(Impl &impl) : impl_(impl) {}

    void lock() override { impl_.lock(); }
    bool tryLock() override { return impl_.tryLock(); }
    bool lockFor(uint32_t timeout_ms) override { return impl_.lockFor(timeout_ms); }
    void unlock() override { impl_.unlock(); }
    bool isLocked() const override { return impl_.isLocked(); }

private:
    Impl &impl_;
};
#endif

#if DP_OSAL_ENABLE_CONDITION_VAR
class IConditionVariable {
public:
    virtual ~IConditionVariable() = default;
    virtual void wait(Mutex &mutex) = 0;
    virtual bool waitFor(Mutex &mutex, uint32_t timeout_ms) = 0;
    virtual void notifyOne() = 0;
    virtual void notifyAll() = 0;
    virtual int getWaitCount() const = 0;
};

template <typename Impl>
class ConditionVariableVirtual : public IConditionVariable {
public:
    explicit ConditionVariableVirtual(Impl &impl) : impl_(impl) {}

    void wait(Mutex &mutex) override { impl_.wait(mutex); }
    bool waitFor(Mutex &mutex, uint32_t timeout_ms) override { return impl_.waitFor(mutex, timeout_ms); }
    void notifyOne() override { impl_.notifyOne(); }
    void notifyAll() override { impl_.notifyAll(); }
    int getWaitCount() const override { return impl_.getWaitCount(); }

private:
    Impl &impl_;
};
#endif

#if DP_OSAL_ENABLE_TIMER
class ITimer {
public:
    virtual ~ITimer() = default;
    virtual void start(uint32_t interval_ms, bool periodic, std::function<void()> callback) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual uint32_t getRemainingTime() const = 0;
    virtual void reset() = 0;
};

template <typename Impl>
class TimerVirtual : public ITimer {
public:
    explicit TimerVirtual(Impl &impl) : impl_(impl) {}

    void start(uint32_t interval_ms, bool periodic, std::function<void()> callback) override {
        impl_.start(interval_ms, periodic, callback);
    }
    void stop() override { impl_.stop(); }
    bool isRunning() const override { return impl_.isRunning(); }
    uint32_t getRemainingTime() const override { return impl_.getRemainingTime(); }
    void reset() override { impl_.reset(); }

private:
    Impl &impl_;
};
#endif

#if DP_OSAL_ENABLE_THREAD_POOL
class IThreadPool {
public:
    virtual ~IThreadPool() = default;
    virtual void start(uint32_t num_threads, int priority, int stack_size) = 0;
    virtual void stop() = 0;
    virtual int suspend() = 0;
    virtual int resume() = 0;
    virtual bool isStarted() const = 0;
    virtual bool isSuspended() const = 0;
    virtual uint32_t submit(std::function<void(void *)> task_function, void *task_argument, int priority) = 0;
    virtual void setPriority(int priority) = 0;
    virtual int getPriority() const = 0;
    virtual size_t getTaskQueueSize() = 0;
    virtual uint32_t getActiveThreadCount() const = 0;
    virtual bool cancelTask(uint32_t task_id) = 0;
    virtual bool cancelTask(std::function<void(void *)> &task_function) = 0;
    virtual void setTaskFailureCallback(std::function<void(void *)> callback) = 0;
    virtual void setMaxThreads(uint32_t max_threads) = 0;
    virtual uint32_t getMaxThreads() const = 0;
    virtual void setMinThreads(uint32_t min_threads) = 0;
    virtual uint32_t getMinThreads() const = 0;
};

template <typename Impl>
class ThreadPoolVirtual : public IThreadPool {
public:
    explicit ThreadPoolVirtual(Impl &impl) : impl_(impl) {}

    void start(uint32_t num_threads, int priority, int stack_size) override { impl_.start(num_threads, priority, stack_size); }
    void stop() override { impl_.stop(); }
    int suspend() override { return impl_.suspend(); }
    int resume() override { return impl_.resume(); }
    bool isStarted() const override { return impl_.isStarted(); }
    bool isSuspended() const override { return impl_.isSuspended(); }
    uint32_t submit(std::function<void(void *)> task_function, void *task_argument, int priority) override {
        return impl_.submit(task_function, task_argument, priority);
    }
    void setPriority(int priority) override { impl_.setPriority(priority); }
    int getPriority() const override { return impl_.getPriority(); }
    size_t getTaskQueueSize() override { return impl_.getTaskQueueSize(); }
    uint32_t getActiveThreadCount() const override { return impl_.getActiveThreadCount(); }
    bool cancelTask(uint32_t task_id) override { return impl_.cancelTask(task_id); }
    bool cancelTask(std::function<void(void *)> &task_function) override { return impl_.cancelTask(task_function); }
    void setTaskFailureCallback(std::function<void(void *)> callback) override { impl_.setTaskFailureCallback(callback); }
    void setMaxThreads(uint32_t max_threads) override { impl_.setMaxThreads(max_threads); }
    uint32_t getMaxThreads() const override { return impl_.getMaxThreads(); }
    void setMinThreads(uint32_t min_threads) override { impl_.setMinThreads(min_threads); }
    uint32_t getMinThreads() const override { return impl_.getMinThreads(); }

private:
    Impl &impl_;
};
#endif

#if DP_OSAL_ENABLE_CHRONO
class IChrono {
public:
    using TimePoint = uint32_t;
    using Duration = double;

    virtual ~IChrono() = default;
    virtual TimePoint now() const = 0;
    virtual Duration elapsed(const TimePoint &start, const TimePoint &end) const = 0;
    virtual std::time_t to_time_t(const TimePoint &time_point) const = 0;
    virtual TimePoint from_time_t(std::time_t time) const = 0;
    virtual std::string to_string(const TimePoint &time_point) const = 0;
};

template <typename Impl>
class ChronoVirtual : public IChrono {
public:
    explicit ChronoVirtual(Impl &impl) : impl_(impl) {}

    TimePoint now() const override { return impl_.now(); }
    Duration elapsed(const TimePoint &start, const TimePoint &end) const override { return impl_.elapsed(start, end); }
    std::time_t to_time_t(const TimePoint &time_point) const override { return impl_.to_time_t(time_point); }
    TimePoint from_time_t(std::time_t time) const override { return impl_.from_time_t(time); }
    std::string to_string(const TimePoint &time_point) const override { return impl_.to_string(time_point); }

private:
    Impl &impl_;
};
#endif

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void StartScheduler() = 0;
    virtual void sleep_ms(uint32_t milliseconds) const = 0;
    virtual void sleep(uint32_t seconds) const = 0;
    virtual uint32_t get_tick_ms() const = 0;
    virtual void enter_critical() const = 0;
    virtual void exit_critical() const = 0;
    virtual const char *get_system_info() const = 0;
#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    virtual size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const = 0;
#endif
};

template <typename Impl>
class SystemVirtual : public ISystem {
public:
    explicit SystemVirtual(Impl &impl) : impl_(impl) {}

    void StartScheduler() override { impl_.StartScheduler(); }
    void sleep_ms(uint32_t milliseconds) const override { impl_.sleep_ms(milliseconds); }
    void sleep(uint32_t seconds) const override { impl_.sleep(seconds); }
    uint32_t get_tick_ms() const override { return impl_.get_tick_ms(); }
    void enter_critical() const override { impl_.enter_critical(); }
    void exit_critical() const override { impl_.exit_critical(); }
    const char *get_system_info() const override { return impl_.get_system_info(); }
#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const override {
        return impl_.get_thread_snapshot(buf, max);
    }
#endif

private:
    Impl &impl_;
};

#if DP_OSAL_ENABLE_MEMORY_MANAGER
class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;
    virtual bool initialize(size_t block_size, size_t block_count) = 0;
    virtual void *allocate(size_t size) = 0;
    virtual void deallocate(void *ptr) = 0;
    virtual void *reallocate(void *ptr, size_t new_size) = 0;
    virtual void *allocateAligned(size_t size, size_t alignment) = 0;
    virtual size_t getAllocatedSize() const = 0;
};

template <typename Impl>
class MemoryManagerVirtual : public IMemoryManager {
public:
    explicit MemoryManagerVirtual(Impl &impl) : impl_(impl) {}

    bool initialize(size_t block_size, size_t block_count) override { return impl_.initialize(block_size, block_count); }
    void *allocate(size_t size) override { return impl_.allocate(size); }
    void deallocate(void *ptr) override { impl_.deallocate(ptr); }
    void *reallocate(void *ptr, size_t new_size) override { return impl_.reallocate(ptr, new_size); }
    void *allocateAligned(size_t size, size_t alignment) override { return impl_.allocateAligned(size, alignment); }
    size_t getAllocatedSize() const override { return impl_.getAllocatedSize(); }

private:
    Impl &impl_;
};
#endif

} // namespace dp::osal

#endif  // DP_OSAL_VIRTUAL_H_
