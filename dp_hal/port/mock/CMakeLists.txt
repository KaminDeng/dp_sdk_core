# kernel/dp_hal/port/mock/CMakeLists.txt
#
# Mock port implementation for dp_hal host testing.

add_library(dp_hal_port_impl STATIC
    dp_hal_port_impl.cpp
)

target_include_directories(dp_hal_port_impl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
           ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
