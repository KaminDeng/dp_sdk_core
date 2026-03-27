//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_THREAD_H__
#define __OSAL_THREAD_H__

#include <atomic>
#include <functional>

#include "interface_thread.h"
#include "osal.h"
#include "osal_debug.h"
#include "osal_thread_stop.h"

namespace osal {
class OSALThread : public IThread {
public:
    OSALThread() : threadHandle(nullptr), running(false), suspended(false) {
        OSAL_LOGD("OSALThread default constructor called\n");
    }

    OSALThread(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
               int priority = OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr)
        : threadHandle(nullptr), running(false), suspended(false) {
        OSAL_LOGD("OSALThread parameterized constructor called\n");
        start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }

    ~OSALThread() override { stop(); }

    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument = nullptr,
              int priority = OSAL_PORT_THREAD_DEFAULT_PRIORITY, int stack_size = 0, void *pstack = nullptr) override {
        if (!isRunning()) {
            this->_taskFunction = taskFunction;
            this->_taskArgument = taskArgument;

            osThreadAttr_t attr = {};
            attr.name = name;
            attr.priority = (osPriority_t)priority;
            attr.stack_size = (stack_size > 0 && static_cast<uint32_t>(stack_size) > OSAL_PORT_THREAD_MIN_STACK_SIZE)
                                  ? static_cast<uint32_t>(stack_size)
                                  : OSAL_PORT_THREAD_MIN_STACK_SIZE;

            if (pstack != nullptr) {
                attr.stack_mem = pstack;
            }
            exitSemaphore = osSemaphoreNew(1, 0, nullptr);
            if (exitSemaphore == nullptr) {
                OSAL_LOGE("Failed to create semaphore\n");
                return -1;  // 失败
            }

            threadHandle = osThreadNew(reinterpret_cast<osThreadFunc_t>(taskRunner), this, &attr);
            if (threadHandle != nullptr) {
                running = true;
                OSAL_LOGD("Thread started successfully\n");
                return 0;  // 成功
            } else {
                OSAL_LOGE("Failed to create thread\n");
                osSemaphoreDelete(exitSemaphore);
                exitSemaphore = nullptr;
                return -1;  // 失败
            }
        }
        return -1;  // 线程已经在运行
    }

    void stop() override {
        if (threadHandle) {
            /* Set the cooperative stop flag so sleep_ms() exits its osDelay
             * loop within 1 ms (the chunk granularity), without using FreeRTOS
             * task notification flags (which interact with vTaskSuspend). */
            stop_requested_.store(true, std::memory_order_release);

            /* Wait for the thread to exit naturally (taskRunner releases the
             * exit semaphore before running=false and osThreadExit).  This
             * avoids pthread_cancel which — via glibc's SIGCANCEL with
             * SA_ONSTACK — triggers __asan_handle_no_return while on the
             * alternate signal stack, causing a GCC 11 ASan CHECK failure. */
            join();

            /* Clean up after join(). */
            threadHandle = nullptr;
            stop_requested_.store(false, std::memory_order_release);
            osSemaphoreId_t sem = exitSemaphore;
            exitSemaphore = nullptr;
            if (sem != nullptr) {
                osSemaphoreDelete(sem);
            }
            OSAL_LOGD("Thread stopped\n");
        }
    }

    int suspend() override {
        if (isRunning()) {
            osThreadSuspend(threadHandle);
            suspended = true;
            OSAL_LOGD("Thread suspended\n");
            return 0;  // 成功
        }
        return -1;  // 线程未运行
    }

    int resume() override {
        /* Do NOT check isRunning() here: 'running' may be false due to a race
         * between the thread completing its first osDelay(1) tick and this
         * resume() call — even though the FreeRTOS task is still alive.
         * Checking 'suspended' (set by suspend()) and 'threadHandle' (set by
         * start()) is sufficient to guard against spurious resume calls. */
        if (suspended && threadHandle) {
            osThreadResume(threadHandle);
            suspended = false;
            OSAL_LOGD("Thread resumed\n");
            return 0;  // 成功
        }
        return -1;  // 线程未挂起
    }

    void join() override {
        /* Capture exitSemaphore locally before checking isRunning().
         * stop() nulls exitSemaphore before deleting it, so if stop()
         * races with join(), the local copy is either valid (stop() not
         * yet called) or null (stop() already ran — nothing to wait for). */
        osSemaphoreId_t sem = exitSemaphore;
        if (isRunning() && sem != nullptr) {
            osSemaphoreAcquire(sem, osWaitForever);
            running = false;
            OSAL_LOGD("Thread joined\n");
        }
    }

    void detach() override {
        // Threre are inherently detached and will clean up after completion.
        OSAL_LOGD("Thread detached\n");
    }

    bool isRunning() const override { return running; }

    void setPriority(int priority) override {
        if (threadHandle) {
            osThreadSetPriority(threadHandle, (osPriority_t)priority);
            OSAL_LOGD("Thread priority set to %d\n", priority);
        }
    }

    int getPriority() const override {
        if (threadHandle) {
            osPriority_t priority = osThreadGetPriority(threadHandle);
            OSAL_LOGD("Thread priority retrieved: %d\n", priority);
            return (int)priority;
        }
        return -1;  // 返回一个无效的优先级表示错误
    }

private:
    static void taskRunner(void *parameters) {
        auto *thread = static_cast<OSALThread *>(parameters);
        /* Install this thread's stop flag as the thread-local pointer so that
         * sleep_ms() can detect stop() without using FreeRTOS task flags. */
        tl_cmsis_stop_flag = &thread->stop_requested_;
        if (thread->_taskFunction) {
            try {
                thread->_taskFunction(thread->_taskArgument);
            } catch (const OSALCmsisThreadStopException &) {
                /* stop() was requested: sleep_ms() threw to abort the task
                 * function before executing any code after the sleep call.
                 * Exit cleanly via the semaphore + osThreadExit path. */
                OSAL_LOGD("OSALThread: task aborted by stop()\n");
            }
        }
        /* Clear the stop-flag pointer BEFORE releasing the exit semaphore.
         * After osSemaphoreRelease, join() may return and stop() may destroy
         * the OSALThread object; accessing thread->* after that would be
         * use-after-free.  The join() caller is responsible for setting
         * running=false after acquiring the semaphore. */
        tl_cmsis_stop_flag = nullptr;
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
}  // namespace osal

#endif  // __OSAL_THREAD_H__