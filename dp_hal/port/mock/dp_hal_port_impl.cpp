/**
 * @file dp_hal_port_impl.cpp
 * @brief HAL 主机 mock 端口：固定身份、steady_clock 计时、日志转发及 abort 断言。
 * @details 用于主机测试运行通用 HAL 逻辑，不模拟 MCU 唯一身份或中断时序。
 * 日志初始化后走 dp_log，之前走 dp_console；没有额外的端口互斥或中断保护。
 */

#include "dp_hal_port.h"

#include <chrono>  // NOLINT(build/c++11)
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "dp_console.h"
#include "dp_log.h"

/**
 * @brief 写出固定的 MOCK 测试身份，便于验证重复读取稳定且整体非零。
 * @param[out] out 有效的 16 字节身份对象。
 * @return 空指针返回 INVALID_ARGUMENT，否则复制固定值并返回 OK。
 * @note 所有使用此端口的实例得到相同值，不代表主机或机械臂硬件的唯一身份。
 */
dp_hal_device_id_status_t dp_hal_port_get_device_id(dp_hal_device_id_t *out) {
    if (out == nullptr) return DP_HAL_DEVICE_ID_INVALID_ARGUMENT;
    // 前四字节为 ASCII MOCK，末字节为 1；中间零字节不会影响“整体非零”。
    static constexpr uint8_t kMockId[16] = {
        0x4DU, 0x4FU, 0x43U, 0x4BU, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U,
    };
    std::memcpy(out->bytes, kMockId, sizeof(kMockId));
    return DP_HAL_DEVICE_ID_OK;
}

/* ---- dp_hal_port_time_us ------------------------------------------------ */
/**
 * @brief 返回 steady_clock 的单调微秒值，供主机测试计算经过时间。
 * @return 时钟自身 epoch 以来的微秒数，不用于日历或跨进程起点约定。
 */
uint64_t dp_hal_port_time_us(void) {
    using namespace std::chrono;  // NOLINT(build/namespaces)
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(duration_cast<microseconds>(now).count());
}

/* ---- dp_hal_port_log ---------------------------------------------------- */
/** @brief 第一次经 dp_log 输出时延迟注册的 HAL 标签；本文件不另加并发保护。 */
static dp_log_tag_t s_tag_hal = DP_LOG_INVALID_TAG;

/**
 * @brief 把借用的文本缓冲区转发给已初始化日志系统，或回退到控制台。
 * @param[in] buf 调用期间可读的文本缓冲区。
 * @param len 请求输出字节数；日志分支转为 int 作为字符串精度，调用者应限制在其范围内。
 * @note %.*s 在 NUL 处结束，不保证输出 len 个二进制字节；延迟标签注册没有本地锁。
 * 主机代码不提供 MCU ISR 安全性证明，实际线程与输出行为取决于后端。
 */
void dp_hal_port_log(const char *buf, size_t len) {
    if (dp_log_is_initialized()) {
        if (s_tag_hal == DP_LOG_INVALID_TAG) {
            s_tag_hal = dp_log_tag_register("HAL");
        }
        dp_log_write(DP_LOG_DEBUG, s_tag_hal, NULL, 0, "%.*s", static_cast<int>(len), buf);
    } else {
        dp_console_write(buf, len);
    }
}

/* ---- dp_hal_port_assert_fail -------------------------------------------- */
/**
 * @brief 向 stderr 打印断言位置后调用 abort，不返回调用点。
 * @param[in] expr 失败表达式文本。
 * @param[in] file 源文件名。
 * @param line 源文件行号。
 * @note abort 的信号/转储行为由宿主运行环境决定，断言失败不会作为普通错误返回。
 */
void dp_hal_port_assert_fail(const char *expr, const char *file, int line) {
    std::fprintf(stderr, "DP_HAL_ASSERT failed: %s [%s:%d]\n", expr, file, line);
    std::abort();
}
