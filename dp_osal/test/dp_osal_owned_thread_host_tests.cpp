/**
 * @file dp_osal_owned_thread_host_tests.cpp
 * @brief 真实pthread后端的owned上下文清理生命周期主机回归。
 * @details 链接器wrap注入一次pthread_create失败；成功路径重复64次，cleanup真实delete内嵌Thread的上下文。
 * 需开启assert，因部分启动表达式位于assert内部；定义NDEBUG不仅删检查，也会删这些调用。
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
    std::atomic<bool> done{false};     ///< delete完成后release发布，供main acquire等待。
};
/**
 * @brief 可被cleanup真实删除的上下文，内嵌线程后端并借用外部Observation。
 */
struct OwnedState {
    dp::osal::Thread worker; ///< 成功路径由cleanup删除所属对象，backend需先结束自身访问。
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
void entry(void* context) { static_cast<OwnedState*>(context)->observation->entries.fetch_add(1U); }
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
    // 在有效断言装配下真实创建并等待64个独立任务，覆盖快速返回与清理的重复生命周期。
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
