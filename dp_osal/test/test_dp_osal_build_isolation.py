"""Core由独立父项目消费：生产OSAL不依赖GTest，HAL测试不借用通信库入口。"""
from pathlib import Path
import subprocess

CORE = Path(__file__).resolve().parents[2]


def run(*arguments):
    result = subprocess.run([str(value) for value in arguments], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_production_osal_without_test_framework_builds_real_thread_and_mutex(tmp_path):
    (tmp_path/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(independent_osal LANGUAGES C CXX)
function(dpsdk_library)
endfunction()
add_subdirectory("{CORE}/dp_osal" osal)
get_target_property(dependencies dp_osal INTERFACE_LINK_LIBRARIES)
get_target_property(includes dp_osal INTERFACE_INCLUDE_DIRECTORIES)
if("${{dependencies}}" MATCHES "gtest" OR "${{includes}}" MATCHES "/test")
 message(FATAL_ERROR "生产OSAL暴露测试依赖或测试头")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE dp_osal)
''')
    (tmp_path/'main.cpp').write_text('''#include "dp_osal_mutex.h"
#include "dp_osal_thread.h"
int main(){dp::osal::Mutex mutex;if(!mutex.tryLockForMs(1))return 1;
 if(!mutex.unlock())return 2;
 dp::osal::Thread thread;if(!thread.start([](void*){}).ok())return 3;
 return thread.join().ok()?0:4;}
''')
    run('cmake','-S',tmp_path,'-B',tmp_path/'build')
    run('cmake','--build',tmp_path/'build','--target','consumer','--parallel','22')
    run(tmp_path/'build/consumer')
