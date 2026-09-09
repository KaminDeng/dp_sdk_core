#!/usr/bin/env python3
"""独立exec真实Host入口，验证旧Mock不支持等待时明确中止。

工程GTest可能裁剪死亡测试；不靠条件编译掉的EXPECT_DEATH声称通过。
"""
import resource
import signal
import subprocess
import sys


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    result = subprocess.run(
        [sys.argv[1], "--dp-osal-unsupported-chrono"],
        capture_output=True, text=True, timeout=5, check=False,
    )
    if result.returncode != -signal.SIGABRT or "OSAL_UNSUPPORTED_CHRONO_ENTERED" not in result.stderr:
        print(f"不支持的绝对等待未明确中止：returncode={result.returncode}", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return 1
    print("旧Mock等待默认实现：已执行实际入口并由SIGABRT中止")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
