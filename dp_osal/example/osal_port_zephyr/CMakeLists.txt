cmake_minimum_required(VERSION 3.20.0)

# example/osal_port_zephyr/CMakeLists.txt
#
# Zephyr RTOS port using built-in CMSIS-RTOS v2 compatibility layer.
#
# Usage in your project's CMakeLists.txt (before add_subdirectory(dp_osal)):
#   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/path/to/osal_port_zephyr)
#
# Zephyr provides cmsis_os2.h via its CMSIS-RTOS v2 compatibility layer.
# See: https://docs.zephyrproject.org/latest/services/portability/cmsis_rtos_v2.html
#
# The Zephyr build system typically handles include paths automatically.
# If cmsis_os2.h is not found, add:
#   ${ZEPHYR_BASE}/include/zephyr/portability to your include paths.

add_library(osal_port INTERFACE)

target_include_directories(osal_port INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZEPHYR_BASE}/include/zephyr/portability
)

target_link_libraries(osal_port INTERFACE zephyr_interface)
