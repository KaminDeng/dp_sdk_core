
#include "dp_osal_thread.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <memory>
#include <new>
#include <pthread.h>
#include <cstdio>
#include <thread>

namespace {

std::atomic<bool> fail_creation{false};

struct Observation {
    std::atomic<unsigned> entries{0U},
        cleanups{0U},
        destructions{0U};
    std::atomic<bool> hold{false};
    std::atomic<bool> done{false};
};

struct OwnedState {
    dp::osal::Thread worker;
    std::shared_ptr<unsigned> payload;
    Observation* observation;

    explicit OwnedState(Observation& value) : observation(&value) {}

    ~OwnedState() { observation->destructions.fetch_add(1U); }
};

void entry(void* context) {
    auto* observation = static_cast<OwnedState*>(context)->observation;
    observation->entries.fetch_add(1U);
    while (observation->hold.load(std::memory_order_acquire)) std::this_thread::yield();
}

void cleanup(void* context) noexcept {
    auto* state = static_cast<OwnedState*>(context);
    auto* observation = state->observation;
    observation->cleanups.fetch_add(1U);
    delete state; // 真实删除含Thread的对象；ASAN检查backend此后不再碰this/TLS。
    observation->done.store(true, std::memory_order_release);
}

void waitDone(Observation& observation) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!observation.done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    assert(observation.done.load(std::memory_order_acquire));
}
Observation* null_observation = nullptr;
void nullEntry(void* context) { assert(context == nullptr); null_observation->entries.fetch_add(1U); }
void nullCleanup(void* context) noexcept {
    assert(context == nullptr);
    null_observation->cleanups.fetch_add(1U);
    null_observation->done.store(true, std::memory_order_release);
}

}

extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);

extern "C" int __wrap_pthread_create(pthread_t* handle, const pthread_attr_t* attr, void* (*run)(void*), void* context) {
    if (fail_creation.load()) return EAGAIN;
    return __real_pthread_create(handle, attr, run, context);
}

int main() {
    // 普通raw复用原生启动后，创建失败仍回滚，下一次启动不残留旧入口/参数。
    dp::osal::Thread ordinary;
    std::atomic<unsigned> raw_calls{0U};
    auto raw_entry = +[](void* context) { static_cast<std::atomic<unsigned>*>(context)->fetch_add(1U); };
    fail_creation = true;
    const int raw_failed = ordinary.startRaw("raw-fail", raw_entry, &raw_calls);
    fail_creation = false;
    assert(raw_failed != 0 && raw_calls.load() == 0U);
    const int raw_retried = ordinary.startRaw("raw-retry", raw_entry, &raw_calls);
    assert(raw_retried == 0); ordinary.join(); assert(raw_calls.load() == 1U);
    std::weak_ptr<unsigned> failed_capture;
    fail_creation = true;
    // 原生失败不得残留可调用包装；用独立weak引用检查实际捕获析构。
    auto capture = std::make_shared<unsigned>(3U); failed_capture = capture;
    const int observed_failed = ordinary.start("capture-fail", [capture](void*) {}, nullptr);
    capture.reset(); fail_creation = false;
    assert(observed_failed != 0 && failed_capture.expired());
    // 在有效断言装配下真实创建并等待64个独立任务，覆盖快速返回与清理的重复生命周期。
    for (unsigned index = 0U; index < 64U; ++index) {
        Observation observation;
        auto* state = new(std::nothrow) OwnedState(observation); assert(state);
        const int started = state->worker.startRawOwned("owned", entry, state, cleanup);
        assert(started == 0);
        waitDone(observation);
        assert(observation.done.load() && observation.entries.load() == 1U && observation.cleanups.load() == 1U &&
               observation.destructions.load() == 1U);
    }
    Observation failed;
    std::unique_ptr<OwnedState> state(new(std::nothrow) OwnedState(failed)); assert(state);
    fail_creation = true;
    const int failed_start = state->worker.startRawOwned("failed", entry, state.get(), cleanup);
    assert(failed_start != 0);
    fail_creation = false;
    assert(failed.entries.load() == 0U && failed.cleanups.load() == 0U && failed.destructions.load() == 0U);
    const int invalid_entry = state->worker.startRawOwned("invalid", nullptr, state.get(), cleanup);
    assert(invalid_entry != 0);
    const int invalid_cleanup = state->worker.startRawOwned("invalid", entry, state.get(), nullptr);
    assert(invalid_cleanup != 0);
    assert(failed.cleanups.load() == 0U);
    state.reset(); assert(failed.destructions.load() == 1U);

    // 原生创建失败后，同一对象重试仍能移交一次清理责任。
    Observation retried;
    auto* retry = new(std::nothrow) OwnedState(retried); assert(retry);
    fail_creation = true;
    const int retry_failed = retry->worker.startRawOwned("retry-fail", entry, retry, cleanup);
    fail_creation = false;
    assert(retry_failed != 0 && retried.cleanups.load() == 0U && retried.entries.load() == 0U);
    const int retry_started = retry->worker.startRawOwned("retry", entry, retry, cleanup);
    assert(retry_started == 0); waitDone(retried);
    assert(retried.entries.load() == 1U && retried.cleanups.load() == 1U && retried.destructions.load() == 1U);

    // 普通等待超时不表示内核退出，也不能回收任务仍持有的引用。
    Observation blocked; blocked.hold = true;
    auto* retained = new(std::nothrow) OwnedState(blocked); assert(retained);
    retained->payload = std::make_shared<unsigned>(7U);
    std::weak_ptr<unsigned> payload = retained->payload;
    const int blocked_started = retained->worker.startRawOwned("retained", entry, retained, cleanup);
    assert(blocked_started == 0);
    const auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
    while (std::chrono::steady_clock::now() < timeout) std::this_thread::yield();
    assert(!blocked.done.load() && blocked.cleanups.load() == 0U && blocked.destructions.load() == 0U);
    assert(!payload.expired());
    blocked.hold.store(false, std::memory_order_release); waitDone(blocked);
    assert(blocked.entries.load() == 1U && blocked.cleanups.load() == 1U && blocked.destructions.load() == 1U);
    assert(payload.expired());

    // nullptr参数是有效借用值；非空入口和cleanup必须各执行一次。
    Observation null_context; null_observation = &null_context;
    dp::osal::Thread null_worker;
    const int null_started = null_worker.startRawOwned("null-context", nullEntry, nullptr, nullCleanup);
    assert(null_started == 0); waitDone(null_context);
    assert(null_context.entries.load() == 1U && null_context.cleanups.load() == 1U);
    std::puts("owned: success=67 create_failure=2 invalid_parameters=2 timeout_retained=1 null_context=1");
}
