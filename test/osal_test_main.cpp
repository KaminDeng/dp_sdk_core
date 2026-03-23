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

extern "C" void osal_test_main(void) {
    OSAL_LOGI("System Type: %s\n", OSALSystem::getInstance().get_system_info());

    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
}
