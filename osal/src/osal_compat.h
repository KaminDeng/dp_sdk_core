/** @file   osal_compat.h
 *  @brief  Compiler-portability macros for OSAL.
 *
 *  Self-contained -- no external dependencies.
 *  Targets: GCC + Clang, Linux / macOS / ARM, 32-bit and 64-bit.
 */

#ifndef OSAL_COMPAT_H_
#define OSAL_COMPAT_H_

/* -- __has_attribute / __has_feature fallbacks ----------------------------- */
#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

/* -- 1. Unused variable / parameter suppression --------------------------- */
#if __has_attribute(unused) || defined(__GNUC__)
#  define OSAL_UNUSED __attribute__((unused))
#else
#  define OSAL_UNUSED
#endif

/* -- 2. Force symbol retention (required for linker-section entries) ------ */
#if __has_attribute(used) || defined(__GNUC__)
#  define OSAL_USED __attribute__((used))
#else
#  define OSAL_USED
#endif

/* -- 3. Printf-format validation ----------------------------------------- */
#if __has_attribute(format) || defined(__GNUC__)
#  define OSAL_PRINTF_LIKE(fmt_idx, args_idx) \
     __attribute__((format(printf, fmt_idx, args_idx)))
#else
#  define OSAL_PRINTF_LIKE(fmt_idx, args_idx)
#endif

/* -- 4. Weak symbol (overridable default implementation) ------------------ */
#if __has_attribute(weak) || defined(__GNUC__)
#  define OSAL_WEAK __attribute__((weak))
#else
#  define OSAL_WEAK
#endif

/* -- 5. ASan suppression -------------------------------------------------- */
#if (defined(__has_feature) && __has_feature(address_sanitizer)) || \
     defined(__SANITIZE_ADDRESS__)
#  define OSAL_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#else
#  define OSAL_NO_SANITIZE_ADDRESS
#endif

/* -- 6. Compile-time assertion (C / C++ unified) -------------------------- */
#ifdef __cplusplus
#  define OSAL_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#  define OSAL_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* -- 7. restrict keyword (C / C++ unified) -------------------------------- */
#ifdef __cplusplus
#  define OSAL_RESTRICT __restrict
#else
#  define OSAL_RESTRICT restrict
#endif

/* -- 8. noreturn (C / C++ unified) ---------------------------------------- */
#ifdef __cplusplus
#  define OSAL_NORETURN [[noreturn]]
#else
#  define OSAL_NORETURN _Noreturn
#endif

/* -- 9. Diagnostic suppression -------------------------------------------- */
#ifdef __clang__
#  define OSAL_DIAG_PUSH      _Pragma("clang diagnostic push")
#  define OSAL_DIAG_POP       _Pragma("clang diagnostic pop")
#  define OSAL_DIAG_IGNORE(w) _Pragma(OSAL__STRINGIFY_(clang diagnostic ignored w))
#elif defined(__GNUC__)
#  define OSAL_DIAG_PUSH      _Pragma("GCC diagnostic push")
#  define OSAL_DIAG_POP       _Pragma("GCC diagnostic pop")
#  define OSAL_DIAG_IGNORE(w) _Pragma(OSAL__STRINGIFY_(GCC diagnostic ignored w))
#else
#  define OSAL_DIAG_PUSH
#  define OSAL_DIAG_POP
#  define OSAL_DIAG_IGNORE(w)
#endif
/** @cond INTERNAL */
#define OSAL__STRINGIFY_(x) #x
/** @endcond */

/* -- 10. Linker section attribute (macOS vs Linux/ARM) -------------------- */
#ifdef __APPLE__
#  define OSAL_SECTION(name) __attribute__((section("__DATA," name)))
#else
#  define OSAL_SECTION(name) __attribute__((section(name)))
#endif

/* -- 11. Inline (always safe) --------------------------------------------- */
#define OSAL_INLINE static inline

/* -- 12. Force inline ----------------------------------------------------- */
#if __has_attribute(always_inline) || defined(__GNUC__)
#  define OSAL_FORCE_INLINE static inline __attribute__((always_inline))
#else
#  define OSAL_FORCE_INLINE static inline
#endif

/* -- 13. FILE basename ---------------------------------------------------- */
#define OSAL_FILENAME \
    (__builtin_strrchr(__FILE__, '/') \
     ? __builtin_strrchr(__FILE__, '/') + 1 \
     : __FILE__)

#endif /* OSAL_COMPAT_H_ */
