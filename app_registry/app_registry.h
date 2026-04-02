/** @file app_registry.h
 *  @brief 应用动态注册框架。
 *
 *  [无需维护] 通用框架，新增 app 时不需要修改此文件。
 *
 *  原理：
 *    各 app 通过 APP_REGISTER 宏在自己的源文件中注册入口函数，
 *    使用 __attribute__((constructor)) 在 main() 前自动注册。
 *    PRODUCT_APP 需通过 --whole-archive 强制加载（CMakeLists.txt 已处理）。
 *    main.cpp 调用 app_registry_run_all() 按优先级排序后依次启动。
 *
 *  用法：
 *    APP_REGISTER(my_app, my_app_main, 0, 0);      // 立即启动
 *    APP_REGISTER(monitor, monitor_main, 1, 30000); // 30s 后启动 */

#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_entry_fn)(void);

/** @brief 注册一个应用（由 APP_REGISTER 宏调用，不直接使用）。 */
void app_registry_add(const char *name, app_entry_fn entry,
                      int priority, uint32_t delay_ms);

/** @brief 按优先级排序并依次启动所有注册的应用。 */
void app_registry_run_all(void);

/** @brief 毫秒延迟（弱符号，POSIX 默认用 usleep，裸机可覆盖）。 */
void app_registry_delay_ms(uint32_t ms);

/** @brief APP_REGISTER 宏。
 *  @param name_      应用名称（C 标识符）
 *  @param fn_        入口函数 void (*)(void)
 *  @param prio_      优先级（越小越先，相同按注册顺序）
 *  @param delay_ms_  延迟启动毫秒数（0 = 立即）
 *
 *  仅在定义了 DPSDK_APP_MAIN 时生效。当 app 作为辅助依赖被链接（非 PRODUCT_APP）
 *  时不注册，避免多个 app 同时注册导致的阻塞冲突。 */
#ifdef DPSDK_APP_MAIN
#define APP_REGISTER(name_, fn_, prio_, delay_ms_)                         \
    __attribute__((constructor)) static void _app_reg_##name_(void) {      \
        app_registry_add(#name_, (fn_), (prio_), (delay_ms_));             \
    }
#else
#define APP_REGISTER(name_, fn_, prio_, delay_ms_)  /* 非 PRODUCT_APP，不注册 */
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_REGISTRY_H */
