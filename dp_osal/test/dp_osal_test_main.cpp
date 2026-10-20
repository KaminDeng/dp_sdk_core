/**
 * @file dp_osal_test_main.cpp
 * @brief 由父工程提供任务上下文的OSAL GTest统一入口。
 * @details 直接包含各测试源以保留静态注册；外层功能宏避免已裁剪类型参与编译，
 * 各套件内部还可按TEST开关跳过。此入口设置OSAL过滤器，不创建自己的RTOS测试线程。
 * RUN_ALL_TESTS状态保存后丢弃，调用方不能仅凭void返回或宿主退出码宣称所有断言通过。
 */
//
// dp_osal_test_main.cpp — entry point for the OSAL gtest suite.
//
// Called from the unified app_demo_task in main.c.
// No FreeRTOS / OSAL threading code here — the task context is provided
// by the caller (xTaskCreate in main.c).
//
#include "dp_osal_debug.h"
#include "dp_osal_system.h"
#include "dp_osal_thread.h"

using namespace dp::osal;

#include <gtest/gtest.h>

// Include all gtest test files as part of this TU so their static GTest
// registrations are linked in when dp_osal_test_main() is referenced.
// Each file guards its tests with #if (DP_OSAL_TEST_YYY_ENABLED || DP_OSAL_TEST_ALL).
// The outer #if guards below prevent compilation errors when a primitive is
// disabled (its header becomes empty and its types are undeclared).
#if DP_OSAL_ENABLE_CHRONO
#include "gtest_chrono.cpp"
#include "gtest_chrono_deadline.cpp"
#endif
#if DP_OSAL_ENABLE_CONDITION_VAR
#include "gtest_condition_variable.cpp"
#endif
#include "gtest_lockguard.cpp"
#if DP_OSAL_ENABLE_MEMORY_MANAGER
#include "gtest_memory_manger.cpp"  // note: filename typo is intentional
#endif
#include "gtest_mutex.cpp"
#include "gtest_queue.cpp"
#if DP_OSAL_ENABLE_RW_LOCK
#include "gtest_rwlock.cpp"
#endif
#include "gtest_semaphore.cpp"
#if DP_OSAL_ENABLE_SPIN_LOCK
#include "gtest_spin_lock.cpp"
#endif
#include "gtest_thread.cpp"
#include "gtest_host_io.cpp"
#include "gtest_realtime_thread.cpp"
#include "gtest_static_timer.cpp"
#if DP_OSAL_ENABLE_THREAD_POOL
#include "gtest_thread_pool.cpp"
#endif
#if DP_OSAL_ENABLE_TIMER
#include "gtest_timer.cpp"
#endif
#include "gtest_virtual_injection.cpp"
#include "gtest_runtime_measurement.cpp"

/**
 * @brief 打印所选系统后端，初始化本地GTest参数并同步运行OSAL相关套件。
 * @details 不透传进程命令行；固定过滤器包括时钟、同步、线程、静态定时器、虚接口注入及Host I/O。
 * 测试是否实际执行须检查每个功能/测试开关及测试报告，不能把注册存在当作已运行。
 */
extern "C" void dp_osal_test_main(void) {
    DP_OSAL_LOGI("System Type: %s\n", System::getInstance().get_system_info());

    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) =
        "OSALChrono*:OSALConditionVariable*:OSALLockGuard*:OSALMemoryManager*:OSALMutex*:OSALMessageQueue*:OSALRWLock*:"
        "OSALSemaphore*:TestOSALSpinLock*:OSALThread*:OSALRealtimeThread*:OSALStaticTimer*:OSALTimer*:"
        "OSALVirtualInjection*:OSALRuntimeMeasurement*:OSALHostIo*";
    int gtest_result = RUN_ALL_TESTS();
    (void)gtest_result;
}
