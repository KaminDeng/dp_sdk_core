# Linux兼容：Host字节I/O与计数唤醒

这些能力属于显式兼容面。配置核心前启用`DP_OSAL_ENABLE_LEGACY_COMPAT`，消费者链接`dp_osal_legacy_compat`；头位于`compat/include`、`compat/interface`与所选`compat/<backend>`，保持原include名。默认dp_osal公开头面不含HostIo、HostWake、RealtimeThread和StaticTimer，measurement开关与本兼容角色独立。普通线程继续使用原`Thread::start`；本页owned所有权契约不能机械替换为lambda。

`dp_osal_host_io.h` 提供CRTP `HostIoBase<OSALHostIo>`，构造不做I/O，非复制；显式open/close返回bool，try_write/read_at/write_at返回kOk/kWouldBlock/kClosed/kError并写回实际字节数。read_at的kOk且0字节表示EOF；EINTR为WouldBlock，调用者按自身预算决定重试。open_exclusive要求绝对新路径，保O_EXCL/O_NOFOLLOW；rename_file仅用于调用者已拥有的路径。

open_output借用描述符的内容位置：常规文件F_DUPFD_CLOEXEC共享原offset且不修改flags；pipe/TTY通过/proc/self/fd重开独立OFD并O_NONBLOCK。closed pipe在当前写线程临时屏蔽/消费本次SIGPIPE，保原pending信号与进程处理策略。常规文件O_NONBLOCK不能界定磁盘时延，应由独立worker执行，调用者以预算退出并保worker状态存储到真实退出，不能超时即释放对象。

OSALHostWake构造不初始化内核资源；open显式建立计数信号量，signal不取用户mutex且报告失败；wait_for_ms用CLOCK_MONOTONIC的sem_clockwait，signal早于wait登记仍保credit。close只允许在所有使用者停止后调用。

现有实现限Linux，Linux后端转发POSIX实现；CMSIS与port/template启用时#error，不增加MCU默认能力。steady_ns沿Linux CLOCK_MONOTONIC，单位ns；无法取时返回0，调用者不得以0伪造有效观测。

禁异常编译时sleep被stop唤醒后协作返回，回调循环需检查自身停止状态。异常退出仅在__cpp_exceptions启用。普通start的可调用对象包装可能涉及动态资源；不能把特定函数指针的测量结果外推为任意lambda无分配，也不能将普通raw和owned的清理契约混同。

验证：gtest_host_io.cpp覆盖满pipe/恢复/closed-reader、借用fd flags与常规文件offset、独占原件/偏移读写与wake先signal credit；另以-fno-exceptions/-fno-rtti编译真实Host入口。HostIo、旧工具和owned分别在显式兼容测试target注册，普通chrono/contract目标不include兼容套件；GNU wrap故障注入还需相应平台与链接器能力。测试注册不代表当前产品已运行这些用例。

`Thread::startRawOwned(name, entry, context, cleanup, ...)` 为POSIX受检原始worker提供最终清理回调。
创建失败不调用cleanup，调用方归还预留引用；成功后entry返回，backend清除entry、TLS和句柄，
在最后一次访问Thread对象之后detach并调用cleanup(context)，恰好一次。
cleanup可释放包含该Thread的上下文。其它backend调用此可选能力明确编译失败；普通start不会获得这项所有权移交。
调用方仍须保留owner引用直到停止请求已经发布；超预算可放弃owner，由worker最终归还资源。

`poll_input(byte, HostInputInfo*)`由单一stdin消费者调用，零等待poll后至多读一个字节，
不修改共享fd flags；未就绪为WouldBlock，读EOF为Closed，读错误为Error。
info捕获原revents/errno和是否真正执行read；失败不改byte。POLLNVAL返回Error/EBADF，POLLERR返回Error/EIO，均不尝试read；POLLIN/HUP仍先交付缓冲字节，排空后Closed。
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
| `compat/interface/interface_host_io.h` | CRTP公开入口、参数生命周期、结果与实际字节数 |
| `compat/interface/interface_host_wake.h` | open/signal/相对毫秒等待/close 的独立唤醒资源 |
| `compat/posix/dp_osal_host_io.h` | 当前Linux系统调用、描述符所有权、错误与短传输 |
| `compat/posix/dp_osal_host_wake.h` | 单调期限、EINTR重试、信号量令牌与关闭前提 |
| `compat/linux/dp_osal_host_*.h` | 只转发到POSIX目录，不复制实现 |
| `compat/cmsis_os/`、`compat/template/` | 此Host能力未实现时明确编译失败，不默认为MCU可用 |

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

## 所属组件与仍需兼容的能力

HostIo是当前Linux兼容入口，不作为新的跨平台综合门面。向正式组件归位必须逐方法证明错误分类、描述符/缓冲所有权、时间域、精度和持久化语义等价。下表记录当前缺口及退出条件。

HostWake也继续使用独立兼容实现：普通POSIX Semaphore基于mutex/condition_variable/int，signal没有失败返回，缺少open/close错误协议，等待bool不能区分error与timeout；HostWake需要sem_t显式初始化、真实sem_post溢出错误、CLOCK_MONOTONIC绝对期限和EINTR沿原期限重试。当前没有可复用的等价底层片段，不能通过替换类型抹去这些语义。


以下目标组件是当前实际 dp_storage/dp_console/Chrono/System，不是拟新增接口。各方法继续兼容的共同退出条件：有正式所属组件提供同等错误、资源所有权、精度和持久化契约，并由现有反例证明；不能只凭名字相近迁移。

| 方法 | 原能力/约束 | 目标及明确缺口 | 验证/退出条件 |
|---|---|---|---|
| open_output | 自持dup/proc重开描述符，借用fd flags不变，普通文件共享offset | console仅UART且初始化依赖设备；不接受进程fd | 原fd/flags/offset、pipe与关闭读端回归 |
| try_write | 非阻塞尝试、实际短写数量、背压、线程SIGPIPE屏蔽恢复 | console写void，无短写/错误/借用fd契约；storage FILE顺序写无fd能力 | 实际短写、满pipe、EPIPE、flags |
| open_exclusive | 绝对新路径O_EXCL/O_NOFOLLOW，自持fd | storage没有独占创建，初始化FAT卷且受有限slots约束 | 原文件保留、符号链接、失败不取得资源 |
| open_existing | 已有普通文件、O_NOFOLLOW、自持fd、读写 | storage模式/卷路径不同，无受检原生描述符 | 目录/符号链接拒绝、真实普通文件 |
| read_at | pread固定64位offset，不改共享offset，短读数量 | storage无seek/read_at，block port针对整镜像不是任意Host文件 | 现有offset回读、EOF/短读 |
| write_at | pwrite固定64位offset、短写数量 | storage无write_at；不能用共享FILE偏移假装原子独立offset | 现有偏移写读及数量 |
| sync | 真实fd fsync，允许阻塞，不含目录同步 | storage flush不是任意Host fd fsync；镜像port sync仅管镜像 | 真实文件sync，断电/目录持久化另测 |
| rename_file | 原生Host路径rename，不自动fsync目录 | storage_rename针对挂载FAT路径，不是Host命名空间 | 未来迁移须同命名空间及失败语义 |
| max_path_bytes | PATH_MAX调用方存储大小 | storage路径与卷约束不同，无同一PATH_MAX契约 | 小buffer反例 |
| canonical_directory | realpath既有Host目录，调用方存储，不绑定持续身份 | storage/console均无规范化既有Host目录能力 | /tmp/..、不存在目录、空参数 |
| read_regular_prefix | 同步有界前缀，检查普通文件，失败used=0，资源局部释放 | storage需卷初始化/slots，不能检查任意Host fd类型 | 有界前缀/目录拒绝/空输出 |
| steady_ns | CLOCK_MONOTONIC实际纳秒，失败0 | Chrono是端口tick时间域，System为ms；乘常数不能恢复精度 | 不改变现有纳秒消费者时间域 |
| poll_input | 单一stdin消费者，零超时，byte/EOF/错误/事件区分 | console是UART，-1混合未初始化/无数据，不是进程标准输入 | 失效stdin、可读+HUP、缓冲排空、错误不read |
| install_termination_handler | 进程SIGINT/SIGTERM与第二步失败回滚 | MCU console/OSAL无进程级信号策略 | 注入SIGTERM失败后SIGINT恢复 |
| wall_time_us | CLOCK_REALTIME可跳变微秒，checked整数换算 | System单调ms无墙钟域；不能换作控制期限 | 实际时钟域、空输出失败 |
| thread_cpu_time_us | 当前线程CPU微秒，非墙钟，缺源失败 | System快照是采样窗口/不同精度，不是本调用点CPU时钟 | 单调CPU读数且与墙钟区分 |
| signal_safe_error | 信号上下文单次stderr write，短写可丢失 | console经设备管理/UART，不具备进程信号安全证明 | 保留紧急输出边界，不承诺完整日志 |
| exit_immediately | _exit，无析构/atexit | MCU复位和普通return不是进程立即退出 | 仅既有决定退出故障路径，退出条件须同语义 |
| close | 释放自持fd，不关闭原借用fd；失败也撤销本地句柄 | storage受slots和FILE所有权管理，不接受原生fd | 重复close与借用原fd仍可用 |

dp_storage公共API源码与builtin_posix port已核对：后者的pread/pwrite/fsync只用于固定镜像块设备，不能当作任意Host文件的现成迁移接口。dp_console实际依赖DeviceManager IUart，不能替代stdin/stdout。没有修改这些组件公共接口。
