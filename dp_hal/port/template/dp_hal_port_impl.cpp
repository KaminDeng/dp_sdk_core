/**
 * @file dp_hal_port_impl.cpp
 * @brief HAL 移植模板，复制到目标端口目录后按平台能力实现各入口。
 * @details 每个入口前的 `\#error` 故意阻止模板成为可运行后端；只有完成对应实现并验证契约
 * 后才能在新端口中移除哨兵。本模板中的 UNAVAILABLE、零时间、空日志和死循环只是占位。
 * 注释阅读不等于移植完成，也不能仅删除 `\#error` 就用于机械臂产品。
 */

#include "dp_hal_port.h"

#include <stddef.h>
#include <stdint.h>

/* ---- dp_hal_port_get_device_id ----------------------------------------- */
#error "Implement dp_hal_port_get_device_id() for your platform"
/**
 * @brief 身份读取占位：忽略输出并报告不可用。
 * @param[out] out 模板不会写入的身份对象。
 * @return 始终为 UNAVAILABLE；实际端口需实现稳定且整体非零的身份来源。
 */
dp_hal_device_id_status_t dp_hal_port_get_device_id(dp_hal_device_id_t *out) {
    (void)out;
    return DP_HAL_DEVICE_ID_UNAVAILABLE;
}

/* ---- dp_hal_port_time_us ------------------------------------------------ */
#error "Implement dp_hal_port_time_us() for your platform"
/**
 * @brief 时间占位入口，实际端口应提供可从 ISR 读取的单调微秒计时。
 * @return 模板恒为零，不能用于超时或性能测量。
 */
uint64_t dp_hal_port_time_us(void) { return 0; }

/* ---- dp_hal_port_log ---------------------------------------------------- */
#error "Implement dp_hal_port_log() for your platform"
/**
 * @brief 日志占位入口，不产生输出。
 * @param[in] buf 待输出文本，模板只消除未使用参数告警。
 * @param len 文本字节数，模板不会消费。
 * @note 移植实现须明确初始化前、ISR 调用、阻塞及缓冲策略。
 */
void dp_hal_port_log(const char *buf, size_t len) {
    (void)buf;
    (void)len;
}

/* ---- dp_hal_port_assert_fail -------------------------------------------- */
#error "Implement dp_hal_port_assert_fail() for your platform"
/**
 * @brief 断言失败占位：在无限循环中停留，保证不返回。
 * @param[in] expr 失败表达式，模板不输出。
 * @param[in] file 源文件名，模板不输出。
 * @param line 源文件行号，模板不输出。
 * @note 产品实现需按故障处置策略记录或停机；此循环本身不失能电机、不控制看门狗。
 */
void dp_hal_port_assert_fail(const char *expr, const char *file, int line) {
    (void)expr;
    (void)file;
    (void)line;
    while (1) {
    }
}
