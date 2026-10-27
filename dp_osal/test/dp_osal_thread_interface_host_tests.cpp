/** @file dp_osal_thread_interface_host_tests.cpp
 * @brief 无内核依赖的CRTP线程配置与入口派发合同。
 */
#include "interface_thread.h"
#include <cassert>
#include <type_traits>

struct Backend : dp::osal::ThreadBase<Backend> {
    static constexpr bool kSupportsTimedJoin = true;
    static constexpr bool kSupportsControlBlockStorage = false;
    dp::osal::ThreadResult doStart(const char* name, dp::osal::detail::ThreadEntryStorage task, void* context,
                                   int priority, int bytes, void* stack,
                                   const dp::osal::ThreadResources& resources = {}) {
        assert(name != nullptr && priority == 7 && bytes == 1024 && stack == nullptr);
        assert(resources.control_block == nullptr);
        if (!task.valid()) return {dp::osal::ThreadStatus::kInvalidArgument};
        task.run(context);
        return {dp::osal::ThreadStatus::kSuccess};
    }
};
int main() {
    using namespace dp::osal;
    static_assert(noexcept(std::declval<Backend&>().start(static_cast<ThreadEntry>(nullptr), nullptr)));
    const ThreadResult timeout{ThreadStatus::kTimeout, 110};
    assert(!timeout.ok() && timeout.native_error == 110 && timeout != 0);
    Backend thread;
    ThreadAttributes attributes;
    attributes.name = "contract";
    attributes.priority = 7;
    attributes.stack_size_bytes = 1024;
    int count = 0;
    assert(thread.start(+[](void* arg) { ++*static_cast<int*>(arg); }, &count, attributes).ok());
    assert(thread.start([&count](void*) { ++count; }, nullptr, attributes).ok());
    assert(count == 2);
    assert(thread.start(static_cast<ThreadEntry>(nullptr), nullptr, attributes).status ==
           ThreadStatus::kInvalidArgument);
}
