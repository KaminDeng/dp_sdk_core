/**
 * @file dp_osal_thread.h
 * @brief CMSIS-RTOS2线程后端，支持函数指针/callable入口及退出信号量连接。
 * @details 原生线程和退出信号量由内核分配；外部执行栈不等于全部控制资源静态分配。
 * stop发布协作请求、尽力通知后等待任务返回，不强制杀任意循环，也没有本层超时保证。
 * running保留到join清除，可能任务已退出而本地标记仍true；单一owner协调生命周期。
 * 本后端不支持finalizer，显式返回失败且不转移上下文。
 */
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_THREAD_H_
#define DP_OSAL_CMSIS_THREAD_H_

#include <atomic>
#include <cstdint>
#include <functional>

#include "interface_thread.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"
#include "dp_osal_thread_stop.h"
#if defined(DP_OSAL_PORT_FREERTOS_THREAD_LIFECYCLE)
#include "dp_osal_freertos_thread_port.h"
#endif

namespace dp::osal {
/**
 * @brief 持有CMSIS线程句柄、退出信号量及任务入口的CRTP后端。
 * @note 生命周期字段不构成并发join/stop协议，不允许靠局部句柄副本推断并发删除安全。
 */
class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
    static constexpr bool kSupportsFinalizer = false;
#ifdef DP_OSAL_PORT_THREAD_STATIC_RESOURCES_UNSUPPORTED
    static constexpr bool kSupportsControlBlockStorage = false;
#else
    static constexpr bool kSupportsControlBlockStorage = true;
#endif
#ifdef DP_OSAL_PORT_THREAD_DETACHED_UNSUPPORTED
    static constexpr bool kSupportsDetachedResources = false;
#else
    static constexpr bool kSupportsDetachedResources = true;
#endif
#if defined(DP_OSAL_PORT_FREERTOS_THREAD_LIFECYCLE)
    using ControlBlockStorage = StaticTask_t;
#endif
    /**
     * @brief 构造空线程句柄及未运行/未挂起状态，不创建内核任务。
     */
    Thread() : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread default constructor called\n");
    }

    /**
     * @brief 构造后立即调用start，构造函数不返回独立启动状态。
     * @param name 内核线程名，复制与存活期由端口规定。
     * @param taskFunction 按值传入的callable任务包装，可为空。
     * @param taskArgument 借用任务参数，覆盖任务最后访问期。
     * @param priority 转为osPriority_t的内核优先级编码。
     * @param stack_size 请求栈字节数，正数不足端口最小值时抬高。
     * @param pstack 可选外部栈，存储需覆盖实际大小/对齐及任务使用期。
     */
    Thread(const char* name, std::function<void(void*)> taskFunction, void* taskArgument = nullptr,
           int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void* pstack = nullptr)
        : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread parameterized constructor called\n");
        start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    /**
     * @brief 调用doStop等待回收仍保存的线程与退出信号量，任务不协作退出时可能阻塞。
     */
    ~Thread() { doStop(); }

private:
    /** @brief 统一保存任务、准备同步资源并创建内核任务；失败不接管上下文。 */
    int doStart(const char* name, ThreadTask task, void* context, int priority, int stack_size, void* stack,
                const ThreadResources& resources = {}) noexcept {
        if (!task.valid() || task.finalizer() != nullptr || isRunning()) return -1;
        if ((threadHandle != nullptr || exitSemaphore != nullptr) && doJoin() != 0) return -1;
        if (resources.control_block != nullptr && resources.control_block_size == 0U) return -1;
#ifdef DP_OSAL_PORT_THREAD_EXTERNAL_STACK_UNSUPPORTED
        if (stack != nullptr) return -1;
#endif
#ifdef DP_OSAL_PORT_THREAD_STATIC_RESOURCES_UNSUPPORTED
        if (resources.control_block != nullptr || resources.completion_control_block != nullptr) return -1;
#endif
#ifdef DP_OSAL_PORT_THREAD_DETACHED_UNSUPPORTED
        if (resources.completion == ThreadCompletion::kDetached) return -1;
#endif
        task_ = std::move(task);
        _taskArgument = context;
        stop_requested_.store(false, std::memory_order_release);
        detached_ = resources.completion == ThreadCompletion::kDetached;
        osThreadAttr_t attr{};
        attr.name = name;
        attr.priority = static_cast<osPriority_t>(priority);
        attr.stack_size = stack_size > 0 ? static_cast<uint32_t>(stack_size) : DP_OSAL_PORT_THREAD_MIN_STACK_SIZE;
        if (attr.stack_size < DP_OSAL_PORT_THREAD_MIN_STACK_SIZE) {
            if (stack != nullptr) {
                clearEntry();
                return -1;
            }
            attr.stack_size = DP_OSAL_PORT_THREAD_MIN_STACK_SIZE;
        }
        attr.stack_mem = stack;
        attr.cb_mem = resources.control_block;
        attr.cb_size = resources.control_block_size;
#ifdef DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES
        if (!DP_OSAL_PORT_THREAD_VALIDATE_RESOURCES(attr)) {
            clearEntry();
            return -1;
        }
#endif
        if (!detached_) {
#ifdef DP_OSAL_PORT_THREAD_JOINABLE_ATTR
            attr.attr_bits = DP_OSAL_PORT_THREAD_JOINABLE_ATTR;
#else
            attr.attr_bits = osThreadJoinable;
#endif
            osSemaphoreAttr_t completion_attr{};
            completion_attr.cb_mem = resources.completion_control_block;
            completion_attr.cb_size = resources.completion_control_block_size;
            exitSemaphore = osSemaphoreNew(1U, 0U, &completion_attr);
            if (exitSemaphore == nullptr) {
                clearEntry();
                return -1;
            }
        }
        // 所有runner借用字段均在创建前初始化；创建后仅owner句柄字段发生写入。
        running.store(true, std::memory_order_release);
        threadHandle = osThreadNew(taskRunner, this, &attr);
        if (threadHandle != nullptr) return 0;
        running.store(false, std::memory_order_release);
        (void)releaseCompletion();
        clearEntry();
        return -1;
    }

    /** @brief 请求协作停止；joinable完整回收，detached只等待最后一次对象访问。 */
    void doStop() {
        if (threadHandle == nullptr && exitSemaphore == nullptr) return;
        stop_requested_.store(true, std::memory_order_release);
        if (detached_) {
            while (running.load(std::memory_order_acquire)) osDelay(1U);
            threadHandle = nullptr;
            return;
        }
        (void)doJoin();
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

    /** @brief 删除失败保留完成资源；下一次join/stop/start可独立重试。 */
    int releaseCompletion() noexcept {
        if (exitSemaphore == nullptr) return 0;
        if (osSemaphoreDelete(exitSemaphore) != osOK) return -1;
        exitSemaphore = nullptr;
        return 0;
    }

    /** @brief 完成token仅表示最后对象访问；原生reap成功后才释放同步资源。 */
    int doJoin() {
        if (detached_) return -1;
        if (threadHandle != nullptr) {
            if (exitSemaphore == nullptr || osSemaphoreAcquire(exitSemaphore, osWaitForever) != osOK) return -1;
#ifdef DP_OSAL_PORT_THREAD_REAP
            const osStatus_t result = DP_OSAL_PORT_THREAD_REAP(threadHandle);
#else
            const osStatus_t result = osThreadJoin(threadHandle);
#endif
            if (result != osOK) {
                // 保留资源所有权并归还token，允许owner再次尝试reap。
                (void)osSemaphoreRelease(exitSemaphore);
                return -1;
            }
            threadHandle = nullptr;
            running.store(false, std::memory_order_release);
        }
        // 句柄已清空代表原生任务已回收，不能为重试completion再次acquire/reap。
        return releaseCompletion();
    }

    /** @brief 已创建任务不能中途转换资源所有权；创建时用ThreadResources显式选择。 */
    int doDetach() { return -1; }

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
     * @brief 设置每任务停止标志入口，执行统一可调用包装，清入口/TLS后释放退出信号量并osThreadExit。
     * @param parameters 有效Thread指针，必须存活至最后一次成员访问。
     * @details 只捕获启用异常构建中的内部停止异常；禁异常任务需自行返回。清停止标志指针后
     * 以退出信号量发布完成，发布后不再访问Thread成员，让owner可以回收对象。
     * @note 未捕获的其它异常不保证仍走正常清理，退出信号量发布不是owner已完成资源回收的证明。
     */
    static void taskRunner(void* parameters) {
        auto* thread = static_cast<Thread*>(parameters);
        osal_cmsis_stop_flag_set(&thread->stop_requested_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            thread->task_.run(thread->_taskArgument);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (const CmsisThreadStopException&) {
            DP_OSAL_LOGD("Thread stopped cooperatively\n");
        }
#endif
        const bool detached = thread->detached_;
        const osSemaphoreId_t completion = thread->exitSemaphore;
        thread->clearEntry();
        osal_cmsis_stop_flag_set(nullptr);
        if (detached) {
            thread->running.store(false, std::memory_order_release);  // 最后一次this访问。
            osThreadExit();
        }
        // 发布token以后只使用局部变量；port finish/reap负责内核结束边界。
        (void)osSemaphoreRelease(completion);
#ifdef DP_OSAL_PORT_THREAD_FINISH
        DP_OSAL_PORT_THREAD_FINISH();
#else
        osThreadExit();
#endif
    }

    void clearEntry() noexcept {
        task_.reset();
        _taskArgument = nullptr;
    }

    osThreadId_t threadHandle;    ///< 保存的内核线程句柄，doStop清空；普通join不清。
    ThreadTask task_;             ///< 所有启动形式共享的任务存储。
    void* _taskArgument{};        ///< 任务借用上下文，不由后端delete。
    std::atomic<bool> running;    ///< 启动后置true，join清false，不直接反映内核任务已经退出。
    std::atomic<bool> suspended;  ///< 本地挂起标志，OS操作返回状态未核实。
    std::atomic<bool> stop_requested_{false};  ///< 协作停止请求，由任务本地存储指针供sleep读取。
    bool detached_ = false;                    ///< 创建时选择的完成/回收模式。
    osSemaphoreId_t exitSemaphore{};  ///< 自持退出信号量，runner发布完成，join等待，doStop删除。
};
}  // namespace dp::osal

#endif  // DP_OSAL_CMSIS_THREAD_H_
