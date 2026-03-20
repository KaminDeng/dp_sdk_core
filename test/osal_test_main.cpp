//
// Created by kamin.deng on 2024/8/14.
//
#include "osal_debug.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

#ifdef OSAL_CONFIG_GOOGLETEST_ENABLE
#include <gtest/gtest.h>

#include "gtest_chrono.cpp"  // 如果系统启动时时间为0, 可能会测试失败
#include "gtest_condition_variable.cpp"
#include "gtest_lockguard.cpp"
#include "gtest_memory_manger.cpp"
#include "gtest_mutex.cpp"
#include "gtest_queue.cpp"
#include "gtest_rwlock.cpp"
#include "gtest_semaphore.cpp"
#include "gtest_spin_lock.cpp"
#include "gtest_thread.cpp"
#include "gtest_thread_pool.cpp"
#include "gtest_timer.cpp"
#endif

void StartDefaultTask(void *argument) {
    (void)argument;
    // setLogLevel(LOG_LEVEL_VERBOSE);

#ifdef OSAL_CONFIG_GOOGLETEST_ENABLE
    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
#endif
}

OSALThread osal_test_thread;
extern "C" int osal_test_main(void) {
    static int arg = 12;
    OSAL_LOGI("System Type: %s\n", OSALSystem::getInstance().get_system_info());
    osal_test_thread.start("osal_test_thread", StartDefaultTask, (void *)&arg, 0, 2048);
    OSALSystem::getInstance().StartScheduler();
    return 0;
}
