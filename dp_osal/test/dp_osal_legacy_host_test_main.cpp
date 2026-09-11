#include <gtest/gtest.h>
#include "gtest_realtime_thread.cpp"
#include "gtest_static_timer.cpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
