add_library(app_registry STATIC app_registry.c)
target_include_directories(app_registry PUBLIC .)
# 不链接 osal —— 延迟通过弱符号 app_registry_delay_ms 实现，避免循环依赖
