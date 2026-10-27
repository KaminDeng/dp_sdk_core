/** @file interface_io.h
 * @brief 字节传输结果；成功仍须检查实际数量，不表示已持久化。 */
#pragma once
#include <cstddef>
#include <cstdint>
namespace dp::osal {
/** @brief kOk不等于全部传输或持久化；数量有效性须遵守具体操作契约。
 * 非法参数返回kError，数量输出可保持原值；有效数量指针在开始传输前清零。
 * 调用方不得因为kError就无条件重发非幂等记录。 */
enum class IoResult : uint8_t { kOk, kWouldBlock, kClosed, kError };
}  // namespace dp::osal
