#include "dp_osal_thread.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <memory>
#include <new>
#include <pthread.h>
#include <thread>

namespace {
std::atomic<bool> fail_creation{false};
struct Observation { std::atomic<unsigned> entries{0U}, cleanups{0U}, destructions{0U}; std::atomic<bool> done{false}; };
struct OwnedState {
    dp::osal::Thread worker;
    Observation* observation;
    explicit OwnedState(Observation& value) : observation(&value) {}
    ~OwnedState() { observation->destructions.fetch_add(1U); }
};
void entry(void* context) { static_cast<OwnedState*>(context)->observation->entries.fetch_add(1U); }
void cleanup(void* context) noexcept {
    auto* state = static_cast<OwnedState*>(context);
    auto* observation = state->observation;
    observation->cleanups.fetch_add(1U);
    delete state; // 真实删除含Thread的对象；ASAN检查backend此后不再碰this/TLS。
    observation->done.store(true, std::memory_order_release);
}
}
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* handle, const pthread_attr_t* attr, void* (*run)(void*), void* context) {
    if (fail_creation.load()) return EAGAIN;
    return __real_pthread_create(handle, attr, run, context);
}
int main() {
    for (unsigned index = 0U; index < 64U; ++index) {
        Observation observation;
        auto* state = new(std::nothrow) OwnedState(observation); assert(state);
        assert(state->worker.startRawOwned("owned", entry, state, cleanup) == 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!observation.done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        assert(observation.done.load() && observation.entries.load() == 1U && observation.cleanups.load() == 1U &&
               observation.destructions.load() == 1U);
    }
    Observation failed;
    std::unique_ptr<OwnedState> state(new(std::nothrow) OwnedState(failed)); assert(state);
    fail_creation = true;
    assert(state->worker.startRawOwned("failed", entry, state.get(), cleanup) != 0);
    fail_creation = false;
    assert(failed.entries.load() == 0U && failed.cleanups.load() == 0U && failed.destructions.load() == 0U);
    assert(state->worker.startRawOwned("invalid", nullptr, state.get(), cleanup) != 0);
    assert(state->worker.startRawOwned("invalid", entry, state.get(), nullptr) != 0);
    assert(failed.cleanups.load() == 0U);
    state.reset(); assert(failed.destructions.load() == 1U);
}
