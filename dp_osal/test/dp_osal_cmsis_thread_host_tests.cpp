/** @file dp_osal_cmsis_thread_host_tests.cpp
 * @brief 执行真实CMSIS Thread后端，受控内核提供完成/回收交错与资源观测。
 */
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <thread>
#include "cmsis_os2.h"

struct Sem {
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
};
struct Exit {};
struct Task {
    std::thread native;
    std::atomic<bool> parked{false};
};
namespace {
thread_local Task* current_task = nullptr;
std::atomic<bool> fail_create{false}, fail_delete{false}, fail_reap{false};
std::atomic<unsigned> sem_created{}, sem_deleted{}, task_created{}, task_reaped{};
std::atomic<bool> park_before_exit{}, exit_entered{}, allow_exit{};
Task* last_task = nullptr;
void* observed_cb = nullptr;
void* observed_stack = nullptr;
}  // namespace
osSemaphoreId_t osSemaphoreNew(uint32_t, uint32_t, const osSemaphoreAttr_t*) {
    ++sem_created;
    return new Sem;
}
osStatus_t osSemaphoreAcquire(osSemaphoreId_t sem, uint32_t ticks) {
    std::unique_lock<std::mutex> lock(sem->mutex);
    if (ticks == osWaitForever)
        sem->cv.wait(lock, [sem] { return sem->ready; });
    else if (!sem->cv.wait_for(lock, std::chrono::milliseconds(ticks), [sem] { return sem->ready; }))
        return osErrorTimeout;
    sem->ready = false;
    return osOK;
}
osStatus_t osSemaphoreRelease(osSemaphoreId_t sem) {
    std::lock_guard<std::mutex> lock(sem->mutex);
    sem->ready = true;
    sem->cv.notify_all();
    return osOK;
}
osStatus_t osSemaphoreDelete(osSemaphoreId_t sem) {
    if (fail_delete.exchange(false)) return 1;
    ++sem_deleted;
    delete sem;
    return osOK;
}
osThreadId_t osThreadNew(osThreadFunc_t entry, void* context, const osThreadAttr_t* attr) {
    observed_cb = attr->cb_mem;
    observed_stack = attr->stack_mem;
    if (fail_create.exchange(false)) return nullptr;
    auto* task = new Task;
    ++task_created;
    last_task = task;
    task->native = std::thread([=] {
        current_task = task;
        try {
            entry(context);
        } catch (const Exit&) {
        }
    });
    // 强制业务在osThreadNew返回、owner发布句柄之前执行自连接。
    if (std::strcmp(attr->name, "self-join") == 0)
        while (!task->parked.load()) std::this_thread::yield();
    return task;
}
osStatus_t osDelay(uint32_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
    return osOK;
}
osStatus_t osThreadSuspend(osThreadId_t) { return osOK; }
osStatus_t osThreadResume(osThreadId_t) { return osOK; }
osStatus_t osThreadSetPriority(osThreadId_t, osPriority_t) { return osOK; }
osPriority_t osThreadGetPriority(osThreadId_t) { return 0; }
osStatus_t osThreadJoin(osThreadId_t task) { return test_reap(task); }
[[noreturn]] void osThreadExit() { test_finish(); }
osStatus_t test_reap(osThreadId_t task) {
    if (fail_reap.exchange(false)) return 1;
    while (!task->parked.load()) std::this_thread::yield();
    task->native.join();
    ++task_reaped;
    delete task;
    return osOK;
}
uint32_t osKernelGetTickCount() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
uint32_t osKernelGetTickFreq() { return 1000U; }
osStatus_t test_reap_for(osThreadId_t task, uint32_t ticks) {
    const uint32_t start = osKernelGetTickCount();
    while (!task->parked.load()) {
        if (ticks != osWaitForever && osKernelGetTickCount() - start >= ticks) return osErrorTimeout;
        std::this_thread::yield();
    }
    return test_reap(task);
}
// pthread_exit与异常禁用/ASan组合不用于此fixture；用专用异常结束受控内核runner。
[[noreturn]] void test_finish() {
    if (park_before_exit.load()) {
        exit_entered = true;
        while (!allow_exit.load()) std::this_thread::yield();
    }
    current_task->parked = true;
    throw Exit{};
}

#include "dp_osal_thread.h"
int main(int argc, char** argv) {
    if (argc > 1) {
        std::set_terminate([] { std::_Exit(77); });
        dp::osal::Thread unjoined;
        assert(unjoined.start("unjoined", +[](void*) {}).ok());
        if (std::strcmp(argv[1], "cleanup-pending") == 0) {
            fail_delete = true;
            assert(unjoined.join().status == dp::osal::ThreadStatus::kNativeError);
        }
        return 0;  // 活句柄和清理待办两种析构违规均必须触发fatal。
    }
    using dp::osal::ThreadStatus;
    assert(dp::osal::detail::cmsisMillisecondsToTicks(100U, 128U) == 13U);
    assert(dp::osal::detail::cmsisMillisecondsToTicks(1U, 100U) == 1U);
    assert(dp::osal::detail::cmsisMillisecondsToTicks(0U, UINT32_MAX) == 0U);
    assert(dp::osal::detail::cmsisMillisecondsToTicks(UINT32_MAX, UINT32_MAX) > UINT32_MAX);
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
    static_assert(dp::osal::Thread::supportsTimedJoin());
#else
    static_assert(!dp::osal::Thread::supportsTimedJoin());
#endif
    unsigned calls = 0;
    auto entry = +[](void* context) { ++*static_cast<unsigned*>(context); };
    dp::osal::Thread thread;
    assert(!thread.joinable());
    assert(thread.requestStop());
    assert(!thread.requestStop());
    assert(thread.join().status == ThreadStatus::kInvalidState);
    fail_create = true;
    assert(thread.start("failed", entry, &calls).status == ThreadStatus::kResourceExhausted);
    assert(calls == 0 && sem_created == sem_deleted);
    assert(thread.start("ordinary", entry, &calls).ok());
    assert(thread.joinable());
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
    assert(thread.tryJoinFor(UINT32_MAX).status == ThreadStatus::kInvalidArgument);
#endif
    assert(thread.detach().status == ThreadStatus::kNotSupported);
    assert(thread.start("busy", entry, &calls).status == ThreadStatus::kBusy);
#ifndef DP_OSAL_PORT_THREAD_REAP_FOR
    assert(thread.tryJoinFor(1).status == ThreadStatus::kNotSupported);
#endif
    assert(thread.join().ok());
    assert(!thread.joinable() && !thread.isRunning());
    assert(thread.join().status == ThreadStatus::kInvalidState);
    assert(calls == 1);
    alignas(16) uint8_t stack[1024]{};
    alignas(16) uint8_t control[256]{};
    dp::osal::ThreadResources resources;
    resources.control_block = control;
    resources.control_block_size = sizeof(control);
    park_before_exit = true;
    assert(thread.start("static", entry, &calls, resources, 0, sizeof(stack), stack).ok());
    while (!exit_entered.load()) std::this_thread::yield();
    assert(!thread.isRunning() && thread.joinable());
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
    // 完成通知已消费但native退出尚未发生；超时保留进度，后续join不得再次acquire。
    assert(thread.tryJoinFor(1).status == ThreadStatus::kTimeout);
    assert(thread.joinable());
#endif
    std::atomic<bool> joined{};
    std::thread owner([&] {
        assert(thread.join().ok());
        joined = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!joined.load());
    allow_exit = true;
    owner.join();
    assert(calls == 2 && observed_cb == control && observed_stack == stack);
    assert(task_created == task_reaped && sem_created == sem_deleted);
    park_before_exit = false;
    // 原生reap失败后，完成通知已消费；重试不能再次等待token。
    assert(thread.start("reap-retry", entry, &calls).ok());
    fail_reap = true;
#ifdef DP_OSAL_PORT_THREAD_REAP_FOR
    assert(thread.tryJoinFor(100).status == ThreadStatus::kNativeError && thread.joinable());
#else
    assert(thread.join().status == ThreadStatus::kNativeError && thread.joinable());
#endif
    assert(thread.join().ok());
    // 原生任务已reap，完成资源删除失败：只重试清理，拒绝覆盖待办。
    assert(thread.start("completion-delete", entry, &calls).ok());
    fail_delete = true;
    assert(thread.join().status == ThreadStatus::kNativeError && !thread.joinable());
    assert(thread.start("busy-cleanup", entry, &calls).status == ThreadStatus::kBusy);
    assert(thread.tryJoinFor(0).ok());
    // 创建失败留下的清理待办同样必须显式回收。
    fail_create = true;
    fail_delete = true;
    assert(thread.start("creation-delete-failure", entry, &calls).status == ThreadStatus::kResourceExhausted);
    assert(thread.start("busy-cleanup", entry, &calls).status == ThreadStatus::kBusy);
    assert(thread.join().ok());
    // stop是请求+join；worker显式检查标志，不靠sleep异常结束。
    std::atomic<bool> entered{false};
    auto cooperative = [&](void*) {
        entered = true;
        while (!thread.stopRequested()) std::this_thread::yield();
    };
    assert(thread.start("cooperative", cooperative).ok());
    while (!entered.load()) std::this_thread::yield();
    assert(thread.requestStop());
    assert(!thread.requestStop());
    assert(thread.stop().ok());
    // 自连接必须立即拒绝，不能消费自己的完成通知。
    std::atomic<bool> self_checked{false};
    assert(thread
               .start("self-join",
                      [&](void*) {
                          assert(thread.join().status == ThreadStatus::kWouldDeadlock);
                          self_checked = true;
                      })
               .ok());
    assert(thread.join().ok() && self_checked.load());
    assert(task_created == task_reaped && sem_created == sem_deleted);
}
