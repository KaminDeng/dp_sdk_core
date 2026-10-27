/** @file interface_byte_output.h
 * @brief 仅输出字节流；创建与原生描述符绑定在具体端口，调用者处理短写和背压。 */
#pragma once
#include "interface_io.h"
namespace dp::osal {
template <typename Impl>
class ByteOutputBase {
public:
    /** @brief 只尝试一次；kOk允许短写。written必须有效，非零size要求bytes有效。
     * @note 缓冲只借用到返回；kWouldBlock由调用方安排重试。错误不证明绝无字节送出。 */
    IoResult tryWrite(const uint8_t* bytes, size_t size, size_t* written) noexcept {
        return impl().doTryWrite(bytes, size, written);
    }
    /** @brief 释放自持输出资源；可重复调用，原借用资源由原owner负责。 */
    bool close() noexcept { return impl().doClose(); }

protected:
    ~ByteOutputBase() = default;

private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
}  // namespace dp::osal
