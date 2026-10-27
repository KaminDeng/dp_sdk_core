/** @file dp_osal_cmsis_thread_host_tests.cpp
 * @brief 执行真实CMSIS Thread后端，受控内核提供完成/回收交错与资源观测。
 */
#include <atomic>
#include <cassert>
#include <condition_variable>
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
std::atomic<bool> fail_create{false}, fail_delete{false};
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
osStatus_t osSemaphoreAcquire(osSemaphoreId_t sem, uint32_t) {
    std::unique_lock<std::mutex> lock(sem->mutex);
    sem->cv.wait(lock, [sem] { return sem->ready; });
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
    while (!task->parked.load()) std::this_thread::yield();
    task->native.join();
    ++task_reaped;
    delete task;
    return osOK;
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
int main() {
    static_assert(!dp::osal::Thread::supportsFinalizer());
    unsigned calls = 0;
    auto entry = +[](void* context) { ++*static_cast<unsigned*>(context); };
    dp::osal::Thread thread;
    assert(thread.start("unsupported", entry, &calls, dp::osal::ThreadFinalizer{+[](void*) noexcept {}}) != 0);
    fail_create = true;
    assert(thread.start("failed", entry, &calls) != 0);
    assert(calls == 0 && sem_created == sem_deleted);
    assert(thread.start("ordinary", entry, &calls) == 0);
    assert(thread.join() == 0);
    assert(thread.join() == 0);
    assert(calls == 1);
    alignas(16) uint8_t stack[1024]{};
    alignas(16) uint8_t control[256]{};
    // CMSIS允许只借用栈；需要TCB成对的内核约束由具体port验证。
    assert(thread.start("borrowed-stack", entry, &calls, 0, sizeof(stack), stack) == 0);
    assert(thread.join() == 0);
    assert(calls == 2);
    dp::osal::ThreadResources resources;
    resources.control_block = control;
    resources.control_block_size = sizeof(control);
    park_before_exit = true;
    assert(thread.start("static", entry, &calls, resources, 0, sizeof(stack), stack) == 0);
    while (!exit_entered.load()) std::this_thread::yield();
    std::atomic<bool> joined{};
    std::thread owner([&] {
        assert(thread.join() == 0);
        joined = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!joined.load());
    allow_exit = true;
    owner.join();
    assert(calls == 3);
    assert(observed_cb == control && observed_stack == stack);
    assert(task_created == task_reaped && sem_created == sem_deleted);
    const unsigned completion_count = sem_created.load();
    resources.completion = dp::osal::ThreadCompletion::kDetached;
    assert(thread.start("detached", entry, &calls, resources, 0, sizeof(stack), stack) == 0);
    assert(thread.join() != 0);
    while (thread.isRunning()) std::this_thread::yield();
    assert(test_reap(last_task) == osOK);
    assert(calls == 4 && sem_created == completion_count);
    assert(task_created == task_reaped && sem_created == sem_deleted);
    // 原生任务已reap但完成信号量删除失败：再次join仅重试同步资源，不能重复reap。
    for (unsigned recovery = 0U; recovery < 4U; ++recovery) {
        const unsigned before_reaped = task_reaped.load();
        {
            dp::osal::Thread retry;
            assert(retry.start("completion-delete", entry, &calls) == 0);
            fail_delete = true;
            assert(retry.join() != 0);
            assert(task_reaped == before_reaped + 1U && sem_created == sem_deleted + 1U);
            if (recovery == 0U) {
                assert(retry.join() == 0);
                assert(task_reaped == before_reaped + 1U && sem_created == sem_deleted);
            } else if (recovery == 1U) {
                fail_delete = true;
                const unsigned before_created = task_created.load();
                assert(retry.start("cleanup-still-fails", entry, &calls) != 0);
                assert(task_created == before_created);  // 不覆盖未释放的旧completion。
                assert(retry.start("after-delete-retry", entry, &calls) == 0);
                assert(retry.join() == 0);
            } else if (recovery == 2U) {
                retry.stop();
                assert(sem_created == sem_deleted);
            }  // recovery=3交析构重试。
        }
        assert(task_created == task_reaped && sem_created == sem_deleted);
    }
    // 创建任务失败时的信号量删除也可能失败；所有权仍保留供下一次join/启动回收。
    {
        dp::osal::Thread retry;
        fail_create = true;
        fail_delete = true;
        assert(retry.start("creation-delete-failure", entry, &calls) != 0);
        assert(sem_created == sem_deleted + 1U);
        assert(retry.join() == 0);
        assert(sem_created == sem_deleted);
        assert(retry.start("creation-recovered", entry, &calls) == 0);
        assert(retry.join() == 0);
    }
    assert(task_created == task_reaped && sem_created == sem_deleted);
}
