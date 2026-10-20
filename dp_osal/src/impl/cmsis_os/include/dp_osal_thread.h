/**
 * @file dp_osal_thread.h
 * @brief CMSIS-RTOS2线程后端，支持legacy/raw入口及退出信号量连接。
 * @details 原生线程和退出信号量由内核分配；外部执行栈不等于全部控制资源静态分配。
 * stop发布协作请求、尽力通知后等待任务返回，不强制杀任意循环，也没有本层超时保证。
 * running保留到join清除，可能任务已退出而本地标记仍true；单一owner协调生命周期。
 * 本后端未提供owned raw cleanup入口，使用该可选能力应由接口层明确编译拒绝。
 */
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_THREAD_H_
#define DP_OSAL_CMSIS_THREAD_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>

#include "interface_thread.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"
#include "dp_osal_thread_stop.h"

namespace dp::osal {
/**
 * @brief 持有CMSIS线程句柄、退出信号量及任务入口的CRTP后端。
 * @note 生命周期字段不构成并发join/stop协议，不允许靠局部句柄副本推断并发删除安全。
 */
class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
    /**
     * @brief 构造空线程句柄及未运行/未挂起状态，不创建内核任务。
     */
    Thread() : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread default constructor called\n");
    }

    /**
     * @brief 构造后立即调用legacy start，构造函数不返回独立启动状态。
     * @param name 内核线程名，复制与存活期由端口规定。
     * @param taskFunction 按值传入的legacy任务包装，可为空。
     * @param taskArgument 借用任务参数，覆盖任务最后访问期。
     * @param priority 转为osPriority_t的内核优先级编码。
     * @param stack_size 请求栈字节数，正数不足端口最小值时抬高。
     * @param pstack 可选外部栈，存储需覆盖实际大小/对齐及任务使用期。
     */
    Thread(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
               int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr)
        : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread parameterized constructor called\n");
        start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    /**
     * @brief 调用doStop等待回收仍保存的线程与退出信号量，任务不协作退出时可能阻塞。
     */
    ~Thread() { doStop(); }

private:
    /**
     * @brief 已运行时拒绝，否则保存legacy包装再创建原生任务，失败清入口。
     * @param name 内核线程名，复制与存活期由端口规定。
     * @param taskFunction 按值传入的legacy任务包装，可为空。
     * @param taskArgument 借用任务参数，覆盖任务最后访问期。
     * @param priority 转为osPriority_t的内核优先级编码。
     * @param stack_size 请求栈字节数，正数不足端口最小值时抬高。
     * @param pstack 可选外部栈，存储需覆盖实际大小/对齐及任务使用期。
     * @return 已运行/创建失败为-1，成功0；与POSIX legacy重复启动返回0不同。
     */
    int doStart(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
                int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr) {
        if (isRunning()) {
            return -1;
        }
        _taskFunction.emplace(std::move(taskFunction));
        _taskArgument = taskArgument;
        entryKind_ = EntryKind::kLegacy;
        const int result = doStartNative(name, priority, stack_size, pstack);
        if (result != 0) {
            clearEntry();
        }
        return result;
    }

    /**
     * @brief 验证未运行及非空函数指针，保存raw入口再创建内核任务。
     * @param name 内核线程名，复制与存活期由端口规定。
     * @param taskFunction 非空原始任务函数，不构造std::function。
     * @param taskArgument 借用任务参数，覆盖任务最后访问期。
     * @param priority 转为osPriority_t的内核优先级编码。
     * @param stack_size 请求栈字节数，正数不足端口最小值时抬高。
     * @param pstack 可选外部栈，存储需覆盖实际大小/对齐及任务使用期。
     * @return 运行中、空入口或创建失败为-1，成功0；失败不接管上下文释放。
     */
    int doStartRaw(const char *name, ThreadEntry taskFunction, void *taskArgument = nullptr,
                   int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr) {
        if (isRunning()) {
            return -1;
        }
        if (taskFunction == nullptr) {
            clearEntry();
            return -1;
        }
        _taskFunction.reset();
        rawTaskFunction_ = taskFunction;
        _taskArgument = taskArgument;
        entryKind_ = EntryKind::kRaw;
        const int result = doStartNative(name, priority, stack_size, pstack);
        if (result != 0) {
            clearEntry();
        }
        return result;
    }

    /**
     * @brief 按端口最小栈装配属性，先创建退出信号量再创建CMSIS线程。
     * @param name 内核线程名。
     * @param priority 转为osPriority_t的请求优先级。
     * @param stack_size 请求字节数，实际至少DP_OSAL_PORT_THREAD_MIN_STACK_SIZE。
     * @param pstack 可选调用者栈，非空时写入attr.stack_mem。
     * @return 线程与信号量创建成功0，否则-1；任务创建失败会删除本次信号量。
     * @note 不提供就地owned清理，不验证外部栈对齐或实际缓冲长度，内核控制资源仍可动态分配。
     */
    int doStartNative(const char *name, int priority, int stack_size, void *pstack) {
        osThreadAttr_t attr = {};
        attr.name = name;
        attr.priority = (osPriority_t)priority;
        attr.stack_size = (stack_size > 0 && static_cast<uint32_t>(stack_size) > DP_OSAL_PORT_THREAD_MIN_STACK_SIZE)
                              ? static_cast<uint32_t>(stack_size)
                              : DP_OSAL_PORT_THREAD_MIN_STACK_SIZE;

        if (pstack != nullptr) {
            attr.stack_mem = pstack;
        }
        exitSemaphore = osSemaphoreNew(1, 0, nullptr);
        if (exitSemaphore == nullptr) {
            DP_OSAL_LOGE("Failed to create semaphore\n");
            return -1;  // 失败
        }

        threadHandle = osThreadNew(taskRunner, this, &attr);
        if (threadHandle != nullptr) {
            running = true;
            DP_OSAL_LOGD("Thread started successfully\n");
            return 0;  // 成功
        }

        DP_OSAL_LOGE("Failed to create thread\n");
        osSemaphoreDelete(exitSemaphore);
        exitSemaphore = nullptr;
        return -1;  // 失败
    }

    /**
     * @brief 有句柄时发布停止标志，条件允许则发送线程flag，然后join并清理句柄/退出信号量。
     * @details 实际停止依赖任务返回或支持的sleep停止路径；此函数不保证1ms/100ms延迟，也不强制终止。
     * @note 不与其它join/stop并发，不在当前任务内等待自身；CMSIS删除/通知状态被忽略。
     */
    void doStop() {
        if (threadHandle) {
            /* 发布协作停止请求；具体sleep分片与检查由System/端口实现，不能在此承诺1ms响应。 */
            stop_requested_.store(true, std::memory_order_release);

            /* 端口配置允许时尽力发送0x1线程flag，返回码不检查；这本身不证明正在osDelay的任务
             * 已立即醒来。停止仍依赖任务的协作检查和退出。 */
#ifdef configUSE_TASK_NOTIFICATIONS
            if (configUSE_TASK_NOTIFICATIONS && threadHandle) {
                osThreadFlagsSet(threadHandle, 0x1U);
            }
#endif

            /* Wait for the thread to exit naturally (taskRunner releases the
             * exit semaphore before running=false and osThreadExit).  This
             * avoids pthread_cancel which — via glibc's SIGCANCEL with
             * SA_ONSTACK — triggers __asan_handle_no_return while on the
             * alternate signal stack, causing a GCC 11 ASan CHECK failure. */
            doJoin();

            /* Clean up after join(). */
            threadHandle = nullptr;
            stop_requested_.store(false, std::memory_order_release);
            osSemaphoreId_t sem = exitSemaphore;
            exitSemaphore = nullptr;
            if (sem != nullptr) {
                osSemaphoreDelete(sem);
            }
            DP_OSAL_LOGD("Thread stopped\n");
        }
    }

    /**
     * @brief 本地running为true时调用osThreadSuspend并置suspended。
     * @return 进入此分支返回0，否则-1；内核挂起返回码被忽略，0不证明内核操作成功。
     */
    int doSuspend() {
        if (isRunning()) {
            osThreadSuspend(threadHandle);
            suspended = true;
            DP_OSAL_LOGD("Thread suspended\n");
            return 0;  // 成功
        }
        return -1;  // 线程未运行
    }

    /**
     * @brief suspended且句柄非空时请求osThreadResume并清本地标志。
     * @return 进入分支0，否则-1；内核返回码未检查，不代表任务已经运行。
     */
    int doResume() {
        /* Do NOT check isRunning() here: 'running' may be false due to a race
         * between the thread completing its first osDelay(1) tick and this
         * resume() call — even though the FreeRTOS task is still alive.
         * Checking 'suspended' (set by suspend()) and 'threadHandle' (set by
         * start()) is sufficient to guard against spurious resume calls. */
        if (suspended && threadHandle) {
            osThreadResume(threadHandle);
            suspended = false;
            DP_OSAL_LOGD("Thread resumed\n");
            return 0;  // 成功
        }
        return -1;  // 线程未挂起
    }

    /**
     * @brief 缓存退出信号量，本地running且信号量非空时无限等待一次退出通知并清running。
     * @note osSemaphoreAcquire结果被忽略；此处不清线程句柄或删除信号量，后续doStop负责回收。
     * 局部副本不是并发生命周期保证，owner必须避免同时删除该信号量。
     */
    void doJoin() {
        /* 缓存本次要等待的信号量；这不是与并发stop删除的同步协议，生命周期仍由单owner协调。 */
        osSemaphoreId_t sem = exitSemaphore;
        if (isRunning() && sem != nullptr) {
            osSemaphoreAcquire(sem, osWaitForever);
            running = false;
            DP_OSAL_LOGD("Thread joined\n");
        }
    }

    /**
     * @brief 仅输出调试文本，不修改线程句柄或资源状态。
     * @note CMSIS任务返回走自身退出路径，但调用者仍需按对象生命周期清理保存的信号量等资源。
     */
    void doDetach() {
        // Threre are inherently detached and will clean up after completion.
        DP_OSAL_LOGD("Thread detached\n");
    }

    /**
     * @brief 读取本地running标记，任务返回时runner不清此标志。
     * @return 可能包含已退出但尚未join的任务，不是osThreadGetState的实时结果。
     */
    bool doIsRunning() const { return running; }

    /**
     * @brief 有句柄时请求内核修改优先级，返回状态被忽略。
     * @param priority 转为osPriority_t的请求值，日志不证明设置成功。
     */
    void doSetPriority(int priority) {
        if (threadHandle) {
            osThreadSetPriority(threadHandle, (osPriority_t)priority);
            DP_OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    /**
     * @brief 有句柄时读取内核优先级，无句柄时返回-1。
     * @return osThreadGetPriority转为int的值，内核错误码也沿此返回。
     */
    int doGetPriority() const {
        if (threadHandle) {
            osPriority_t priority = osThreadGetPriority(threadHandle);
            DP_OSAL_LOGD("Thread priority retrieved: %d\n", priority);
            return (int)priority;
        }
        return -1;  // 返回一个无效的优先级表示错误
    }

    /**
     * @brief 设置每任务停止标志入口，执行raw/legacy任务，清入口/TLS后释放退出信号量并osThreadExit。
     * @param parameters 有效Thread指针，必须存活至最后一次成员访问。
     * @details 只捕获启用异常构建中的内部停止异常；禁异常任务需自行返回。清停止标志指针后
     * 以退出信号量发布完成，发布后不再访问Thread成员，让owner可以回收对象。
     * @note 未捕获的其它异常不保证仍走正常清理，退出信号量发布不是owner已完成资源回收的证明。
     */
    static void taskRunner(void *parameters) {
        auto *thread = static_cast<Thread *>(parameters);
        /* Install this thread's stop flag as the thread-local pointer so that
         * sleep_ms() can detect stop() without using FreeRTOS task flags. */
        osal_cmsis_stop_flag_set(&thread->stop_requested_);
        if (thread->entryKind_ != EntryKind::kNone) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                if (thread->entryKind_ == EntryKind::kRaw) {
                    ThreadEntry entry = thread->rawTaskFunction_;
                    if (entry != nullptr) {
                        entry(thread->_taskArgument);
                    }
                } else if (thread->_taskFunction.has_value() && static_cast<bool>(*thread->_taskFunction)) {
                    (*thread->_taskFunction)(thread->_taskArgument);
                }
            } catch (const CmsisThreadStopException &) {
                /* stop() was requested: sleep_ms() threw to abort the task
                 * function before executing any code after the sleep call.
                 * Exit cleanly via the semaphore + osThreadExit path. */
                DP_OSAL_LOGD("Thread: task aborted by stop()\n");
            }
#else
            /* Bare-metal / -fno-exceptions: run function directly.
             * Cooperative stop via exception is unavailable; the task must
             * poll for the stop flag or return voluntarily. */
            if (thread->entryKind_ == EntryKind::kRaw) {
                ThreadEntry entry = thread->rawTaskFunction_;
                if (entry != nullptr) {
                    entry(thread->_taskArgument);
                }
            } else if (thread->_taskFunction.has_value() && static_cast<bool>(*thread->_taskFunction)) {
                (*thread->_taskFunction)(thread->_taskArgument);
            }
#endif
        }
        thread->clearEntry();
        /* Clear the stop-flag pointer BEFORE releasing the exit semaphore.
         * After osSemaphoreRelease, join() may return and stop() may destroy
         * the Thread object; accessing thread->* after that would be
         * use-after-free.  The join() caller is responsible for setting
         * running=false after acquiring the semaphore. */
        osal_cmsis_stop_flag_set(nullptr);
        osSemaphoreRelease(thread->exitSemaphore);
        /* Do NOT access thread->* after osSemaphoreRelease — the object may
         * have been destroyed by the time execution reaches here. */
        osThreadExit();
    }

    /**
     * @brief 标记无入口、legacy包装或raw指针，runner据此选择执行分支。
     */
    enum class EntryKind : uint8_t {
        kNone,   ///< 无任务入口。
        kLegacy, ///< optional<std::function>包装入口。
        kRaw,    ///< 原始函数指针入口。
    };

    /**
     * @brief 清空任务包装、raw指针、借用参数及入口类别，不删除外部上下文或内核资源。
     */
    void clearEntry() {
        _taskFunction.reset();
        rawTaskFunction_ = nullptr;
        _taskArgument = nullptr;
        entryKind_ = EntryKind::kNone;
    }

    osThreadId_t threadHandle; ///< 保存的内核线程句柄，doStop清空；普通join不清。
    std::optional<std::function<void(void *)>> _taskFunction; ///< legacy包装，raw路径不构造该可调用包装。
    ThreadEntry rawTaskFunction_ = nullptr; ///< raw任务入口，参数由_taskArgument借用。
    void *_taskArgument{}; ///< 任务借用上下文，不由后端delete。
    EntryKind entryKind_ = EntryKind::kNone; ///< runner派发入口类别。
    std::atomic<bool> running; ///< 启动后置true，join清false，不直接反映内核任务已经退出。
    std::atomic<bool> suspended; ///< 本地挂起标志，OS操作返回状态未核实。
    std::atomic<bool> stop_requested_{false}; ///< 协作停止请求，由任务本地存储指针供sleep读取。
    osSemaphoreId_t exitSemaphore{}; ///< 自持退出信号量，runner发布完成，join等待，doStop删除。
};
} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_THREAD_H_
