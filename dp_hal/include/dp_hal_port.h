/**
 * @file dp_hal_port.h
 * @brief HAL 平台端口的 C 接口：设备身份、微秒时间、调试输出和断言失败。
 * @details 通用 HAL 仅依赖这些声明；具体实现由所选端口的 dp_hal_port_impl.cpp 提供。
 * 产品端口应满足下面的稳定身份及上下文要求，主机 mock 用于测试，template 用于移植起步。
 * 声明本身不会提供硬件 UID、加锁或中断适配；评估实际平台能力时必须阅读对应实现。
 */

#ifndef DP_HAL_PORT_H_
#define DP_HAL_PORT_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 128 位设备身份的原始字节容器，不约定 UUID 文本格式或整数端序。 */
typedef struct {
    uint8_t bytes[16];  ///< 16 字节身份；成功读取时应整体非零，同一设备部署期内稳定。
} dp_hal_device_id_t;

/** @brief 设备身份读取结果；失败时调用者不能使用输出推断有效身份。 */
typedef enum {
    DP_HAL_DEVICE_ID_OK = 0,                 ///< 已写出端口提供的设备身份。
    DP_HAL_DEVICE_ID_INVALID_ARGUMENT = -1,  ///< 输出指针等参数无效。
    DP_HAL_DEVICE_ID_UNAVAILABLE = -2,       ///< 平台没有可供使用的身份来源。
} dp_hal_device_id_status_t;

/**
 * @brief 读取部署期间稳定、整体非零的 128 位设备身份。
 * @param[out] out 接收身份的有效对象，不能为 NULL。
 * @return OK 表示可使用输出；其余返回值表示参数错误或身份不可获得。
 * @note 该接口不证明身份不可伪造。主机 mock 返回所有实例共享的固定测试值，
 * 不能用于证明多块硬件唯一性；真正的身份来源及并发约束由产品端口实现。
 */
dp_hal_device_id_status_t dp_hal_port_get_device_id(dp_hal_device_id_t *out);

/**
 * @brief 获取平台单调递增的微秒计时值，用于计算时间差。
 * @return 单位为微秒的计时值；起点由端口决定，不是 Unix 时间戳。
 * @note 产品端口要求无需 OSAL 且可从 ISR 调用；计数扩展、回绕及读取原子性由端口负责。
 * 主机 mock 使用 steady_clock，不构成 MCU 中断安全性验证。
 */
uint64_t dp_hal_port_time_us(void);

/**
 * @brief 将调试文本交给所选端口的低层输出通道。
 * @param[in] buf 至少可读取 len 字节的文本缓冲区，调用期间保持有效。
 * @param len 请求输出的字节长度。
 * @note 产品端口要求支持 OSAL 初始化前及 ISR 场景，阻塞和缓冲策略由端口提供。
 * 主机 mock 在日志初始化后使用受长度限制的字符串格式化，遇到 NUL 会提前结束，
 * 因而不能把此接口当作保证逐字节传输的二进制通道。
 */
void dp_hal_port_log(const char *buf, size_t len);

/**
 * @brief 处理 HAL 断言失败并终止正常控制流，不允许返回调用点。
 * @param[in] expr 失败条件的字符串形式。
 * @param[in] file 触发断言的源文件名。
 * @param line 触发断言的源文件行号。
 * @note 主机端调用 abort；嵌入式端的停机、日志及复位策略由具体端口实现。
 */
DP_HAL_NORETURN void dp_hal_port_assert_fail(const char *expr, const char *file, int line);

#ifdef __cplusplus
}
#endif

/**
 * @brief HAL 运行时断言：条件为假时转入不可返回的端口失败处理。
 * @param cond 只求值一次的布尔条件；文本、文件名与行号用于诊断。
 * @note do/while 包装使宏可作为一条语句使用；本宏没有 NDEBUG 裁剪分支。
 */
#define DP_HAL_ASSERT(cond)                                            \
    do {                                                               \
        if (!(cond)) {                                                 \
            dp_hal_port_assert_fail(#cond, DP_HAL_FILENAME, __LINE__); \
        }                                                              \
    } while (0)

#endif  // DP_HAL_PORT_H_
