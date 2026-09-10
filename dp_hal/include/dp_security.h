/** @file dp_security.h
 *  @brief Platform-neutral entropy, boot-session and key-slot provider contract. */
#ifndef DP_SECURITY_H_
#define DP_SECURITY_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DP_SECURITY_SHA256_SIZE 32u

typedef enum {
    DP_SECURITY_OK = 0,
    DP_SECURITY_ERR_INVALID_ARG = -1,
    DP_SECURITY_ERR_NOT_SUPPORTED = -2,
    DP_SECURITY_ERR_IO = -3,
    DP_SECURITY_ERR_ENTROPY = -4,
    DP_SECURITY_ERR_AUTH = -5,
    DP_SECURITY_ERR_ROLLBACK = -6,
} dp_security_status_t;

typedef enum {
    DP_SECURITY_BOOT_HARDWARE_RNG = 1,
    DP_SECURITY_BOOT_SECURE_MONOTONIC_COUNTER = 2,
    DP_SECURITY_BOOT_WEAR_LEVELLED_COUNTER = 3,
} dp_security_boot_provider_t;

typedef enum {
    DP_SECURITY_LINK_PHYSICAL_TRUSTED = 1,
    DP_SECURITY_LINK_PSK_MAC = 2,
    /* TLS credentials are owned by the network transport security port, not
     * by this lite-link provider. Validation returns NOT_SUPPORTED here. */
    DP_SECURITY_LINK_MUTUAL_TLS = 3,
} dp_security_link_profile_t;

typedef dp_security_status_t (*dp_security_random_fill_fn)(void *context,
                                                           uint8_t *output,
                                                           size_t size);
/* Must atomically persist and return a strictly increasing value. A
 * wear-levelled implementation may erase only after its journal is full. */
typedef dp_security_status_t (*dp_security_next_boot_counter_fn)(void *context,
                                                                 uint32_t *counter);
typedef dp_security_status_t (*dp_security_mac_sha256_fn)(
    void *context, uint8_t key_id, uint8_t direction,
    const uint8_t *data, size_t size,
    uint8_t output[DP_SECURITY_SHA256_SIZE]);
typedef dp_security_status_t (*dp_security_verify_sha256_fn)(
    void *context, uint8_t key_id, uint8_t direction,
    const uint8_t *data, size_t size,
    const uint8_t *expected, size_t expected_size);
typedef dp_security_status_t (*dp_security_load_key_epoch_fn)(void *context,
                                                              uint32_t *epoch);
/* stage and activate must be power-loss atomic. key_slot_id refers to
 * provisioned protected storage and never carries raw key material. */
typedef dp_security_status_t (*dp_security_stage_key_slot_fn)(
    void *context, uint16_t key_slot_id, uint8_t key_id, uint32_t epoch);
typedef dp_security_status_t (*dp_security_activate_key_slot_fn)(
    void *context, uint8_t key_id, uint32_t epoch);

typedef struct {
    void *context;
    dp_security_boot_provider_t boot_provider;
    dp_security_random_fill_fn random_fill;
    dp_security_next_boot_counter_fn next_boot_counter;
    dp_security_mac_sha256_fn mac_sha256;
    dp_security_verify_sha256_fn verify_sha256;
    dp_security_load_key_epoch_fn load_key_epoch;
    dp_security_stage_key_slot_fn stage_key_slot_atomic;
    dp_security_activate_key_slot_fn activate_key_slot_atomic;
} dp_security_provider_t;

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
    if (profile == DP_SECURITY_LINK_PSK_MAC &&
        (provider->boot_provider == DP_SECURITY_BOOT_HARDWARE_RNG ||
         provider->mac_sha256 == NULL || provider->verify_sha256 == NULL ||
         provider->load_key_epoch == NULL || provider->stage_key_slot_atomic == NULL ||
         provider->activate_key_slot_atomic == NULL)) {
        return DP_SECURITY_ERR_NOT_SUPPORTED;
    }
    return DP_SECURITY_OK;
}

static inline uint32_t dp_security_mix_session(uint32_t value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

static inline dp_security_status_t dp_security_create_boot_identity(
    const dp_security_provider_t *provider, uint32_t *boot_counter,
    uint32_t *session_id) {
    uint32_t entropy = 0u;
    uint32_t counter = 0u;
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
    *session_id = dp_security_mix_session(entropy ^ counter ^ 0x9e3779b9u);
    if (*session_id == 0u) *session_id = 1u;
    *boot_counter = counter;
    return DP_SECURITY_OK;
}

static inline dp_security_status_t dp_security_create_boot_session(
    const dp_security_provider_t *provider, uint32_t *session_id) {
    uint32_t unused_counter = 0u;
    return dp_security_create_boot_identity(provider, &unused_counter, session_id);
}

#ifdef __cplusplus
}
#endif

#endif  // DP_SECURITY_H_
