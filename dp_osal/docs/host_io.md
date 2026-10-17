# Host字节I/O与计数唤醒

`dp_osal_host_io.h` 提供CRTP `HostIoBase<OSALHostIo>`，构造不做I/O，非复制；显式open/close返回bool，try_write/read_at/write_at返回kOk/kWouldBlock/kClosed/kError并写回实际字节数。read_at的kOk且0字节表示EOF；EINTR为WouldBlock，调用者按自身预算决定重试。open_exclusive要求绝对新路径，保O_EXCL/O_NOFOLLOW；rename_file仅用于调用者已拥有的路径。

open_output借用描述符的内容位置：常规文件F_DUPFD_CLOEXEC共享原offset且不修改flags；pipe/TTY通过/proc/self/fd重开独立OFD并O_NONBLOCK。closed pipe在当前写线程临时屏蔽/消费本次SIGPIPE，保原pending信号与进程处理策略。常规文件O_NONBLOCK不能界定磁盘时延，应由独立reader写入，调用者以预算退出并保reader状态存储到真实退出，不能超时即释放对象。

OSALHostWake构造不初始化内核资源；open显式建立计数信号量，signal不取用户mutex且报告失败；wait_for_ms用CLOCK_MONOTONIC的sem_clockwait，signal早于wait登记仍保credit。close只允许在所有使用者停止后调用。

现有实现限Linux，Linux后端转发POSIX实现；CMSIS与port/template启用时#error，不增加MCU默认能力。steady_ns沿Linux CLOCK_MONOTONIC，单位ns；无法取时返回0，调用者不得以0伪造有效观测。

POSIX Thread的startRaw不构造std::function；禁异常编译时sleep被stop唤醒后协作返回，raw回调循环需检查自身停止状态。legacy异常退出仅在__cpp_exceptions启用；CMSIS保持原实现。

验证：gtest_host_io.cpp覆盖满pipe/恢复/closed-reader、借用fd flags与常规文件offset、独占原件/偏移读写与wake先signal credit；另以-fno-exceptions/-fno-rtti编译真实Host入口。测试在现有单TU注册，不为产品增加独立默认任务。

`Thread::startRawOwned(name, entry, context, cleanup, ...)` 为POSIX受检原始worker提供最终清理回调。
创建失败不调用cleanup，调用方归还预留引用；成功后entry返回，backend清除entry、TLS和句柄，
在最后一次访问Thread对象之后detach并调用cleanup(context)，恰好一次。
cleanup可释放包含该Thread的上下文。其它backend调用此可选能力明确编译失败；旧startRaw/start语义保持。
调用方仍须保留owner引用直到停止请求已经发布；超预算可放弃owner，由worker最终归还资源。
