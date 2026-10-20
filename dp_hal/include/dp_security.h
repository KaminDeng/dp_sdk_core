/**
 * @file dp_security.h
 * @brief 与平台无关的启动身份、安全链路能力及密钥槽回调契约。
 * @details 本文件定义供应者形状检查和 32 位启动会话混合，不实现随机源、MAC 算法或存储。
 * 上层先按链路配置校验回调齐备性，再通过供应者取得熵或启动计数。
 * 单调计数、掉电原子性、密钥保护及防回滚均是平台回调必须履行的责任，
 * 校验成功仅说明配置及必要函数指针存在，不能作为这些安全属性已验证的证据。
 */
#ifndef DP_SECURITY_H_
#define DP_SECURITY_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SHA-256 MAC 输出缓冲区长度，单位为字节（256 位）。 */
#define DP_SECURITY_SHA256_SIZE 32u

/** @brief 通用安全状态；回调失败可沿启动身份创建路径原样返回。 */
typedef enum {
    DP_SECURITY_OK = 0,                 ///< 当前操作成功。
    DP_SECURITY_ERR_INVALID_ARG = -1,   ///< 指针、枚举或其他参数不合法。
    DP_SECURITY_ERR_NOT_SUPPORTED = -2, ///< 供应者能力不齐备或配置不支持。
    DP_SECURITY_ERR_IO = -3,            ///< 平台读写等操作失败，由回调报告。
    DP_SECURITY_ERR_ENTROPY = -4,       ///< 随机源不可用或本次 RNG-only 熵为零。
    DP_SECURITY_ERR_AUTH = -5,          ///< 认证检查失败，由安全实现报告。
    DP_SECURITY_ERR_ROLLBACK = -6,      ///< 检测到版本/epoch 回退，由安全实现报告。
} dp_security_status_t;

/** @brief 启动身份的主要来源，枚举值不会自动选择或初始化硬件驱动。 */
typedef enum {
    DP_SECURITY_BOOT_HARDWARE_RNG = 1,             ///< 仅依赖硬件随机值，不提供持久启动计数。
    DP_SECURITY_BOOT_SECURE_MONOTONIC_COUNTER = 2, ///< 由平台安全单调计数器提供启动计数。
    DP_SECURITY_BOOT_WEAR_LEVELLED_COUNTER = 3,   ///< 由平台带磨损均衡的持久计数器提供启动计数。
} dp_security_boot_provider_t;

/** @brief 链路能力检查配置；物理可信的选择由部署前提保证。 */
typedef enum {
    DP_SECURITY_LINK_PHYSICAL_TRUSTED = 1, ///< 检查启动来源，不要求 MAC/密钥生命周期回调。
    DP_SECURITY_LINK_PSK_MAC = 2,          ///< 预共享密钥 MAC：要求持久计数及全部密钥回调。
    /* TLS 凭据由网络传输安全端口管理，本 lite-link 供应者始终返回 NOT_SUPPORTED。 */
    DP_SECURITY_LINK_MUTUAL_TLS = 3,
} dp_security_link_profile_t;

/**
 * @brief 随机源回调：向调用者缓冲区填充指定字节数。
 * @param context 供应者携带的借用上下文。
 * @param[out] output 至少容纳 size 字节的输出区。
 * @param size 请求的字节数；启动身份创建当前请求 sizeof(uint32_t) 字节。
 * @return OK 表示已填满；失败原因由平台返回，不能用固定伪随机值冒充硬件熵。
 */
typedef dp_security_status_t (*dp_security_random_fill_fn)(void *context,
                                                           uint8_t *output,
                                                           size_t size);
/**
 * @brief 原子持久化并返回严格递增的启动计数。
 * @param context 平台计数器上下文。
 * @param[out] counter 接收本次启动计数的对象。
 * @return OK 或平台错误；本层不自行检测回调返回的计数是否为零、重复或回退。
 * @note 磨损均衡实现只应在日志写满后擦除；掉电恢复及溢出策略必须由平台定义。
 */
typedef dp_security_status_t (*dp_security_next_boot_counter_fn)(void *context,
                                                                 uint32_t *counter);
/**
 * @brief 使用受保护密钥计算 SHA-256 MAC，原始密钥不通过此接口传出。
 * @param context 平台密钥存储/算法上下文。
 * @param key_id 协议侧密钥标识。
 * @param direction 链路方向域；其编码由调用者与平台实现共同约定。
 * @param[in] data 待认证的消息缓冲区。
 * @param size 消息字节数。
 * @param[out] output 接收 DP_SECURITY_SHA256_SIZE 字节的结果。
 * @return OK 或算法/存储错误，具体 MAC 算法及域分离由实现保证。
 */
typedef dp_security_status_t (*dp_security_mac_sha256_fn)(
    void *context, uint8_t key_id, uint8_t direction,
    const uint8_t *data, size_t size,
    uint8_t output[DP_SECURITY_SHA256_SIZE]);
/**
 * @brief 校验消息的预期 MAC，由平台实现认证比较及错误报告。
 * @param context 平台安全上下文。
 * @param key_id 协议侧密钥标识。
 * @param direction 与生成 MAC 时一致的方向域。
 * @param[in] data 待认证消息。
 * @param size 消息字节数。
 * @param[in] expected 预期认证值缓冲区。
 * @param expected_size 预期认证值的字节数，长度合法性由回调检查。
 * @return OK 表示认证通过，否则返回认证或平台错误。
 */
typedef dp_security_status_t (*dp_security_verify_sha256_fn)(
    void *context, uint8_t key_id, uint8_t direction,
    const uint8_t *data, size_t size,
    const uint8_t *expected, size_t expected_size);
/**
 * @brief 读取当前密钥生命周期版本，以便上层执行更新及防回滚策略。
 * @param context 平台密钥元数据上下文。
 * @param[out] epoch 接收当前版本号的有效对象。
 * @return OK 或平台读取错误。
 */
typedef dp_security_status_t (*dp_security_load_key_epoch_fn)(void *context,
                                                              uint32_t *epoch);
/**
 * @brief 原子暂存待切换密钥槽与目标版本，掉电后必须保持可判定状态。
 * @param context 平台受保护存储上下文。
 * @param key_slot_id 已配置的受保护存储槽编号，不是原始密钥。
 * @param key_id 上层使用的密钥标识。
 * @param epoch 要暂存的生命周期版本。
 * @return OK 或平台错误；此层不实现事务、持久化或版本检查。
 */
typedef dp_security_status_t (*dp_security_stage_key_slot_fn)(
    void *context, uint16_t key_slot_id, uint8_t key_id, uint32_t epoch);
/**
 * @brief 原子激活先前暂存的密钥版本，保证掉电后恢复状态一致。
 * @param context 平台受保护存储上下文。
 * @param key_id 要激活的协议密钥标识。
 * @param epoch 目标生命周期版本。
 * @return OK 或平台错误，匹配暂存状态及防回滚检查由回调负责。
 */
typedef dp_security_status_t (*dp_security_activate_key_slot_fn)(
    void *context, uint8_t key_id, uint32_t epoch);

/**
 * @brief 平台能力表，函数指针及上下文均借用，创建身份时同步调用。
 * @note 调用者保证表及 context 有效；本结构不转移所有权、不加锁，也不提供存储实现。
 */
typedef struct {
    void *context;                                           ///< 原样传给所有回调的借用上下文。
    dp_security_boot_provider_t boot_provider;                ///< 选择必需随机源或计数器回调。
    dp_security_random_fill_fn random_fill;                   ///< RNG 模式必需，计数器模式可选混合熵。
    dp_security_next_boot_counter_fn next_boot_counter;       ///< 两种计数器模式必需，回调负责持久递增。
    dp_security_mac_sha256_fn mac_sha256;                     ///< PSK 配置所需的 MAC 生成能力。
    dp_security_verify_sha256_fn verify_sha256;               ///< PSK 配置所需的 MAC 校验能力。
    dp_security_load_key_epoch_fn load_key_epoch;             ///< PSK 配置所需的密钥版本读取能力。
    dp_security_stage_key_slot_fn stage_key_slot_atomic;      ///< PSK 配置所需的掉电原子暂存能力。
    dp_security_activate_key_slot_fn activate_key_slot_atomic; ///< PSK 配置所需的掉电原子激活能力。
} dp_security_provider_t;

/**
 * @brief 只检查指定链路配置所需的供应者类别与函数指针。
 * @param[in] provider 平台能力表，不能为 NULL。
 * @param profile 物理可信、PSK MAC 或 mutual TLS 配置。
 * @retval DP_SECURITY_OK 必要能力入口齐备；函数不会调用这些回调。
 * @retval DP_SECURITY_ERR_INVALID_ARG 空表、未知配置或未知启动来源。
 * @retval DP_SECURITY_ERR_NOT_SUPPORTED TLS、不完整能力或 PSK 使用 RNG-only 来源。
 * @details 先检查参数，再拒绝此层不承载的 TLS，然后检查启动来源，最后检查 PSK 能力。
 * 这里不验证设备是否物理可信、随机质量、MAC 正确性或持久存储的原子性。
 */
static inline dp_security_status_t dp_security_validate_provider(
    const dp_security_provider_t *provider, dp_security_link_profile_t profile) {
    if (provider == NULL ||
        (profile != DP_SECURITY_LINK_PHYSICAL_TRUSTED &&
         profile != DP_SECURITY_LINK_PSK_MAC && profile != DP_SECURITY_LINK_MUTUAL_TLS)) {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    if (profile == DP_SECURITY_LINK_MUTUAL_TLS) return DP_SECURITY_ERR_NOT_SUPPORTED;
    if (provider->boot_provider == DP_SECURITY_BOOT_HARDWARE_RNG) {
        if (provider->random_fill == NULL) return DP_SECURITY_ERR_NOT_SUPPORTED;
    } else if (provider->boot_provider == DP_SECURITY_BOOT_SECURE_MONOTONIC_COUNTER ||
               provider->boot_provider == DP_SECURITY_BOOT_WEAR_LEVELLED_COUNTER) {
        if (provider->next_boot_counter == NULL) return DP_SECURITY_ERR_NOT_SUPPORTED;
    } else {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    // PSK 除认证与密钥更新入口外还要求计数器来源，避免只靠随机值宣称防重放。
    if (profile == DP_SECURITY_LINK_PSK_MAC &&
        (provider->boot_provider == DP_SECURITY_BOOT_HARDWARE_RNG ||
         provider->mac_sha256 == NULL || provider->verify_sha256 == NULL ||
         provider->load_key_epoch == NULL || provider->stage_key_slot_atomic == NULL ||
         provider->activate_key_slot_atomic == NULL)) {
        return DP_SECURITY_ERR_NOT_SUPPORTED;
    }
    return DP_SECURITY_OK;
}

/**
 * @brief 通过移位、异或及无符号乘法扩散 32 位会话种子。
 * @param value 要混合的 32 位值。
 * @return 混合后的 32 位值，可能为零。
 * @note 无符号运算按 2^32 回绕；这是确定性整数混合，不是加密散列或认证算法。
 */
static inline uint32_t dp_security_mix_session(uint32_t value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

/**
 * @brief 同步取得启动计数及非零会话 ID，供上层标识本次启动。
 * @param[in] provider 有效且满足物理可信配置的能力表。
 * @param[out] boot_counter 启动计数输出，RNG-only 模式成功时为零。
 * @param[out] session_id 32 位非零会话标识输出，与 boot_counter 使用不同对象。
 * @return OK 或参数/能力/熵错误；随机源及计数器失败原样传播。
 * @details 先验证两个输出指针并置零，再检查能力表。若配置了随机回调，
 * 即使处于计数器模式也先调用它，失败立即返回；随后在计数器模式调用计数回调。
 * RNG-only 的全零熵会失败，计数器模式没有额外的零值/回退检查。
 * 最后混合熵、计数及常量，并将零结果改为 1，再写出计数。
 * @note 有效且互不重叠的输出在后续失败时保留为零；无效输出指针路径不清空另一个输出。
 * 非零会话不保证无碰撞或不可预测；持久计数器回调可能已经更新存储，本函数不回滚它。
 */
static inline dp_security_status_t dp_security_create_boot_identity(
    const dp_security_provider_t *provider, uint32_t *boot_counter,
    uint32_t *session_id) {
    uint32_t entropy = 0u;  // 可选的 4 字节平台熵；字节解释沿用本机 uint32_t 布局。
    uint32_t counter = 0u;  // RNG-only 保持零；计数器模式由回调写入。
    dp_security_status_t status;
    if (boot_counter == NULL || session_id == NULL) return DP_SECURITY_ERR_INVALID_ARG;
    *boot_counter = 0u;
    *session_id = 0u;
    status = dp_security_validate_provider(provider, DP_SECURITY_LINK_PHYSICAL_TRUSTED);
    if (status != DP_SECURITY_OK) return status;
    if (provider->random_fill != NULL) {
        status = provider->random_fill(provider->context, (uint8_t *)&entropy, sizeof(entropy));
        if (status != DP_SECURITY_OK) return status;
    }
    if (provider->boot_provider != DP_SECURITY_BOOT_HARDWARE_RNG) {
        status = provider->next_boot_counter(provider->context, &counter);
        if (status != DP_SECURITY_OK) return status;
    }
    if (provider->boot_provider == DP_SECURITY_BOOT_HARDWARE_RNG && entropy == 0u) {
        return DP_SECURITY_ERR_ENTROPY;
    }
    // 非密码学混合只生成会话标识；协议认证仍须独立使用平台安全能力。
    *session_id = dp_security_mix_session(entropy ^ counter ^ 0x9e3779b9u);
    if (*session_id == 0u) *session_id = 1u;
    *boot_counter = counter;
    return DP_SECURITY_OK;
}

/**
 * @brief 仅需要会话 ID 时的兼容包装，内部仍完成一次完整启动身份创建。
 * @param[in] provider 启动身份供应者。
 * @param[out] session_id 接收非零会话 ID 的有效对象。
 * @return 原样返回 dp_security_create_boot_identity 的状态。
 * @note 局部计数结果被丢弃，但计数器回调仍会被调用并可能持久递增。
 */
static inline dp_security_status_t dp_security_create_boot_session(
    const dp_security_provider_t *provider, uint32_t *session_id) {
    uint32_t unused_counter = 0u;
    return dp_security_create_boot_identity(provider, &unused_counter, session_id);
}

#ifdef __cplusplus
}
#endif

#endif  // DP_SECURITY_H_
