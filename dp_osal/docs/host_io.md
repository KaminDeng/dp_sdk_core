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

`poll_input(byte, HostInputInfo*)`由单一stdin消费者调用，零等待poll后至多读一个字节，
不修改共享fd flags；未就绪为WouldBlock，读EOF为Closed，读错误为Error。
info捕获原revents/errno和是否真正执行read；失败不改byte。
只允许单消费者以保持poll→read间可读性，不把此API用于任意共享描述符。

`install_termination_handler(handler, include_interrupt=true)`保持原进程级SIGTERM/SIGINT处理函数语义；
仅启动owner注册，空handler失败，第二次注册失败时回滚第一次。
传false仅安装SIGTERM，保只读probe原SIGINT行为；不是worker级信号或可重入注册API。
`signal_safe_error`只用异步信号安全的write写stderr固定诊断，保持原probe信号路径的尽力输出语义；
不保证慢stderr有界，不能用于持权限owner日志，正常owner仍须typed journal。
`exit_immediately`只供原终止诊断的立即退出。

`wall_time_us`为UNIX CLOCK_REALTIME微秒，`thread_cpu_time_us`为调用线程CPU微秒，
均以bool标记有效性并保失败output不变；不能以这些域替代steady_ns时限。
新增功能只在既有Linux Host后端，CMSIS/template保持启用时明确拒绝，不改变MCU资源或默认能力。

路径与等待阶段文件输入同样经过HostIo：max_path_bytes给出后端路径容量，canonical_directory只向调用方已准备的缓冲写入规范目录，失败返回false；read_regular_prefix只接受普通文件，最多读取调用方容量，不在应用层构造filesystem/fstream或字符串。两者可能等待文件系统I/O，仅用于准备或启用运动之前的permit等待阶段，不承诺磁盘延迟上界。FIFO以非阻塞open后按类型拒绝，不会等待其writer。CMSIS/template保持显式#error，Linux通过POSIX实现同步获得接口。


## 从接口到实现的阅读路线

| 文件位置 | 阅读重点 |
| --- | --- |
| `src/interface/interface_host_io.h` | CRTP公开入口、参数生命周期、结果与实际字节数 |
| `src/interface/interface_host_wake.h` | open/signal/相对毫秒等待/close 的独立唤醒资源 |
| `src/impl/posix/include/dp_osal_host_io.h` | 当前Linux系统调用、描述符所有权、错误与短传输 |
| `src/impl/posix/include/dp_osal_host_wake.h` | 单调期限、EINTR重试、信号量令牌与关闭前提 |
| `src/impl/linux/include/dp_osal_host_*.h` | 只转发到POSIX目录，不复制实现 |
| `src/impl/cmsis_os/include/`、`port/template/` | 此Host能力未实现时明确编译失败，不默认为MCU可用 |

这里的“POSIX目录”不等于可在任意POSIX系统直接启用：当前Host I/O和HostWake实现含Linux限制，
其它平台必须提供自己的适配。对象通过CRTP静态调用`Impl::do*`，不经过虚函数；基类受保护非虚析构，
不能当作拥有资源的多态基类删除。

一次有界输出的调用顺序是：准备阶段显式open取得自持句柄，消费阶段try_write检查状态和实际数量，
成功短写时只前移已写出的长度，WouldBlock时按调用者预算暂停或重试，Closed/Error时停止使用该通道。
`kOk`不表示全部请求已传出，更不表示文件已经持久化。文件提交还需分别安排sync、rename及业务提交记录；
单独rename不会自动同步目录，不能以一次函数成功宣称断电后必定保留全部证据。

所有缓冲、路径及输出指针均由调用者提供，本接口不接管它们的存储。不要在worker仍可能访问时释放上下文；
owned清理只保证后端在最后一次访问Thread之后交还上下文，调用者仍需让自己的owner停止请求和引用计数配合。
HostWake的signal只增加一次可消费令牌，不证明业务谓词已满足或目标线程已执行；醒来后仍应检查自己的状态。

`poll_input`的“零等待”指poll超时为零；必须保持stdin单消费者，否则poll与read之间另一读者可能抢走字节。
`signal_safe_error`只尽力写一次，可能短写、失败或受慢stderr影响；`exit_immediately`不执行C++析构，
不应把这两个故障入口当作正常日志与资源回收路径。

时间单位必须连同来源一起阅读：HostIo的steady_ns是单调纳秒，wall_time_us是可跳变墙钟微秒，
thread_cpu_time_us只统计当前线程消耗CPU的微秒。通用Chrono::TimePoint是后端tick：POSIX当前1000Hz，
CMSIS按内核频率；Mutex::tryLockFor、Queue::receiveFor、Semaphore::tryWaitFor在两个后端也分别使用
POSIX毫秒和CMSIS原生tick。上述HostWake::wait_for_ms则明确使用毫秒，不能把同一个数值盲目跨接口复用。

本页描述的是接口与当前实现边界，既有测试清单用于定位验证入口，不代表每次阅读或注释修改都重新执行过硬件验证。
