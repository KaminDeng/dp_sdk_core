/** @file dp_osal_cmsis_rtos_native_tests.cpp
 * @brief 真实 FreeRTOS/RT-Thread 模拟内核的独立线程生命周期回归。
 * @details 测试仅替换不相关的日志叶；调度、完成通知与原生退出回收均使用真实内核。
 * 进程立即退出以终止常驻调度器；被测 worker 均已经显式 join。
 */
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "dp_osal_thread.h"
#include "cmsis_os2.h"
#include "dp_log.h"
extern "C" int dp_log_is_initialized(void) { return 0; }
extern "C" dp_log_tag_t dp_log_tag_register(const char*) { return 0; }
extern "C" void dp_log_write_ex(dp_log_level_t, dp_log_tag_t, const char*, const char*, uint16_t, const char*, ...) {}
static void tests(void*) {
    using namespace dp::osal;
    Thread thread;
    unsigned count = 0;
    for (unsigned i = 0; i < 48; ++i) {
        assert(thread.start("recycle", +[](void* arg) { ++*static_cast<unsigned*>(arg); }, &count).ok());
        assert(thread.joinable());
        assert(thread.join().ok());
        assert(!thread.joinable() && !thread.isRunning());
    }
    assert(count == 48);
    std::atomic<bool> entered{false};
    assert(thread
               .start("cooperative",
                      [&](void*) {
                          entered = true;
                          while (!thread.stopRequested()) osDelay(1);
                      })
               .ok());
    while (!entered.load()) osDelay(1);
    assert(thread.tryJoinFor(1).status == ThreadStatus::kNotSupported);
    assert(thread.stop().ok());
    assert(thread.start("self", [&](void*) { assert(thread.join().status == ThreadStatus::kWouldDeadlock); }).ok());
    assert(thread.join().ok());
    (std::puts)("CMSIS_RTOS_THREAD_PASS recycle=48 cooperative_stop=1 self_join=1 unsupported_timeout=1");
    std::fflush(stdout);
    std::_Exit(0);
}
int main() {
    static dp::osal::Thread runner;
    assert(osKernelInitialize() == osOK);
    assert(runner.start("test-runner", tests, nullptr, 0, 16384).ok());
    (void)osKernelStart();
    return 1;
}
