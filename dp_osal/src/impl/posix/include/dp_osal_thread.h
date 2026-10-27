/**
 * @file dp_osal_thread.h
 * @brief pthread线程后端，支持函数指针/callable入口及owned上下文最终清理。
 * @details start发布ready后runner才读任务字段；普通任务通过join/stop回收，owned路径先完成
 * 最后一次Thread访问再调用cleanup，因此cleanup可以删除内嵌当前Thread的上下文。
 * stop是协作请求并等待，未提供有界超时；禁异常任务必须自行返回，不能把此API当强制终止。
 * 生命周期由单一owner协调，不允许并发start/join/stop/detach或在自身任务内等待自身退出。
 */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_THREAD_H_
#define DP_OSAL_POSIX_THREAD_H_

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "interface_thread.h"
#include "dp_osal_port.h"  // provides DP_OSAL_PORT_THREAD_MIN_STACK_SIZE
#include "dp_osal_debug.h"

namespace dp::osal {

// Exception thrown by osal_sleep_ms_interruptible() when the owning Thread
// is stopped. Caught by taskRunner to cleanly abort the user function without
// using pthread_cancel (which triggers a GCC 11 ASan CHECK failure via
// AsanThread::Destroy → UnsetAlternateSignalStack → sigaltstack interceptor).
/**
 * @brief 启用异常时由可中断sleep抛出的内部停止异常，runner仅捕获此类型。
 */
struct OSALThreadStopException : public std::exception {
    /**
     * @brief 返回固定停止原因文本，不分配字符串。
     * @return 静态字面量指针，调用者不释放。
     */
    const char* what() const noexcept override { return "Thread: stop requested"; }
};

// Per-thread stop context: owned by Thread, pointed to by the thread-local
// tl_stop_ctx while taskRunner is executing.
/**
 * @brief 每个Thread自持的停止请求与可中断等待上下文，TLS只借用其地址。
 */
struct OSALThreadStopCtx {
    std::mutex mtx;                 ///< 保护停止等待与通知的条件变量配对。
    std::condition_variable cv;     ///< stop请求可唤醒当前线程的可中断sleep。
    std::atomic<bool> stop{false};  ///< 当前Thread的协作停止请求，不代表设备STOP授权。
};

// Thread-local pointer to the current thread's stop context.
// Set by taskRunner before calling the user function; cleared on exit.
// Used by osal_sleep_ms_interruptible() so System::sleep_ms() is interruptible.
/**
 * @brief 当前pthread正在执行的Thread停止上下文，runner进入时设置、退出前清空。
 */
inline thread_local OSALThreadStopCtx* tl_stop_ctx = nullptr;

// Interruptible sleep used by System::sleep_ms in the POSIX backend.
// - If called from inside an Thread and stop() is requested before the
//   timeout expires, throws OSALThreadStopException to abort the user function.
// - Falls back to plain sleep_for when not inside an Thread (e.g. main thread).
/**
 * @brief 有当前停止上下文时以条件变量等待毫秒，否则普通sleep_for。
 * @param ms 请求的相对毫秒数。
 * @details 停止谓词成立且启用异常时抛内部停止异常；禁异常时仅提前返回，用户循环仍须自行退出。
 * 不在本后端runner内时没有停止上下文，回退普通主机休眠。
 */
inline void osal_sleep_ms_interruptible(uint32_t ms) {
    OSALThreadStopCtx* ctx = tl_stop_ctx;
    if (ctx) {
        std::unique_lock<std::mutex> lk(ctx->mtx);
        bool stopped = ctx->cv.wait_for(lk, std::chrono::milliseconds(ms), [ctx] { return ctx->stop.load(); });
        if (stopped) {
#if defined(__cpp_exceptions)
            throw OSALThreadStopException{};
#else
            // 禁异常入口协作返回，调用方循环检查自身停止状态。
            return;
#endif
        }
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

/**
 * @brief 具体pthread线程对象及借用任务状态，CRTP基类访问私有do*入口。
 * @note running是本地观测，不是完整资源所有权；detach后任务仍可能继续访问对象。
 */
class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
    static constexpr bool kSupportsFinalizer = true;
    static constexpr bool kSupportsControlBlockStorage = false;
    static constexpr bool kSupportsDetachedResources = false;
    /**
     * @brief 构造无句柄、未运行且未挂起的对象，不创建pthread。
     */
    Thread() : threadHandle_(0), running(false), suspended(false), priority_(0) {
        DP_OSAL_LOGD("Thread default constructor called\n");
    }

    /**
     * @brief 构造后立即调用start，启动失败只经日志/状态体现，构造函数不返回错误码。
     * @param name 本后端不使用的线程名。
     * @param fn 任务包装，按值传递。
     * @param arg 借用参数。
     * @param priority 请求优先级。
     * @param stack_size 请求栈字节数。
     * @param pstack 可选外部栈，调用者保证大小/对齐/存活期。
     */
    Thread(const char* name, std::function<void(void*)> fn, void* arg = nullptr, int priority = 0, int stack_size = 0,
           void* pstack = nullptr)
        : threadHandle_(0), running(false), suspended(false), priority_(priority) {
        DP_OSAL_LOGD("Thread parameterized constructor called\n");
        start(name, fn, arg, priority, stack_size, pstack);
    }

    /**
     * @brief 调用doStop请求并等待仍持有句柄的任务结束，可能因任务不退出而长期阻塞。
     */
    ~Thread() { doStop(); }

private:
    /** @brief 保存唯一任务并共用原生创建；失败回滚，不调用最终清理。 */
    int doStart(const char* name, ThreadTask task, void* context, int priority, int stack_size, void* stack,
                const ThreadResources& resources = {}) noexcept {
        if (resources.control_block != nullptr || resources.completion_control_block != nullptr ||
            resources.completion != ThreadCompletion::kJoinable)
            return -1;
        if (isRunning() || !task.valid() || (task.finalizer() != nullptr && stack != nullptr)) return -1;
        doJoin();  // 已结束的旧pthread仍需回收，才能再次使用入口存储。
        auto_cleanup_ = task.finalizer() != nullptr;
        task_ = std::move(task);
        taskArgument = context;
        const int result = doStartNative(name, priority, stack_size, stack);
        // 成功发布ready之后可能已经执行finalizer，成功路径不能再访问this。
        if (result != 0) {
            clearEntry();
            auto_cleanup_ = false;
        }
        return result;
    }

    /**
     * @brief 初始化pthread属性、配置外部/系统栈并创建joinable线程，最后release发布ready。
     * @param name 当前明确忽略的名称。
     * @param priority 大于0时尝试设属性优先级；相关属性操作结果部分未检查。
     * @param stack_size 请求字节数，实际至少端口最小值。
     * @param pstack 非空时交给pthread_attr_setstack；为NULL且大小正时只设栈大小。
     * @return 属性初始化/栈设置/创建结果，0表示pthread_create成功。
     * @note 不设显式调度继承策略，不能仅凭参数断言实际实时优先级；pthread自身仍可分配控制资源。
     */
    int doStartNative(const char* name, int priority, int stack_size, void* pstack) noexcept {
        (void)name;
        int result = 0;

        // Reset ready_ so the new thread waits until the constructor/start() finishes.
        ready_.store(false, std::memory_order_relaxed);

        // Reset the stop context for a fresh start
        stopCtx_.stop.store(false);

        pthread_attr_t attr;
        result = pthread_attr_init(&attr);
        if (result != 0) {
            return result;
        }

        const size_t requested = stack_size > 0 ? static_cast<size_t>(stack_size) : 0U;
        const size_t minimum = static_cast<size_t>(DP_OSAL_PORT_THREAD_MIN_STACK_SIZE);
        const size_t actual = requested > minimum ? requested : minimum;
        if (pstack != nullptr) {
            result = pthread_attr_setstack(&attr, pstack, actual);
        } else if (stack_size > 0) {
            result = pthread_attr_setstacksize(&attr, actual);
        }
        if (result != 0) {
            pthread_attr_destroy(&attr);
            return result;
        }

        // 设置线程优先级
        if (priority > 0) {
            sched_param schedParam;
            pthread_attr_getschedparam(&attr, &schedParam);
            schedParam.sched_priority = priority;
            pthread_attr_setschedparam(&attr, &schedParam);
        }

        // 设置线程分离状态
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        pthread_t new_handle = 0;
        result = pthread_create(&new_handle, &attr, taskRunner, this);
        if (result == 0) {
            threadHandle_.store(new_handle, std::memory_order_relaxed);
            running = true;
            DP_OSAL_LOGD("Thread started successfully\n");
        } else {
            DP_OSAL_LOGE("Failed to create thread: %s\n", strerror(result));
        }

        pthread_attr_destroy(&attr);

        // Signal the new thread that all members are fully initialized.
        // Must be the last write in start() so the thread sees a consistent object.
        ready_.store(true, std::memory_order_release);
        return result;
    }

    /**
     * @brief 取走当前句柄，通知挂起等待与停止上下文，然后pthread_join等待任务自退。
     * @details 不使用pthread_cancel，不会强制打断任意阻塞I/O或无限循环；禁异常sleep只提前返回。
     * 挂起谓词只看suspended，通知不清此标志，故不能保证仍挂起任务能靠stop单独退出。
     * @note 无超时，join返回码被忽略；owner须避免自连接及不受支持的生命周期并发。
     */
    void doStop() {
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            // 1. Wake waitIfSuspended
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.notify_all();
            }

            // 2. 发布停止请求并唤醒可中断sleep；启用异常时抛内部停止异常，禁异常时只提前返回。
            // 用户仍须退出其任务循环，不使用pthread_cancel强制终止。
            {
                std::unique_lock<std::mutex> lk(stopCtx_.mtx);
                stopCtx_.stop.store(true);
                stopCtx_.cv.notify_all();
            }

            // 3. Wait for the thread to exit naturally — no pthread_cancel needed.
            pthread_join(h, nullptr);
            DP_OSAL_LOGD("Thread stopped and joined\n");
        }
    }

    /**
     * @brief 持锁设置协作挂起标志，不调用OS强制挂起任意已执行用户代码。
     * @return 固定0；真正等待发生在runner入口的waitIfSuspended检查点。
     */
    int doSuspend() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = true;
        DP_OSAL_LOGD("Thread suspended\n");
        return 0;  // 成功
    }

    /**
     * @brief 持锁清挂起标志并通知所有本地条件变量等待者。
     * @return 固定0，不证明任务已获调度。
     */
    int doResume() {
        std::unique_lock<std::mutex> lock(mutex_);
        suspended = false;
        cv_.notify_all();
        DP_OSAL_LOGD("Thread resumed\n");
        return 0;  // 成功
    }

    /**
     * @brief 取走保存句柄并等待对应pthread结束，已无句柄则不操作。
     * @note 不发送停止请求、无超时且返回pthread_join状态；任务须自然返回或已收到协作停止。
     */
    int doJoin() {
        if (auto_cleanup_) return -1;
        pthread_t h = threadHandle_.exchange(0);
        if (h) {
            const int result = pthread_join(h, nullptr);
            if (result != 0) return result;
            DP_OSAL_LOGD("Thread joined\n");
        }
        return 0;
    }

    /**
     * @brief 取走保存句柄并返回pthread_detach结果。
     * @note 不结束任务、不延长当前对象或上下文生命期，外部仍须保证它们不被过早销毁。
     */
    int doDetach() {
        pthread_t h = threadHandle_.exchange(0);
        return h ? pthread_detach(h) : 0;
    }

    /**
     * @brief 读取本地atomic运行标记。
     * @return 状态观测，结束与句柄回收是不同阶段，不作为对象可释放的证明。
     */
    bool doIsRunning() const { return running; }

    /**
     * @brief 始终缓存请求值，有句柄时尝试按SCHED_FIFO设置真实线程优先级。
     * @param priority 待缓存及提交的优先级。
     * @note OS调用结果被忽略，权限不足时日志/缓存值仍不代表实际设置成功。
     */
    void doSetPriority(int priority) {
        priority_ = priority;  // always store, so getPriority() works after thread ends
        pthread_t h = threadHandle_.load();
        if (h) {
            struct sched_param schedParam;
            schedParam.sched_priority = priority;
            // Note: SCHED_FIFO requires root on macOS/Linux. Failure is non-fatal.
            pthread_setschedparam(h, SCHED_FIFO, &schedParam);
            DP_OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    /**
     * @brief 返回缓存的请求优先级，不向OS读取实际调度优先级。
     * @return priority_，即使任务结束或上次OS设置失败也返回该值。
     */
    int doGetPriority() const {
        // Return the stored priority so callers get a consistent value
        // even when the thread has already finished (threadHandle == 0)
        // or when the OS call would fail (e.g. no root for SCHED_FIFO).
        DP_OSAL_LOGD("Thread priority retrieved: %d\n", priority_);
        return priority_;
    }

    /**
     * @brief 等ready发布后设置TLS，执行所选任务，清入口/TLS并在安全时调用owned cleanup。
     * @param parameters 有效且覆盖最后访问期的Thread指针。
     * @return 正常返回NULL，避免额外pthread_exit路径。
     * @details owned路径先缓存cleanup和context，清入口与TLS，取走句柄并写running=false作为最后this访问，
     * 随后只用局部变量detach和cleanup；cleanup可delete包含本对象的上下文。
     * @note 只捕获内部停止异常，不捕获任意任务异常，不能据此保证异常终止进程时仍完成清理。
     */
    static void* taskRunner(void* parameters) {
        auto* thread = static_cast<Thread*>(parameters);
        while (!thread->ready_.load(std::memory_order_acquire)) std::this_thread::yield();
        const ThreadCleanup cleanup = thread->task_.finalizer();
        void* const context = thread->taskArgument;
        tl_stop_ctx = &thread->stopCtx_;
        thread->waitIfSuspended();
#if defined(__cpp_exceptions)
        try {
#endif
            thread->task_.run(context);
#if defined(__cpp_exceptions)
        } catch (const OSALThreadStopException&) {
            DP_OSAL_LOGD("Thread stopped cooperatively\n");
        }
#endif
        thread->clearEntry();
        tl_stop_ctx = nullptr;
        if (cleanup != nullptr) {
            const pthread_t handle = thread->threadHandle_.exchange(0, std::memory_order_acq_rel);
            thread->running.store(false, std::memory_order_release);  // 最后一次this访问。
            if (handle != 0) (void)pthread_detach(handle);
            cleanup(context);
            return nullptr;
        }
        thread->running.store(false, std::memory_order_release);
        return nullptr;
    }

    /**
     * @brief 在suspended为true时循环等待本地条件变量。
     * @note 谓词不检查停止标志，必须由owner正确恢复/协调，通知本身不解除挂起。
     */
    void waitIfSuspended() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (suspended) {
            cv_.wait(lock);
        }
    }

    void clearEntry() noexcept {
        task_.reset();
        taskArgument = nullptr;
    }

    std::atomic<pthread_t> threadHandle_{0};
    ThreadTask task_;
    void* taskArgument = nullptr;
    std::atomic<bool> running;    ///< 本地运行观测标记，不负责底层句柄全部回收。
    std::atomic<bool> suspended;  ///< runner入口使用的协作挂起谓词。
    // Signals the thread that start() has finished writing all members.
    // Prevents TSAN data races when pthread_create() schedules the thread
    // before the constructor/start() returns.
    std::atomic<bool> ready_{false};  ///< start完成字段写入后的release/acquire发布屏障。
    bool auto_cleanup_ = false;       ///< 自动最终清理模式不支持普通join同步。
    int priority_;                    ///< 缓存请求优先级，不证明OS设置成功。
    std::mutex mutex_;                ///< 协作挂起/恢复的条件变量互斥。
    std::condition_variable cv_;      ///< runner等待resume的条件变量。
    OSALThreadStopCtx stopCtx_;       ///< 自持停止上下文，runner通过TLS临时借用。
};
}  // namespace dp::osal

#endif  // DP_OSAL_POSIX_THREAD_H_
