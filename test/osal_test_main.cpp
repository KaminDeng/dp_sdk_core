//
// osal_test_main.cpp — Test entry point for the osal library.
//
// All gtest test files are #included here so they compile as part of
// the osal static library (no separate test executable is needed;
// osal_test_main() is called from the host application's task).
//
// Test feature flags (OSAL_TEST_ALL, OSAL_TEST_THREAD_ENABLED, etc.)
// are defined in osal_port.h (Section 5).
//
#include "osal_debug.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

#include <gtest/gtest.h>

// Include all gtest test files. Each file guards its tests with
// #if (OSAL_TEST_YYY_ENABLED || OSAL_TEST_ALL)
#include "gtest_chrono.cpp"
#include "gtest_condition_variable.cpp"
#include "gtest_lockguard.cpp"
#include "gtest_memory_manger.cpp"  // note: filename typo is intentional
#include "gtest_mutex.cpp"
#include "gtest_queue.cpp"
#include "gtest_rwlock.cpp"
#include "gtest_semaphore.cpp"
#include "gtest_spin_lock.cpp"
#include "gtest_thread.cpp"
#include "gtest_thread_pool.cpp"
#include "gtest_timer.cpp"

void StartDefaultTask(void *argument) {
    (void)argument;
    // Uncomment to enable verbose osal logging during tests:
    // setLogLevel(LOG_LEVEL_VERBOSE);

    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
}

OSALThread osal_test_thread;

extern "C" int osal_test_main(void) {
    static int arg = 12;
    OSAL_LOGI("System Type: %s\n", OSALSystem::getInstance().get_system_info());
    osal_test_thread.start("osal_test_thread", StartDefaultTask, static_cast<void *>(&arg), 0, 2048);
    OSALSystem::getInstance().StartScheduler();
    return 0;
}
