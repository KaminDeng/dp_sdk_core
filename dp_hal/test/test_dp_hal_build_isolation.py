"""HAL测试实际独立消费，禁止借父工程通信库入口。"""
from pathlib import Path
import subprocess
CORE = Path(__file__).resolve().parents[2]
def run(*arguments):
    result = subprocess.run([str(value) for value in arguments], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_can_fd_contract_target_runs_from_independent_parent(tmp_path):
    gtest=CORE.parents[1]/'thirdparty/googletest'
    assert gtest.is_dir(), '此回归需要显式外部GTest源码依赖'
    (tmp_path/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(independent_hal_test LANGUAGES C CXX)
function(dpsdk_library)
endfunction()
enable_testing()
set(BUILD_TESTING ON)
set(DP_HAL_BUILD_CAN_FD_TESTS ON CACHE BOOL "HAL合同")
add_subdirectory("{gtest}" gtest)
add_subdirectory("{CORE}/dp_hal" hal)
''')
    run('cmake','-S',tmp_path,'-B',tmp_path/'build')
    run('cmake','--build',tmp_path/'build','--target','dp_hal_can_fd_contract_tests','--parallel','22')
    run('ctest','--test-dir',tmp_path/'build','--output-on-failure','-R','dp_hal.can_fd')
