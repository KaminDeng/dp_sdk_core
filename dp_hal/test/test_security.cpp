#ifndef TEST_SECURITY_CPP_
#define TEST_SECURITY_CPP_

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "dp_security.h"
}

namespace {

struct SecurityFixture {
    uint32_t random_value{0x11223344U};
    uint32_t counter_value{7U};
};

dp_security_status_t fill_random(void* context, uint8_t* output, size_t size) {
    auto* fixture = static_cast<SecurityFixture*>(context);
    if (fixture == nullptr || output == nullptr || size != sizeof(fixture->random_value)) {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    std::memcpy(output, &fixture->random_value, size);
    return DP_SECURITY_OK;
}

dp_security_status_t next_counter(void* context, uint32_t* output) {
    auto* fixture = static_cast<SecurityFixture*>(context);
    if (fixture == nullptr || output == nullptr) {
        return DP_SECURITY_ERR_INVALID_ARG;
    }
    *output = fixture->counter_value++;
    return DP_SECURITY_OK;
}

dp_security_status_t mac_stub(void*, uint8_t, uint8_t, const uint8_t*, size_t,
                              uint8_t output[DP_SECURITY_SHA256_SIZE]) {
    if (output != nullptr) {
        std::memset(output, 0xA5, DP_SECURITY_SHA256_SIZE);
    }
    return DP_SECURITY_OK;
}

dp_security_status_t verify_stub(void*, uint8_t, uint8_t, const uint8_t*, size_t,
                                 const uint8_t*, size_t) {
    return DP_SECURITY_OK;
}

dp_security_status_t load_epoch_stub(void*, uint32_t* epoch) {
    if (epoch != nullptr) *epoch = 1U;
    return DP_SECURITY_OK;
}

dp_security_status_t stage_key_stub(void*, uint16_t, uint8_t, uint32_t) {
    return DP_SECURITY_OK;
}

dp_security_status_t activate_key_stub(void*, uint8_t, uint32_t) {
    return DP_SECURITY_OK;
}

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

TEST(DpHalSecurity, PskProfileRequiresMacAndAtomicKeyLifecycle) {
    SecurityFixture fixture;
    dp_security_provider_t provider{};
    provider.context = &fixture;
    provider.boot_provider = DP_SECURITY_BOOT_WEAR_LEVELLED_COUNTER;
    provider.next_boot_counter = next_counter;
    EXPECT_EQ(dp_security_validate_provider(&provider, DP_SECURITY_LINK_PSK_MAC),
              DP_SECURITY_ERR_NOT_SUPPORTED);

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
