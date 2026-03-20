cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 需要在定义 osal_config.cmake 中定义 OSAL_CONFIG_PLATFORM 和 OSAL_CONFIG_DEPENDENT_LIBRARIES
include("${CMAKE_CURRENT_SOURCE_DIR}/../osal_port/osal_config.cmake")
if (NOT DEFINED OSAL_CONFIG_DEPENDENT_LIBRARIES OR NOT DEFINED OSAL_CONFIG_GOOGLETEST_ENABLE)
    message(FATAL_ERROR "OSAL_CONFIG_DEPENDENT_LIBRARIES / OSAL_CONFIG_GOOGLETEST_ENABLE is not defined")
endif()

if (DEFINED OSAL_CONFIG_GOOGLETEST_ENABLE)
    add_definitions(-DOSAL_CONFIG_GOOGLETEST_ENABLE)
else()
    set(OSAL_CONFIG_GOOGLETEST_ENABLE 0)
    message(STATUS "OSAL_CONFIG_GOOGLETEST_ENABLE:${OSAL_CONFIG_GOOGLETEST_ENABLE}")
endif()

set(GOOGLETEST_SUITE_PATH "test/gtest")
set(OSAL_IMPL_PATH "src/impl")

# Check the OSAL_CONFIG_PLATFORM value and set sources accordingly
if (OSAL_CONFIG_PLATFORM STREQUAL "OSAL_CONFIG_POSIX")
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/posix/*.cpp")
    message(STATUS "Platform Config: ${OSAL_CONFIG_PLATFORM}")
    message(STATUS "Source Files for POSIX Platform: ${SYSTEM_IMPL_SOURCES}")

elseif (OSAL_CONFIG_PLATFORM STREQUAL "OSAL_CONFIG_CMSIS_OS")
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/cmsis_os/*.cpp")
    message(STATUS "Platform Config: ${OSAL_CONFIG_PLATFORM}")
    message(STATUS "Source Files for CMSIS OS Platform: ${SYSTEM_IMPL_SOURCES}")

else ()
    message(FATAL_ERROR "Unsupported platform '${OSAL_CONFIG_PLATFORM}' specified. No sources found: ${SYSTEM_IMPL_SOURCES}")
endif ()

file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

# 添加库
add_library(osal STATIC
        src/debug/osal_debug.cpp
        ${SYSTEM_IMPL_SOURCES}
        test/osal_test_main.cpp
        $<IF:${OSAL_CONFIG_GOOGLETEST_ENABLE},${GTEST_SUITE_SOURCES},>
)

# 遍历子目录以确保包含所有头文件
foreach (subdir
        src
        src/interface
        src/debug
        $<IF:$<STREQUAL:${OSAL_CONFIG_PLATFORM},OSAL_CONFIG_CMSIS_OS>,src/impl/cmsis_os/include,>
        $<IF:$<STREQUAL:${OSAL_CONFIG_PLATFORM},OSAL_CONFIG_POSIX>,src/impl/posix/include,>
        test
        $<IF:${OSAL_CONFIG_GOOGLETEST_ENABLE},${GOOGLETEST_SUITE_PATH},>
)
    target_include_directories(osal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
endforeach ()

target_link_libraries(osal ${OSAL_CONFIG_DEPENDENT_LIBRARIES})
