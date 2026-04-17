/** @file   dp_hal_compat.h
 *  @brief  Compiler-portability macros for dp_hal.
 *
 *  Self-contained -- no external dependencies.
 *  Targets: GCC + Clang, Linux / macOS / ARM, 32-bit and 64-bit.
 */

#ifndef DP_HAL_COMPAT_H_
#define DP_HAL_COMPAT_H_

/* -- __has_attribute / __has_feature fallbacks ----------------------------- */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif

/* -- 1. Unused variable / parameter suppression --------------------------- */
#if __has_attribute(unused) || defined(__GNUC__)
#define DP_HAL_UNUSED __attribute__((unused))
#else
#define DP_HAL_UNUSED
#endif

/* -- 2. Force symbol retention (required for linker-section entries) ------- */
#if __has_attribute(used) || defined(__GNUC__)
#define DP_HAL_USED __attribute__((used))
#else
#define DP_HAL_USED
#endif

/* -- 3. Printf-format validation ----------------------------------------- */
#if __has_attribute(format) || defined(__GNUC__)
#define DP_HAL_PRINTF_LIKE(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define DP_HAL_PRINTF_LIKE(fmt_idx, args_idx)
#endif

/* -- 4. Weak symbol (overridable default implementation) ------------------ */
#if __has_attribute(weak) || defined(__GNUC__)
#define DP_HAL_WEAK __attribute__((weak))
#else
#define DP_HAL_WEAK
#endif

/* -- 5. ASan suppression (required for linker-section stride iteration) --- */
#if (defined(__has_feature) && __has_feature(address_sanitizer)) || defined(__SANITIZE_ADDRESS__)
#define DP_HAL_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#else
#define DP_HAL_NO_SANITIZE_ADDRESS
#endif

/* -- 6. Compile-time assertion (C / C++ unified) -------------------------- */
#ifdef __cplusplus
#define DP_HAL_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define DP_HAL_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* -- 7. restrict keyword (C / C++ unified) -------------------------------- */
#ifdef __cplusplus
#define DP_HAL_RESTRICT __restrict
#else
#define DP_HAL_RESTRICT restrict
#endif

/* -- 8. noreturn (C / C++ unified) ---------------------------------------- */
#ifdef __cplusplus
#define DP_HAL_NORETURN [[noreturn]]
#else
#define DP_HAL_NORETURN _Noreturn
#endif

/* -- 9. Diagnostic suppression (clang first -- clang also defines __GNUC__) */
#ifdef __clang__
#define DP_HAL_DIAG_PUSH _Pragma("clang diagnostic push")
#define DP_HAL_DIAG_POP _Pragma("clang diagnostic pop")
#define DP_HAL_DIAG_IGNORE(w) _Pragma(DP_HAL__STRINGIFY(clang diagnostic ignored w))
#elif defined(__GNUC__)
#define DP_HAL_DIAG_PUSH _Pragma("GCC diagnostic push")
#define DP_HAL_DIAG_POP _Pragma("GCC diagnostic pop")
#define DP_HAL_DIAG_IGNORE(w) _Pragma(DP_HAL__STRINGIFY(GCC diagnostic ignored w))
#else
#define DP_HAL_DIAG_PUSH
#define DP_HAL_DIAG_POP
#define DP_HAL_DIAG_IGNORE(w)
#endif
/** @cond INTERNAL */
#define DP_HAL__STRINGIFY(x) #x
/** @endcond */

/* -- 10. Linker section attribute (macOS vs Linux) ------------------------ */
/* macOS: __DATA,__sectionname   Linux: .sectionname                        */
#ifdef __APPLE__
#define DP_HAL_SECTION(name) __attribute__((section("__DATA," name)))
#else
#define DP_HAL_SECTION(name) __attribute__((section(name)))
#endif

/* -- 11. Inline (always safe, avoids C99 extern inline pitfall) ----------- */
#define DP_HAL_INLINE static inline

/* -- 12. Force inline ----------------------------------------------------- */
#if __has_attribute(always_inline) || defined(__GNUC__)
#define DP_HAL_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define DP_HAL_FORCE_INLINE static inline
#endif

/* -- 13. FILE basename (compile-time, no runtime cost on GCC/Clang) ------- */
#define DP_HAL_FILENAME (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#endif /* DP_HAL_COMPAT_H_ */
