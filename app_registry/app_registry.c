/** @file app_registry.c
 *  @brief 应用注册表实现。 */

#include "app_registry.h"

#include <stdio.h>
#include <stdlib.h>
#ifndef DPSDK_BARE_METAL
#include <unistd.h>
#endif

#ifndef APP_REGISTRY_MAX
#define APP_REGISTRY_MAX 16
#endif

typedef struct {
    const char *name;
    app_entry_fn entry;
    int priority;
    uint32_t delay_ms;
} app_entry_t;

static app_entry_t s_entries[APP_REGISTRY_MAX];
static int s_count = 0;

void app_registry_add(const char *name, app_entry_fn entry,
                      int priority, uint32_t delay_ms) {
    if (s_count >= APP_REGISTRY_MAX) {
        return;
    }
    s_entries[s_count].name = name;
    s_entries[s_count].entry = entry;
    s_entries[s_count].priority = priority;
    s_entries[s_count].delay_ms = delay_ms;
    s_count++;
}

/** @brief 比较函数：按 priority 升序排列。 */
static int cmp_priority(const void *a, const void *b) {
    return ((const app_entry_t *)a)->priority -
           ((const app_entry_t *)b)->priority;
}

/** @brief 毫秒延迟（弱符号，可被平台 port 覆盖）。
 *  宿主机默认使用 usleep()，裸机产品应在 port 中提供强定义。 */
#ifndef DPSDK_BARE_METAL
__attribute__((weak)) void app_registry_delay_ms(uint32_t ms) {
    usleep(ms * 1000U);
}
#else
__attribute__((weak)) void app_registry_delay_ms(uint32_t ms) {
    (void)ms;
    /* 裸机：需由 port 提供强定义覆盖此弱符号 */
}
#endif

void app_registry_run_all(void) {
    if (s_count == 0) {
        printf("[app_registry] 无注册应用\n");
        return;
    }
    qsort(s_entries, (size_t)s_count, sizeof(app_entry_t), cmp_priority);
    printf("[app_registry] 已注册 %d 个应用:\n", s_count);
    for (int i = 0; i < s_count; i++) {
        printf("  [%d] %s (priority=%d, delay=%u ms)\n",
               i, s_entries[i].name, s_entries[i].priority,
               (unsigned)s_entries[i].delay_ms);
    }
    for (int i = 0; i < s_count; i++) {
        if (s_entries[i].delay_ms > 0) {
            printf("[app_registry] %s: 延迟 %u ms...\n",
                   s_entries[i].name, (unsigned)s_entries[i].delay_ms);
            app_registry_delay_ms(s_entries[i].delay_ms);
        }
        printf("[app_registry] 启动: %s\n", s_entries[i].name);
        s_entries[i].entry();
    }
}
