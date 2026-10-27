/** @file can_async_port.h
 * @brief 仅显式选择尚未实现的物理异步 CAN port 时触发，旧聚合头不引入它。
 */
#error "请实现本平台的异步 CAN port，或显式绑定 dp::hal::UnsupportedCanAsync"
