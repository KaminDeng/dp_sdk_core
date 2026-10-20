##
# @file CMakeLists.txt
# @brief 从端口头选择OSAL实现、传播裁剪开关，并装配既有测试目标。
# @details 此目录由父工程add_subdirectory消费，依赖父工程的dpsdk_library及gtest目标。
# 配置阶段依次处理功能option、端口路径、后端文本识别、源码/头路径、链接依赖与可选主机测试。
# 下列option是构建/接口裁剪，不等于测试已经运行或具体后端已经实现该能力。
# 修改注释不执行configure/build/test；实际配置值以父工程缓存与产品装配为准。
cmake_minimum_required(VERSION 3.15)

dpsdk_library(
    NAME          dp_osal
    DESCRIPTION   "C++17 OS 抽象层"
    PRIMARY_TAG   general
    TAGS          general dp_osal
    NOTES         "通过 dp_osal_port 选择后端（POSIX/CMSIS-OS2）"
)

# 此目录要求C++17；PUBLIC头中的CRTP、inline静态成员和if constexpr依赖对应语言能力。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── OSAL 原语裁剪 ──────────────────────────────────────
# ENABLE_*传播为明确0/1宏，供公共头及测试决定是否保留类型；默认ON不代表后端能力经过鉴定。
# STATIC_ALLOC使用独立option名，后面映射到DP_OSAL_ENABLE_STATIC_ALLOC。
# BUILD_HOST_TESTS只控制下方额外独立可执行目标，不会在此移除dp_osal库中的既有测试源。
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

# ── Port directory resolution ──────────────────────────────────────────────────
# Users set DP_OSAL_PORT_DIR before add_subdirectory(dp_osal) to supply their port:
#   set(DP_OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
#
# If DP_OSAL_PORT_DIR is not set, the built-in POSIX port is used (stdout debug,
# all tests enabled). This is suitable for Linux/macOS development out of the box.
# 父工程必须在add_subdirectory之前设置自定义端口；仅检查“是否定义”，空值也不会触发默认回退。
# 默认POSIX不意味着Host I/O可在任意POSIX系统使用，当前Host专用实现仍显式限制Linux。
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

# ── Backend auto-detection ─────────────────────────────────────────────────────
# Scan dp_osal_port.h for the DP_OSAL_BACKEND_CMSIS_OS macro (non-commented line).
# Defaults to posix if not found. Defining both backends is undefined behaviour.
# 这里按头文件文本逐行匹配首个CMSIS或ESP_IDF define，不执行C预处理器：
# 不求值#define后的0/1，不解析#if禁用区域，也不完整识别跨行块注释；端口头须遵守可识别写法。
# 未匹配时取posix，匹配后break，不能把它当成多后端冲突检测器。
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

# ── Source selection ───────────────────────────────────────────────────────────
set(OSAL_IMPL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/impl")
set(GOOGLETEST_SUITE_PATH "test/gtest")

# ESP-IDF从调用者端口src取实现；其它后端从仓库内对应impl目录收集cpp。
# GLOB_RECURSE在configure阶段展开，新增文件后需重新配置才能反映到源集合。
if (_osal_backend STREQUAL "esp_idf")
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${DP_OSAL_PORT_DIR}/src/*.cpp")
else()
    file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
endif()
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

message(STATUS "[dp_osal] Impl sources:   ${SYSTEM_IMPL_SOURCES}")

# ── Library target ─────────────────────────────────────────────────────────────
# 静态库同时列入debug、实现、统一测试入口和GLOB得到的测试cpp。
# 统一入口还直接include测试源保留注册；这里保留既有源装配，不根据注释推断已链接或已执行的套件。
add_library(dp_osal STATIC
    src/debug/dp_osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/dp_osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

# ── Include directories ────────────────────────────────────────────────────────
# DP_OSAL_PORT_DIR is PUBLIC so downstream targets can also see dp_osal_port.h
# (needed if they use dp_osal types directly or include any dp_osal header).
# PUBLIC路径供依赖者包含端口、CRTP接口和所选后端，生成器表达式只添加匹配后端目录。
# 公开路径与后面的PUBLIC 0/1宏需一起理解，不能混用来自另一产品后端的头。
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

# ── Dependencies ───────────────────────────────────────────────────────────────
# gtest is always linked (test sources always compiled).
# For CMSIS-OS ports that depend on additional platform libraries, the user
# must link them externally:
#   target_link_libraries(dp_osal PUBLIC freertos_kernel cmsis_os2_impl)
target_link_libraries(dp_osal PUBLIC gtest)
if(_osal_backend STREQUAL "posix")
    # 线程平台依赖由OSAL后端公开传递，不要求通信/应用反向猜测pthread。
    find_package(Threads REQUIRED)
    target_link_libraries(dp_osal PUBLIC Threads::Threads)
endif()

# osal_debug.h uses ##__VA_ARGS__ (GNU extension) to support zero-arg variadic
# macro calls. Suppress the pedantic warning about this GNU extension.
target_compile_options(dp_osal PUBLIC -Wno-gnu-zero-variadic-macro-arguments)

# 只有显式打开独立Host测试且文本识别后端为posix才进入；BUILD_TESTING另决定是否注册CTest。
# 生成测试可执行文件、注册CTest和实际执行是三个不同阶段。
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
    # owned目标强制保留assert（部分启动调用就在assert中），禁异常/RTTI，并wrap真实pthread_create注入失败。
    # 仍使用真实主机线程；这不是MCU烧录或机械臂动作目标。
    add_executable(dp_osal_owned_thread_host_tests
        test/dp_osal_owned_thread_host_tests.cpp
        test/dp_osal_host_console_stub.cpp)
    target_link_libraries(dp_osal_owned_thread_host_tests PRIVATE dp_osal)
    target_compile_options(dp_osal_owned_thread_host_tests PRIVATE -UNDEBUG -fno-exceptions -fno-rtti)
    target_link_options(dp_osal_owned_thread_host_tests PRIVATE -Wl,--wrap=pthread_create)
    if(BUILD_TESTING)
        add_test(NAME dp_osal_owned_thread_host_tests COMMAND dp_osal_owned_thread_host_tests)
    endif()
    # 同一源生成三种独立测量目标：主机steady_clock、MCU宏模拟但无hook、MCU宏模拟加固定步进hook。
    # 这些宏只在主机编译选择代码分支，不会因此取得真实MCU的时钟或硬件资格。
    foreach(_clock_case host no_clock hook)
        add_executable(dp_osal_measurement_${_clock_case}_tests test/dp_osal_measurement_clock_host_tests.cpp)
        target_include_directories(dp_osal_measurement_${_clock_case}_tests PRIVATE src/interface)
        target_compile_features(dp_osal_measurement_${_clock_case}_tests PRIVATE cxx_std_17)
        target_compile_options(dp_osal_measurement_${_clock_case}_tests PRIVATE -UNDEBUG)
        if(NOT _clock_case STREQUAL "host")
            target_compile_definitions(dp_osal_measurement_${_clock_case}_tests PRIVATE DPSDK_MCU_FIRMWARE=1)
        endif()
        if(_clock_case STREQUAL "hook")
            target_compile_definitions(dp_osal_measurement_${_clock_case}_tests PRIVATE DP_OSAL_TEST_CLOCK_HOOK=1)
        endif()
        if(BUILD_TESTING)
            add_test(NAME dp_osal_measurement_${_clock_case}_tests COMMAND dp_osal_measurement_${_clock_case}_tests)
        endif()
    endforeach()

endif()

# ── 原语裁剪：传播编译定义 ────────────────────────────────────────────────────
# 将option缓存结果变成PUBLIC编译定义，消费者看到与库相同的裁剪界面；每个开关总有明确0/1值。
foreach(_feat THREAD_POOL RW_LOCK SPIN_LOCK CONDITION_VAR TIMER CHRONO MEMORY_MANAGER THREAD_SNAPSHOT)
    if(DP_OSAL_ENABLE_${_feat})
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=1)
    else()
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=0)
    endif()
endforeach()
# STATIC_ALLOC 使用不同的 option 名
if(DP_OSAL_STATIC_ALLOC)
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=1)
else()
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=0)
endif()
