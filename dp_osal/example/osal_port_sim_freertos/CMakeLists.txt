cmake_minimum_required(VERSION 3.15)

# example/osal_port_sim_freertos/CMakeLists.txt
#
# FreeRTOS-on-POSIX simulator port using CMSIS-OS2 adapter — new osal_port.h format.
#
# Usage in your project's CMakeLists.txt (before add_subdirectory(dp_osal)):
#   set(DP_OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/path/to/osal_port_sim_freertos)
#
# The cmsis_os2_freertos/ subdirectory contains the CMSIS-OS2 implementation
# that wraps FreeRTOS APIs. Its include paths are exported here so dp_osal can
# find cmsis_os2.h when OSAL_BACKEND_CMSIS_OS is selected.

add_library(osal_port INTERFACE)

target_include_directories(osal_port INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis_os2_freertos/cmsis_base
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis_os2_freertos
)

target_link_libraries(osal_port INTERFACE freertos_kernel freertos_config)
