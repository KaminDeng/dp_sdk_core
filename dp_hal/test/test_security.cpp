/**
 * @file test_security.cpp
 * @brief 用内存替身验证启动身份创建和安全供应者能力检查的基本分支。
 * @details 由 HAL 单翻译单元入口包含。随机值固定，计数只在内存递增，MAC 与密钥回调
 * 仅作函数指针占位；这些测试不证明真实熵质量、认证正确性、持久单调性或掉电原子性。
 */
#ifndef TEST_SECURITY_CPP_
#define TEST_SECURITY_CPP_

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "dp_security.h"
}

namespace {

/** @brief 当前测试作用域内的可控回调上下文，无文件、Flash 或硬件安全模块。 */
struct SecurityFixture {
    uint32_t random_value{0x11223344U}; ///< memcpy 输出的固定 4 字节值，可设零覆盖错误分支。
    uint32_t counter_value{7U};        ///< 下次返回的内存计数，读取后自增；没有持久化。
};

/**
 * @brief 将夹具的固定整数按本机字节布局复制到恰好 4 字节的输出区。
 * @param context SecurityFixture 指针。
 * @param[out] output 有效输出缓冲区。
 * @param size 必须等于 random_value 的大小。
 * @return 参数不符返回 INVALID_ARG，否则 OK；这不是硬件随机源。
 */
dp_security_status_t fill_random(void* context, uint8_t* output, size_t size) {
    auto* fixture = static_cast<SecurityFixture*>(context);
    if (fixture == nullptr || output == nullptr || size != sizeof(fixture->random_value)) {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    std::memcpy(output, &fixture->random_value, size);
    return DP_SECURITY_OK;
}

/**
 * @brief 返回当前夹具计数后自增，用于观察连续两次身份创建是否调用了计数入口。
 * @param context SecurityFixture 指针。
 * @param[out] output 接收自增前的值。
 * @return 空上下文/输出返回 INVALID_ARG，否则 OK。
 * @note 无锁且无持久化，仅在本测试的同步调用中使用。
 */
dp_security_status_t next_counter(void* context, uint32_t* output) {
    auto* fixture = static_cast<SecurityFixture*>(context);
    if (fixture == nullptr || output == nullptr) {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    *output = fixture->counter_value++;
    return DP_SECURITY_OK;
}

/**
 * @brief MAC 能力占位，非空输出填满 32 个 0xA5，随后始终返回 OK。
 * @details 忽略上下文、密钥、方向和消息；PSK 测试只用其非空地址通过能力形状检查。
 * 空输出也返回 OK，因此不能作为生产 MAC 或参数校验实现。
 */
dp_security_status_t mac_stub(void*, uint8_t, uint8_t, const uint8_t*, size_t,
                              uint8_t output[DP_SECURITY_SHA256_SIZE]) {
    if (output != nullptr) {
        std::memset(output, 0xA5, DP_SECURITY_SHA256_SIZE);
    }
    return DP_SECURITY_OK;
}

/** @brief 无条件 OK 的认证能力占位，不读取消息或预期 MAC，不提供认证保证。 */
dp_security_status_t verify_stub(void*, uint8_t, uint8_t, const uint8_t*, size_t,
                                 const uint8_t*, size_t) {
    return DP_SECURITY_OK;
}

/** @brief 若输出非空则写固定 epoch=1，然后返回 OK；不读取持久密钥版本。 */
dp_security_status_t load_epoch_stub(void*, uint32_t* epoch) {
    if (epoch != nullptr) *epoch = 1U;
    return DP_SECURITY_OK;
}

/** @brief 暂存能力占位，始终 OK，不写存储、不验证槽号或掉电事务。 */
dp_security_status_t stage_key_stub(void*, uint16_t, uint8_t, uint32_t) {
    return DP_SECURITY_OK;
}

/** @brief 激活能力占位，始终 OK，不切换密钥、不执行防回滚检查。 */
dp_security_status_t activate_key_stub(void*, uint8_t, uint32_t) {
    return DP_SECURITY_OK;
}

/**
 * @brief 只有随机回调的 RNG 供应者可通过物理可信能力检查，并生成非零会话。
 * @details 输入是固定测试值，断言只覆盖配置及会话非零，不检测随机质量或会话唯一性。
 */
TEST(DpHalSecurity, HardwareRandomProviderCreatesNonzeroBootSession) {
    SecurityFixture fixture;
    dp_security_provider_t provider{};
    provider.context = &fixture;
    provider.boot_provider = DP_SECURITY_BOOT_HARDWARE_RNG;
    provider.random_fill = fill_random;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PHYSICAL_TRUSTED),
              DP_SECURITY_OK);
    uint32_t session = 0U;
    EXPECT_EQ(dp_security_create_boot_session(&provider, &session), DP_SECURITY_OK);
    EXPECT_NE(session, 0U);
}

/**
 * @brief 计数器模式可同时混合可选固定熵，连续调用写出计数 7、8 及不同会话。
 * @details 名称中的 PersistentCounter 指配置类别；实际替身仅修改内存，未验证断电持久化。
 */
TEST(DpHalSecurity, PersistentCounterProviderCanMixOptionalEntropy) {
    SecurityFixture fixture;
    dp_security_provider_t provider{};
    provider.context = &fixture;
    provider.boot_provider = DP_SECURITY_BOOT_SECURE_MONOTONIC_COUNTER;
    provider.random_fill = fill_random;
    provider.next_boot_counter = next_counter;
    uint32_t first = 0U;
    uint32_t second = 0U;
    uint32_t first_counter = 0U;
    uint32_t second_counter = 0U;
    ASSERT_EQ(dp_security_create_boot_identity(&provider, &first_counter, &first), DP_SECURITY_OK);
    ASSERT_EQ(dp_security_create_boot_identity(&provider, &second_counter, &second), DP_SECURITY_OK);
    EXPECT_EQ(first_counter, 7U);
    EXPECT_EQ(second_counter, 8U);
    EXPECT_NE(first, second);
}

/**
 * @brief PSK 配置先拒绝缺失能力，再接受齐备指针，最后拒绝切回 RNG-only 的来源。
 * @details 校验函数只检查指针而不调用 MAC、版本或密钥更新替身，
 * 因而这组断言不构成 MAC 正确性、掉电原子性或防回滚测试。
 */
TEST(DpHalSecurity, PskProfileRequiresMacAndAtomicKeyLifecycle) {
    SecurityFixture fixture;
    dp_security_provider_t provider{};
    provider.context = &fixture;
    provider.boot_provider = DP_SECURITY_BOOT_WEAR_LEVELLED_COUNTER;
    provider.next_boot_counter = next_counter;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PSK_MAC),
              DP_SECURITY_ERR_NOT_SUPPORTED);

    // 补齐的是能力入口的形状，不为这些恒成功替身赋予真实密码学或存储行为。
    provider.mac_sha256 = mac_stub;
    provider.verify_sha256 = verify_stub;
    provider.load_key_epoch = load_epoch_stub;
    provider.stage_key_slot_atomic = stage_key_stub;
    provider.activate_key_slot_atomic = activate_key_stub;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PSK_MAC), DP_SECURITY_OK);

    provider.boot_provider = DP_SECURITY_BOOT_HARDWARE_RNG;
    provider.random_fill = fill_random;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PSK_MAC),
              DP_SECURITY_ERR_NOT_SUPPORTED);
}

/**
 * @brief 零熵 RNG 创建失败且清零会话；随后计数器模式缺回调时能力检查失败。
 * @details 初始 session=123 用于证明失败路径没有保留旧会话，本测试未覆盖所有无效参数。
 */
TEST(DpHalSecurity, RejectsZeroOrIncompleteBootProviders) {
    SecurityFixture fixture;
    fixture.random_value = 0U;
    dp_security_provider_t provider{};
    provider.context = &fixture;
    provider.boot_provider = DP_SECURITY_BOOT_HARDWARE_RNG;
    provider.random_fill = fill_random;
    uint32_t session = 123U;
    EXPECT_EQ(dp_security_create_boot_session(&provider, &session), DP_SECURITY_ERR_ENTROPY);
    EXPECT_EQ(session, 0U);

    provider.boot_provider = DP_SECURITY_BOOT_SECURE_MONOTONIC_COUNTER;
    provider.random_fill = nullptr;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PHYSICAL_TRUSTED),
              DP_SECURITY_ERR_NOT_SUPPORTED);
}

}  // namespace

#endif  // TEST_SECURITY_CPP_
