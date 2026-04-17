# example/osal_port_freertos/CMakeLists.txt
#
# STM32 FreeRTOS+CMSIS-OS2 port — new osal_port.h single-file format.
#
# Usage in your project's CMakeLists.txt (before add_subdirectory(dp_osal)):
#   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/path/to/osal_port_freertos)
#
# NOTE: osal_port_debug_write uses HAL_UART_Transmit (huart6).
# Your project must link stm32cubemx (or your equivalent HAL library).
# If you need _write() newlib redirection, add a .cpp file with the
# extern "C" int _write(...) implementation shown in osal_port.h.
cmake_minimum_required(VERSION 3.15)

add_library(osal_port INTERFACE)

target_include_directories(osal_port INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# STM32 HAL library provides HAL_UART_Transmit used in osal_port_debug_write.
# Adjust the library name to match your BSP.
target_link_libraries(osal_port INTERFACE stm32cubemx)
