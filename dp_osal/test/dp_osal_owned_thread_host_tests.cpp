/**
 * @file dp_osal_owned_thread_host_tests.cpp
 * @brief 真实pthread后端的owned上下文清理生命周期主机回归。
 * @details 链接器wrap注入一次pthread_create失败；成功路径重复64次，cleanup真实delete内嵌Thread的上下文。
 * 启动表达式在断言之前执行；仍须保留有效断言才能核对结果。
 * 若目标启用ASAN，可进一步观测backend最后访问后的释放问题；源码存在不代表本轮已运行ASAN。
 */
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
/**
 * @brief 链接wrap的创建失败注入开关，只在主测试同步控制的失败分支置true。
 */
std::atomic<bool> fail_creation{false};
/**
 * @brief 比OwnedState更长寿的原子观察对象，记录入口、cleanup、析构次数与完成发布。
 * @details entries/cleanups/destructions分别累计一次生命周期事件，done用release/acquire供main等待。
 */
struct Observation {
    std::atomic<unsigned> entries{0U}, ///< raw入口累计进入次数。
        cleanups{0U},                 ///< backend调用cleanup累计次数。
        destructions{0U};             ///< 上下文析构累计次数。
    std::atomic<bool> hold{false};      ///< 测试超时期间保留上下文，随后显式允许正常退出。
    std::atomic<bool> done{false};     ///< delete完成后release发布，供main acquire等待。
};
/**
 * @brief 可被cleanup真实删除的上下文，内嵌线程后端并借用外部Observation。
 */
struct OwnedState {
    dp::osal::Thread worker; ///< 成功路径由cleanup删除所属对象，backend需先结束自身访问。
    std::shared_ptr<unsigned> payload; ///< 超时期间必须由仍存活的上下文保留的引用。
    Observation* observation; ///< 借用的外部计数对象，比当前上下文存活更久。
    /**
     * @brief 绑定外部观察对象，不启动任务。
     * @param value 覆盖worker清理期的观察对象。
     */
    explicit OwnedState(Observation& value) : observation(&value) {}
    /**
     * @brief 累计上下文析构次数；随后按C++成员析构顺序销毁worker。
     */
    ~OwnedState() { observation->destructions.fetch_add(1U); }
};
/**
 * @brief raw任务入口仅增加一次入口计数。
 * @param context 有效OwnedState，存活至后端cleanup。
 */
void entry(void* context) {
    auto* observation = static_cast<OwnedState*>(context)->observation;
    observation->entries.fetch_add(1U);
    while (observation->hold.load(std::memory_order_acquire)) std::this_thread::yield();
}
/**
 * @brief 缓存外部观察指针后delete上下文，再发布done完成标记。
 * @param context 成功启动后由backend移交的OwnedState。
 * @note delete同时销毁内嵌Thread，后端此后不能再读this或该上下文；观察对象仍由main持有。
 */
void cleanup(void* context) noexcept {
    auto* state = static_cast<OwnedState*>(context);
    auto* observation = state->observation;
    observation->cleanups.fetch_add(1U);
    delete state; // 真实删除含Thread的对象；ASAN检查backend此后不再碰this/TLS。
    observation->done.store(true, std::memory_order_release);
}
/** @brief 仅等待独立观察对象，不读取可能已被cleanup释放的state。 */
void waitDone(Observation& observation) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!observation.done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    assert(observation.done.load(std::memory_order_acquire));
}
Observation* null_observation = nullptr; ///< null上下文用例由main顺序装配，不在运行中改写。
void nullEntry(void* context) { assert(context == nullptr); null_observation->entries.fetch_add(1U); }
void nullCleanup(void* context) noexcept {
    assert(context == nullptr);
    null_observation->cleanups.fetch_add(1U);
    null_observation->done.store(true, std::memory_order_release);
}

}
/**
 * @brief 链接器--wrap提供的真实pthread_create入口声明，不是本文件自己实现。
 * @details 未命名参数沿pthread_create签名传递；测试目标须配置正确链接wrap选项。
 */
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
/**
 * @brief 创建失败开关开启时返回EAGAIN，否则转发真实pthread_create。
 * @param handle 后端线程句柄输出。
 * @param attr 线程属性。
 * @param run pthread入口。
 * @param context pthread上下文。
 * @return 注入的EAGAIN或真实创建结果，不创建硬件/设备任务。
 */
extern "C" int __wrap_pthread_create(pthread_t* handle, const pthread_attr_t* attr, void* (*run)(void*), void* context) {
    if (fail_creation.load()) return EAGAIN;
    return __real_pthread_create(handle, attr, run, context);
}
/**
 * @brief 重复验证成功路径恰好一次清理，再验证创建/参数失败不接管上下文。
 * @return 断言全部满足时隐式0，否则assert终止；需保持NDEBUG未定义。
 * @details 每轮以2秒主机单调期限等待done，不是实时延迟上界测试。失败创建由unique_ptr保留所有权，
 * 空入口或空cleanup仍不调用cleanup，最后由调用者reset触发一次析构。
 */
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
