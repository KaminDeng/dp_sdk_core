#include <gtest/gtest.h>
#include "gtest_thread.cpp"
#include "gtest_thread_start.cpp"
#if DP_OSAL_ENABLE_THREAD_POOL
#include "gtest_thread_pool.cpp"
#endif
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
