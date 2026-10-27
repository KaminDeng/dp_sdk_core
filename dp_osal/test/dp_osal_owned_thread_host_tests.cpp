/**
 * @file dp_osal_owned_thread_host_tests.cpp
 * @brief 真实 pthread 后端的显式拥有者生命周期回归。
 * @details 旧目标名暂由构建入口沿用；不再存在后台最终清理模式。
 */
#include "dp_osal_thread.h"
#if DP_OSAL_ENABLE_THREAD_POOL
#include "dp_osal_thread_pool.h"
#endif
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <pthread.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using dp::osal::Thread;
using dp::osal::ThreadStatus;
std::atomic<bool> fail_creation{false};
std::atomic<int> join_error{0};
std::atomic<int> timed_join_error{0};
struct Work {
    Thread* thread = nullptr;
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<int> self_join{0};
};
void blocked(void* context) {
    auto& work = *static_cast<Work*>(context);
    work.entered.store(true, std::memory_order_release);
    while (!work.release.load(std::memory_order_acquire)) std::this_thread::yield();
}
void cooperative(void* context) {
    auto& work = *static_cast<Work*>(context);
    work.entered.store(true, std::memory_order_release);
    while (!work.thread->stopRequested()) dp::osal::osal_sleep_ms_interruptible(5000U);
    work.release.store(true, std::memory_order_release);
}
void immediate(void* context) {
    if (context != nullptr) static_cast<std::atomic<unsigned>*>(context)->fetch_add(1U);
}
void selfJoin(void* context) {
    auto& work = *static_cast<Work*>(context);
    const auto result = work.thread->join();
    work.self_join.store(result.status == ThreadStatus::kWouldDeadlock ? 1 : -1);
}
void awaitEntry(Work& work) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!work.entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    assert(work.entered.load());
}
}  // namespace
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* handle, const pthread_attr_t* attr, void* (*run)(void*), void* arg) {
    return fail_creation.load() ? EAGAIN : __real_pthread_create(handle, attr, run, arg);
}
extern "C" int __real_pthread_join(pthread_t, void**);
extern "C" int __wrap_pthread_join(pthread_t handle, void** result) {
    const int error = join_error.exchange(0);
    return error != 0 ? error : __real_pthread_join(handle, result);
}
#if defined(__linux__) && defined(__GLIBC__) && defined(__USE_GNU)
#if __GLIBC_PREREQ(2, 31)
extern "C" int __real_pthread_clockjoin_np(pthread_t, void**, clockid_t, const timespec*);
extern "C" int __wrap_pthread_clockjoin_np(pthread_t handle, void** result, clockid_t clock, const timespec* deadline) {
    assert(clock == CLOCK_MONOTONIC);
    const int error = timed_join_error.exchange(0);
    return error != 0 ? error : __real_pthread_clockjoin_np(handle, result, clock, deadline);
}
#endif
#endif
int main() {
    Thread thread;
    assert(!thread.joinable());
    assert(thread.join().status == ThreadStatus::kInvalidState);
    assert(thread.tryJoinFor(0).status == ThreadStatus::kInvalidState);
    assert(thread.requestStop());
    assert(!thread.requestStop());
    const auto invalid = thread.start("invalid", nullptr, nullptr);
    assert(invalid.status == ThreadStatus::kInvalidArgument && !thread.joinable());
    std::atomic<unsigned> calls{0U};
    fail_creation = true;
    const auto failed = thread.start("failed", immediate, &calls);
    fail_creation = false;
    assert(failed.status == ThreadStatus::kResourceExhausted && failed.native_error == EAGAIN);
    assert(!thread.joinable() && calls.load() == 0U);
    // 创建失败必须释放被入口包装捕获的资源，且允许显式重试。
    auto capture = std::make_shared<unsigned>(3U);
    std::weak_ptr<unsigned> weak = capture;
    fail_creation = true;
    const auto capture_failed = thread.start("capture-fail", [capture](void*) {}, nullptr);
    capture.reset();
    fail_creation = false;
    assert(!capture_failed.ok() && weak.expired());

    for (unsigned index = 0; index < 300U; ++index) {
        assert(thread.start("quick", immediate, &calls).ok());
        while (thread.isRunning()) std::this_thread::yield();
        assert(thread.joinable());
        assert(thread.start("not-reaped", immediate, &calls).status == ThreadStatus::kBusy);
        assert(thread.join().ok());
        assert(!thread.joinable());
    }
    assert(calls.load() == 300U);
    // 外部栈只有真实 join 后才能复用；不能把小栈默默上调为越界的有效容量。
    void* stack = nullptr;
    constexpr int stack_bytes = 1024 * 1024;
    assert(posix_memalign(&stack, static_cast<size_t>(sysconf(_SC_PAGESIZE)), stack_bytes) == 0);
    const auto small_stack = thread.start("small-stack", immediate, nullptr, 0, 1, stack);
    assert(small_stack.status == ThreadStatus::kInvalidArgument && !thread.joinable());
    for (unsigned index = 0; index < 3U; ++index) {
        assert(thread.start("external-stack", immediate, nullptr, 0, stack_bytes, stack).ok());
        assert(thread.join().ok());
    }
    free(stack);
    Work work;
    work.thread = &thread;
    assert(thread.start("blocking", blocked, &work).ok());
    awaitEntry(work);
    assert(thread.start("busy", immediate, nullptr).status == ThreadStatus::kBusy);
    assert(thread.detach().status == ThreadStatus::kNotSupported && thread.joinable());
    if (Thread::supportsTimedJoin()) {
        const auto before = std::chrono::steady_clock::now();
        assert(thread.tryJoinFor(10).status == ThreadStatus::kTimeout);
        assert(thread.tryJoinFor(0).status == ThreadStatus::kTimeout);
        assert(std::chrono::steady_clock::now() - before < std::chrono::seconds(1));
        assert(thread.joinable());
        timed_join_error = EIO;
        const auto timed_failed = thread.tryJoinFor(1);
        assert(timed_failed.status == ThreadStatus::kNativeError && timed_failed.native_error == EIO);
        assert(thread.joinable());
    } else {
        assert(thread.tryJoinFor(1).status == ThreadStatus::kNotSupported);
    }
    join_error = EIO;
    const auto join_failed = thread.join();
    assert(join_failed.status == ThreadStatus::kNativeError && join_failed.native_error == EIO);
    assert(thread.joinable());
    work.release = true;
    assert(thread.join().ok() && !thread.joinable());

    Work self;
    self.thread = &thread;
    assert(thread.start("self-join", selfJoin, &self).ok());
    assert(thread.join().ok());
    assert(self.self_join.load() == 1);
    // requestStop 唤醒可中断 sleep，用户自行返回，不靠异常退出。
    Work stop;
    stop.thread = &thread;
    assert(thread.start("cooperative", cooperative, &stop).ok());
    awaitEntry(stop);
    const auto before_stop = std::chrono::steady_clock::now();
    assert(thread.requestStop());
    assert(!thread.requestStop());
    assert(thread.stopRequested());
    assert(thread.join().ok());
    assert(stop.release.load() && std::chrono::steady_clock::now() - before_stop < std::chrono::seconds(1));
    assert(thread.start("reset-stop", immediate, nullptr).ok());
    assert(!thread.stopRequested());
    if (Thread::supportsTimedJoin())
        assert(thread.tryJoinFor(2000).ok());
    else
        assert(thread.join().ok());

    // 析构不能静默 join、detach 或遗失回收责任。
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        std::set_terminate([] { _Exit(77); });
        {
            Thread unreaped;
            if (!unreaped.start("unreaped", immediate, nullptr).ok()) _Exit(78);
        }
        _Exit(79);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 77);
#if DP_OSAL_ENABLE_THREAD_POOL
    // 聚合拥有者在 join 原生失败时也必须保留 worker，而非清容器触发析构违约。
    {
        dp::osal::ThreadPool pool;
        pool.start(1);
        join_error = EIO;
        pool.stop();
        pool.stop();  // 第二次回收此前仍被拥有的线程。
        pool.start(1);
        pool.stop();
        fail_creation = true;
        pool.start(1);
        fail_creation = false;
        assert(!pool.isStarted());
        pool.stop();
        pool.start(1);
        pool.stop();
    }
#endif
    std::puts("线程生命周期：300 次复用、超时重试、原生失败保留、协作停止、自连接及析构约束通过");
}
