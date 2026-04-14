//
// osal_test_main.cpp — entry point for the OSAL gtest suite.
//
// Called from the unified app_demo_task in main.c.
// No FreeRTOS / OSAL threading code here — the task context is provided
// by the caller (xTaskCreate in main.c).
//
#include "osal_debug.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

#include <gtest/gtest.h>

// Include all gtest test files as part of this TU so their static GTest
// registrations are linked in when osal_test_main() is referenced.
// Each file guards its tests with #if (OSAL_TEST_YYY_ENABLED || OSAL_TEST_ALL).
// The outer #if guards below prevent compilation errors when a primitive is
// disabled (its header becomes empty and its types are undeclared).
#if OSAL_ENABLE_CHRONO
#include "gtest_chrono.cpp"
#endif
#if OSAL_ENABLE_CONDITION_VAR
#include "gtest_condition_variable.cpp"
#endif
#include "gtest_lockguard.cpp"
#if OSAL_ENABLE_MEMORY_MANAGER
#include "gtest_memory_manger.cpp"  // note: filename typo is intentional
#endif
#include "gtest_mutex.cpp"
#include "gtest_queue.cpp"
#if OSAL_ENABLE_RW_LOCK
#include "gtest_rwlock.cpp"
#endif
#include "gtest_semaphore.cpp"
#if OSAL_ENABLE_SPIN_LOCK
#include "gtest_spin_lock.cpp"
#endif
#include "gtest_thread.cpp"
#if OSAL_ENABLE_THREAD_POOL
#include "gtest_thread_pool.cpp"
#endif
#if OSAL_ENABLE_TIMER
#include "gtest_timer.cpp"
#endif
#include "gtest_virtual_injection.cpp"

extern "C" void osal_test_main(void) {
    OSAL_LOGI("System Type: %s\n", OSALSystem::getInstance().get_system_info());

    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) =
        "OSALChrono*:OSALConditionVariable*:OSALLockGuard*:OSALMemoryManager*:OSALMutex*:OSALMessageQueue*:OSALRWLock*:"
        "OSALSemaphore*:TestOSALSpinLock*:OSALThread*:OSALTimer*:OSALVirtualInjection*";
    int gtest_result = RUN_ALL_TESTS();
    (void)gtest_result;
}
