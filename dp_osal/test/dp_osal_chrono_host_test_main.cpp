#include <gtest/gtest.h>

#if DP_OSAL_ENABLE_CHRONO
#include "gtest_chrono_deadline.cpp"
#endif
#include "gtest_realtime_thread.cpp"

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
