# kernel/dp_device/test/CMakeLists.txt
#
# dp_device test static library (linked into firmware test binary).
# Test .cpp files are #included by dp_device_test_main.cpp (single TU pattern).

add_library(dp_device_test STATIC
    dp_device_test_main.cpp
)

target_include_directories(dp_device_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(dp_device_test PRIVATE dp_device dp_hal gtest dpsdk_compat)
