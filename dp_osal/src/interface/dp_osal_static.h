/** @file osal_static.h
 *  @brief OSAL static allocation helpers for MCU firmware builds. */
#ifndef DP_OSAL_STATIC_H_
#define DP_OSAL_STATIC_H_

#include <new>
#include <type_traits>

#ifndef DP_OSAL_ENABLE_STATIC_ALLOC
#define DP_OSAL_ENABLE_STATIC_ALLOC 0
#endif

/** @brief Define a statically allocated OSAL object.
 *
 *  In static-alloc mode, the object is placement-new'ed into static storage.
 *  Otherwise, this expands to a regular local object definition.
 */
#if DP_OSAL_ENABLE_STATIC_ALLOC
#define DP_OSAL_STATIC_OBJECT_DEFINE(type_, name_, ...)                           \
    alignas(type_) unsigned char _osal_static_buf_##name_[sizeof(type_)];      \
    type_ &name_ = *new (_osal_static_buf_##name_) type_(__VA_ARGS__)
#else
#define DP_OSAL_STATIC_OBJECT_DEFINE(type_, name_, ...) type_ name_(__VA_ARGS__)
#endif

/* Convenience aliases for frequently used primitives. */
#define DP_OSAL_MUTEX_DEFINE(name_) DP_OSAL_STATIC_OBJECT_DEFINE(::dp::osal::Mutex, name_)
#define DP_OSAL_SEMAPHORE_DEFINE(name_, ...) DP_OSAL_STATIC_OBJECT_DEFINE(::dp::osal::Semaphore, name_, __VA_ARGS__)

#endif  // DP_OSAL_STATIC_H_
