cmake_minimum_required(VERSION 3.15)

dpsdk_library(
    NAME          dp_osal
    DESCRIPTION   "C++17 OS 抽象层"
    PRIMARY_TAG   general
    TAGS          general dp_osal
    NOTES         "通过 dp_osal_port 选择后端（POSIX/CMSIS-OS2）"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(DP_OSAL_ENABLE_THREAD_POOL    "线程池"         ON)
option(DP_OSAL_ENABLE_RW_LOCK        "读写锁"         ON)
option(DP_OSAL_ENABLE_SPIN_LOCK      "自旋锁"         ON)
option(DP_OSAL_ENABLE_CONDITION_VAR  "条件变量"        ON)
option(DP_OSAL_ENABLE_TIMER          "软件定时器"      ON)
option(DP_OSAL_ENABLE_CHRONO         "高精度时钟"      ON)
option(DP_OSAL_ENABLE_MEMORY_MANAGER "OSAL 内存管理"   ON)
option(DP_OSAL_ENABLE_THREAD_SNAPSHOT  "线程快照（top）"  ON)
option(DP_OSAL_STATIC_ALLOC          "静态分配模式"    OFF)
option(DP_OSAL_BUILD_HOST_TESTS      "Build standalone host OSAL tests" OFF)

if (NOT DEFINED DP_OSAL_PORT_DIR)
    message(STATUS "[dp_osal] DP_OSAL_PORT_DIR not set — using built-in POSIX port (port/builtin_posix)")
    set(DP_OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/builtin_posix")
endif()

if (NOT EXISTS "${DP_OSAL_PORT_DIR}/dp_osal_port.h")
    message(FATAL_ERROR
        "[dp_osal] dp_osal_port.h not found in DP_OSAL_PORT_DIR='${DP_OSAL_PORT_DIR}'.\n"
        "Copy dp_osal/port/template/dp_osal_port.h to '${DP_OSAL_PORT_DIR}/' and fill it in.\n"
        "Then set DP_OSAL_PORT_DIR to that directory before add_subdirectory(dp_osal).")
endif()

file(STRINGS "${DP_OSAL_PORT_DIR}/dp_osal_port.h" _osal_port_lines)
set(_osal_backend "posix")
foreach(_line IN LISTS _osal_port_lines)
    if (_line MATCHES "^[ \t]*#[ \t]*define[ \t]+DP_OSAL_BACKEND_CMSIS_OS")
        set(_osal_backend "cmsis_os")
        break()
    endif()
    if (_line MATCHES "^[ \t]*#[ \t]*define[ \t]+DP_OSAL_BACKEND_ESP_IDF")
        set(_osal_backend "esp_idf")
        break()
    endif()
endforeach()
message(STATUS "[dp_osal] Port directory: ${DP_OSAL_PORT_DIR}")
message(STATUS "[dp_osal] Backend:        ${_osal_backend}")

set(OSAL_IMPL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/impl")
set(GOOGLETEST_SUITE_PATH "test/gtest")

if (_osal_backend STREQUAL "esp_idf")
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${DP_OSAL_PORT_DIR}/src/*.cpp")
else()
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
endif()
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")
set(_osal_single_tu_tests
    chrono chrono_deadline condition_variable lockguard memory_manger mutex queue rwlock semaphore
    spin_lock thread thread_pool timer virtual_injection runtime_measurement measurement_detail
    contract_compat snapshot_detail realtime_thread static_timer host_io)
foreach(_suite IN LISTS _osal_single_tu_tests)
    list(FILTER GTEST_SUITE_SOURCES EXCLUDE REGEX "/gtest_${_suite}\\.cpp$")
endforeach()

message(STATUS "[dp_osal] Impl sources:   ${SYSTEM_IMPL_SOURCES}")

add_library(dp_osal STATIC
    src/debug/dp_osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/dp_osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

target_include_directories(dp_osal
    PUBLIC
        "${DP_OSAL_PORT_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/debug"
        $<$<STREQUAL:${_osal_backend},esp_idf>:${DP_OSAL_PORT_DIR}>
        $<$<STREQUAL:${_osal_backend},cmsis_os>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/cmsis_os/include>
        $<$<STREQUAL:${_osal_backend},posix>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/posix/include>
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/${GOOGLETEST_SUITE_PATH}"
)


target_link_libraries(dp_osal PUBLIC gtest)
if(_osal_backend STREQUAL "posix")
    find_package(Threads REQUIRED)
    target_link_libraries(dp_osal PUBLIC Threads::Threads)
endif()

target_compile_options(dp_osal PUBLIC -Wno-gnu-zero-variadic-macro-arguments)

if(DP_OSAL_BUILD_HOST_TESTS AND _osal_backend STREQUAL "posix")
    include(CTest)
    add_executable(dp_osal_chrono_host_tests
        test/dp_osal_chrono_host_test_main.cpp
        test/dp_osal_host_console_stub.cpp
    )
    target_link_libraries(dp_osal_chrono_host_tests PRIVATE dp_osal gtest)
    target_compile_features(dp_osal_chrono_host_tests PRIVATE cxx_std_17)
    if(BUILD_TESTING)
        add_test(NAME dp_osal_chrono_host_tests COMMAND dp_osal_chrono_host_tests)
    endif()
    add_executable(dp_osal_contract_host_tests
        test/dp_osal_contract_host_test_main.cpp test/dp_osal_host_console_stub.cpp)
    target_link_libraries(dp_osal_contract_host_tests PRIVATE dp_osal gtest)
    if(BUILD_TESTING)
        add_test(NAME dp_osal_contract_host_tests COMMAND dp_osal_contract_host_tests)
        if(DP_OSAL_ENABLE_CHRONO AND UNIX)
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            add_test(NAME dp_osal_chrono_unsupported_host_test
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test/check_chrono_unsupported.py
                    $<TARGET_FILE:dp_osal_contract_host_tests>)
        endif()
    endif()

endif()

foreach(_feat THREAD_POOL RW_LOCK SPIN_LOCK CONDITION_VAR TIMER CHRONO MEMORY_MANAGER THREAD_SNAPSHOT)
    if(DP_OSAL_ENABLE_${_feat})
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=1)
    else()
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=0)
    endif()
endforeach()
if(DP_OSAL_STATIC_ALLOC)
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=1)
else()
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=0)
endif()
