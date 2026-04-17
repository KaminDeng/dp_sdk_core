//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_THREAD_H_
#define DP_OSAL_CMSIS_THREAD_H_

#include <atomic>
#include <functional>

#include "interface_thread.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"
#include "dp_osal_thread_stop.h"

namespace dp::osal {
class Thread : public ThreadBase<Thread> {
    friend class ThreadBase<Thread>;

public:
    Thread() : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread default constructor called\n");
    }

    Thread(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
               int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr)
        : threadHandle(nullptr), running(false), suspended(false) {
        DP_OSAL_LOGD("Thread parameterized constructor called\n");
        start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    ~Thread() { doStop(); }

private:
    int doStart(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
                int priority = DP_OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr) {
        if (!isRunning()) {
            this->_taskFunction = taskFunction;
            this->_taskArgument = taskArgument;

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

            threadHandle = osThreadNew(reinterpret_cast<osThreadFunc_t>(taskRunner), this, &attr);
            if (threadHandle != nullptr) {
                running = true;
                DP_OSAL_LOGD("Thread started successfully\n");
                return 0;  // 成功
            } else {
                DP_OSAL_LOGE("Failed to create thread\n");
                osSemaphoreDelete(exitSemaphore);
                exitSemaphore = nullptr;
                return -1;  // 失败
            }
        }
        return -1;  // 线程已经在运行
    }

    void doStop() {
        if (threadHandle) {
            /* Set the cooperative stop flag so sleep_ms() exits its osDelay
             * loop within 1 ms (the chunk granularity), without using FreeRTOS
             * task notification flags (which interact with vTaskSuspend). */
            stop_requested_.store(true, std::memory_order_release);

            /* On FreeRTOS, wake the thread from osDelay immediately via task
             * notification flag 0x1.  On RT-Thread/Zephyr where task flags are
             * a stub, stop() still sets the atomic flag; the sleeping thread
             * will detect it at the next SLEEP_CHUNK_MS boundary (≤100 ms). */
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

    int doSuspend() {
        if (isRunning()) {
            osThreadSuspend(threadHandle);
            suspended = true;
            DP_OSAL_LOGD("Thread suspended\n");
            return 0;  // 成功
        }
        return -1;  // 线程未运行
    }

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

    void doJoin() {
        /* Capture exitSemaphore locally before checking isRunning().
         * stop() nulls exitSemaphore before deleting it, so if stop()
         * races with join(), the local copy is either valid (stop() not
         * yet called) or null (stop() already ran — nothing to wait for). */
        osSemaphoreId_t sem = exitSemaphore;
        if (isRunning() && sem != nullptr) {
            osSemaphoreAcquire(sem, osWaitForever);
            running = false;
            DP_OSAL_LOGD("Thread joined\n");
        }
    }

    void doDetach() {
        // Threre are inherently detached and will clean up after completion.
        DP_OSAL_LOGD("Thread detached\n");
    }

    bool doIsRunning() const { return running; }

    void doSetPriority(int priority) {
        if (threadHandle) {
            osThreadSetPriority(threadHandle, (osPriority_t)priority);
            DP_OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    int doGetPriority() const {
        if (threadHandle) {
            osPriority_t priority = osThreadGetPriority(threadHandle);
            DP_OSAL_LOGD("Thread priority retrieved: %d\n", priority);
            return (int)priority;
        }
        return -1;  // 返回一个无效的优先级表示错误
    }

    static void taskRunner(void *parameters) {
        auto *thread = static_cast<Thread *>(parameters);
        /* Install this thread's stop flag as the thread-local pointer so that
         * sleep_ms() can detect stop() without using FreeRTOS task flags. */
        osal_cmsis_stop_flag_set(&thread->stop_requested_);
        if (thread->_taskFunction) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                thread->_taskFunction(thread->_taskArgument);
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
            thread->_taskFunction(thread->_taskArgument);
#endif
        }
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

    osThreadId_t threadHandle;
    std::function<void(void *)> _taskFunction;
    void *_taskArgument{};
    std::atomic<bool> running;
    std::atomic<bool> suspended;
    std::atomic<bool> stop_requested_{false}; /* cooperative stop flag */
    osSemaphoreId_t exitSemaphore{};
};
} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_THREAD_H_
