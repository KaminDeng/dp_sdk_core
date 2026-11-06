/**
 * @file dp_hal_port.h
 * @brief HAL 平台端口的 C 接口：设备与启动身份、微秒时间、调试输出和断言失败。
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

/** @name 可选的平台启动身份能力
 * @details 启动身份在本次启动内保持不变，且同设备的新启动不得重用旧值。
 * 端口以持久计数或其它可靠来源初始化；不能用32位tick、UART会话号或设备UID代替。
 * 未声明能力的既有端口明确返回UNAVAILABLE，不要求补充实现。
 * @{
 */
/** @brief 启动身份初始化结果；只有OK时可使用输出。 */
typedef enum {
    DP_HAL_BOOT_IDENTITY_OK = 0,                 ///< 已取得本次启动的非零身份。
    DP_HAL_BOOT_IDENTITY_INVALID_ARGUMENT = -1,  ///< 输出指针为空。
    DP_HAL_BOOT_IDENTITY_UNAVAILABLE = -2,       ///< 所选端口不支持或身份尚未就绪。
} dp_hal_boot_identity_status_t;

#if defined(DP_HAL_PORT_HAS_BOOT_IDENTITY) && DP_HAL_PORT_HAS_BOOT_IDENTITY
/** @brief 已选端口的幂等初始化叶函数；公共调用方使用dp_hal_init_boot_identity。
 * @param[out] out 成功时写出非零64位身份，失败时保持原值。
 * @return 参数/可用性结果；可在首次调用持久化启动计数，不创建通信会话。
 */
dp_hal_boot_identity_status_t dp_hal_port_init_boot_identity(uint64_t *out);
#endif

/** @brief 查询所选端口是否声明启动身份能力，不保证初始化已经成功。
 * @return 1表示有正式端口实现，0表示本次配置不支持。
 */
static inline int dp_hal_has_boot_identity(void) {
#if defined(DP_HAL_PORT_HAS_BOOT_IDENTITY) && DP_HAL_PORT_HAS_BOOT_IDENTITY
    return 1;
#else
    return 0;
#endif
}

/** @brief 幂等初始化并取得本设备本次启动身份；它不是时间戳、认证或使能许可。
 * @param[out] out 调用期间有效的64位输出，不能为NULL；失败保持原值。
 * @return OK、INVALID_ARGUMENT或UNAVAILABLE，不静默生成替代身份。
 * @note 必须在业务消息发布前由单一线程串行调用，不适用于ISR或控制热路径。
 * 首次调用可能执行持久化及有界Flash等待；重复成功调用返回本次启动缓存而不递增。
 * 端口可缓存首次失败，此时本次启动后续调用仍返回UNAVAILABLE，不能假定会重试Flash。
 * 无堆分配；不支持能力及空指针分支没有初始化副作用。
 */
static inline dp_hal_boot_identity_status_t dp_hal_init_boot_identity(uint64_t *out) {
    if (out == 0) return DP_HAL_BOOT_IDENTITY_INVALID_ARGUMENT;
#if defined(DP_HAL_PORT_HAS_BOOT_IDENTITY) && DP_HAL_PORT_HAS_BOOT_IDENTITY
    return dp_hal_port_init_boot_identity(out);
#else
    return DP_HAL_BOOT_IDENTITY_UNAVAILABLE;
#endif
}

/** @} */

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
 * 主机端直接写 stderr，保留 NUL；MCU 端直接使用底层 UART。
 * 接口没有错误返回或持久化承诺，不能将其当作可靠文件传输通道。
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
