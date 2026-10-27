# 正式资源与I/O契约

## 所有权与线程

`Thread` 只空构造，禁止复制/移动，默认创建 joinable 线程；一个 owner 串行执行生命周期及挂起/恢复/优先级操作；仅 requestStop、stopRequested、joinable、isRunning 四项允许跨线程调用，仍须由 owner 保证包装对象寿命。推荐 `start(entry, context, attributes)`，也保留 `start(name, entry, context, priority, stack_bytes, stack)` 便捷形式。函数指针及可转换的无捕获 lambda 不为入口包装额外分配，捕获 callable 仍可能由 std::function 分配；不公开任务存储或线程自销毁回调。

`requestStop()` 幂等发布协作停止请求，业务循环显式查询 `stopRequested()` 并唤醒自身等待；请求不代表已停止。`join()` 成功才确认原生线程结束及资源回收，允许释放借用 context、栈与控制块。自然返回后 `joinable()` 仍为 true，`isRunning()==false` 不是释放屏障。已有回收责任时 start 返回 kBusy；join 失败/超时保留尚未完成的责任，禁止覆盖旧线程。原生线程已回收但同步资源清理失败时，后续 join 仅重试剩余清理，不能重复回收原生句柄；joinable 为 false 也不能抹掉已报告的清理失败。自身 join 返回 kWouldDeadlock。

析构前必须完成 join 和必要清理；违反约束在 Release 也终止进程/触发平台 fatal，不自动无限等待、不强杀、不静默分离。显式 `stop()` 是 requestStop 加无限 join 的便捷操作，可能一直等待业务退出，不能拿它实现有界关闭。所有后端 `detach()` 返回 kNotSupported。

有限等待通过 `supportsTimedJoin()`（后端 `kSupportsTimedJoin`）查询；`tryJoinFor(timeout_ms)` 使用毫秒相对期限，超时保留全部资源。当前 Linux/glibc >= 2.31 的 GNU 接口和 Zephyr 原生 join 支持，其他 POSIX、FreeRTOS、RT-Thread 与 ESP-IDF 明确返回 kNotSupported，不用“完成通知加无限回收”伪装有界 join。ThreadResult 用 ok()/status/native_error 检查；错误包括 Timeout、NotSupported、InvalidState、Busy、WouldDeadlock 和资源/原生错误。挂起/恢复/优先级是后端扩展，不是通用同步安全点。

静态资源通过`ThreadResources`和后端资源存储表达，`supportsControlBlockStorage()` 反映实际能力；栈容量统一字节。完成通知不等于原生线程真正退出，借用资源须一直保留到成功 join。FreeRTOS/ESP 的 owner 回收与 SMP 在核运行状态由端口处理；不支持的资源配置显式拒绝。

### 通用使用方式

| 接口 | 用法与责任 |
|---|---|
| `Thread worker;` | 只建立空对象，不创建原生线程；不可复制或移动。 |
| `start(entry, context, attributes)` | 显式检查 `ThreadResult::ok()`；失败不接管 context，已有责任返回 `kBusy`。 |
| `requestStop()` | 幂等发布停止请求；bool 不是停止完成证明，也不替业务唤醒队列。 |
| `stopRequested()` | worker 在循环/等待返回后检查的协作标志。 |
| `joinable()` | 当前是否仍持有原生线程连接责任，线程自然结束后仍可能为 true。 |
| `join()` | 无期限等待原生结束并回收；失败保留剩余责任，允许重试。 |
| `supportsTimedJoin()` | 所选后端的编译期能力，不以接口存在推断已支持。 |
| `tryJoinFor(timeout_ms)` | 支持的后端在同一相对期限内连接/回收；0 表示只尝试，不等待。 |
| `stop()` | 显式 requestStop 加无限 join，不能作为有界析构或强杀。 |
| `isRunning()` | 观测业务入口执行状态，不是内核退出或 context 可释放证明。 |
| `detach()` | 当前所有后端显式返回 `kNotSupported`。 |

`ThreadAttributes` 使用 name、priority、stack_size_bytes、stack 和 resources；priority 的编码由 port 解释，不能假定跨内核实时级别相等。`ThreadResources` 的 control_block/control_block_size、completion_control_block/completion_control_block_size 描述借用控制资源，不提供所有权转移。资源是否被支持以能力和实际 start 结果为准。

```cpp
#include "dp_osal_thread.h"
#include "dp_osal_system.h"

struct WorkerContext { dp::osal::Thread* worker; };

void run_worker(void* raw) {
    auto& context = *static_cast<WorkerContext*>(raw);
    while (!context.worker->stopRequested()) {
        // 执行一小段工作；实际队列等待需要 owner 在停止时另行唤醒。
        dp::osal::System::getInstance().sleep_ms(1U);
    }
}

void run_example() {
    dp::osal::Thread worker;
    WorkerContext context{&worker};
    dp::osal::ThreadAttributes attributes;
    attributes.name = "worker";
    attributes.stack_size_bytes = 2048U;
    const auto started = worker.start(run_worker, &context, attributes);
    if (!started.ok()) return;  // 未启动，不接管 context。
    worker.requestStop();
    if (!worker.join().ok()) std::terminate();  // 不以失败结果释放借用资源。
}
```

有界关闭由长期 owner 保留对象，先 requestStop/唤醒，再 tryJoinFor；Timeout 或 NotSupported 不能导致局部 Thread 析构。需要退让 UI/控制线程时，把完整服务保留在已有长期管理上下文，在后续调用重试真实 join，不创建无界后台线程。

Dummy SDK 的 EvidenceSink 与 PanelEventJournal 各有四个进程寿命管理槽；阻塞 write/flush/close 耗尽 owner 预算后保留 Thread 和 context，第五个未回收启动显式失败。后续 prepare/start/stop 或 start/finish 在原调用上下文尝试非阻塞 join 后清理；不新增清理线程，也不 detach。无后续管理调用时，上限内资源保留至进程退出。正常 join 立即归还槽，前端捕获内容仍可保留。RetainedContext hooks、shared_ptr owner 自定义 deleter 与 context 析构均须有界、非阻塞且禁止重入组件；管理器在原管理调用中释放最后引用，不能保证任意用户 deleter 有界。文件关闭仍由 worker 执行。

dp_comm raw observer 仅保留一个待回收 worker：停用回调并发布停止后真实 join，再释放拥有者和上下文；不能以 running=false 或线程完成通知代替回收，也不能在重连时累积离散后台任务。

## 唤醒与时间

Semaphore为唯一计数唤醒机制；构造默认初始化，DeferredInit允许checked open重试。release反馈未打开/满计数/系统失败，waitForMs分开成功、timeout和error；提前signal保留令牌，EINTR沿同一单调deadline重试。max=0取后端上限，跨平台不得猜为相同值。init/close不得与等待/发送并发，关闭失败也撤销旧句柄所有权。

Chrono的tick换算用真实ticksPerSecond，绝对tick期限满足半圈比较前提。steadyNs使用独立实际高精度单调来源，不能从32位tick恢复精度；0表示不可用。wallTimeUs/threadCpuTimeUs读取失败保留输出并返回false，墙钟不用于控制期限。CMSIS/ESP缺少可靠来源时明确不可用。

## 字节输出

ByteOutput只负责持有输出资源、tryWrite与close；数量与IoResult共同解释。NativeIo负责Linux借用描述符绑定：普通文件dup保共享offset；pipe/TTY经proc重开独立非阻塞描述符，不能改变借用fd的flags。遇背压不内部无界重试，短写返回实际数量；关闭读端不导致致命SIGPIPE。

系统write已完成后，恢复signal mask仍可能失败。此时Error不证明零字节已写，使用方不得无条件重发。参数错误和系统错误的数量语义由接口及反例明确，持久化不由一次tryWrite成功证明。

## 文件与文件系统

File独占创建要求绝对路径、O_EXCL/O_NOFOLLOW；打开已有路径必须验证普通文件，FIFO以非阻塞打开后拒绝。readAt/writeAt使用独立64位offset，处理短读/短写，不改变共享offset；sync只同步文件。FileSystem负责rename、调用方缓冲的目录规范化、有界普通文件前缀；失败used=0，局部句柄释放。rename后父目录持久性需要另行处理，不伪称已完成。

## 原生进程端口

NativeProcess仅为平台适配入口，不是通用CORE原语，不进入MCU公共接口。stdin只有一个消费者；POLLIN/HUP先排空字节再EOF，POLLERR/POLLNVAL不能read，错误不覆盖byte。poll EINTR返回WouldBlock，read EINTR返回Error；write/pread/pwrite EINTR为WouldBlock，保原分类。

SIGTERM安装失败须回滚先前SIGINT；不需SIGINT的调用不能顺便安装。紧急stderr使用单次异步信号安全write，立即退出不执行析构；这些能力不提供运动授权/停止确认。SDK证据写盘仅依赖文件/流/时钟，不依赖signal/立即退出服务。

## 集成时发现的应用状态域修正

本次统一验证另发现已存在于 Dummy SDK 共享释放策略的状态域混用，并作独立最小修复；这不是 Thread 生命周期能力或固件协议变更。内部 `LifecycleState::kIdle` 的数值为 1、`kEnablingHold` 为 2，而 ROS 收到的 wire Idle 编码为 2，不能把两个域的整数直接传入同一谓词比较。

`SessionReleaseObservation` 改收 `status_idle/state_idle` 两个布尔事实：RemoteMotionPort 先按内部枚举判断，ROS DeviceManagementChannel 先按 wire 编码判断，再由 `session_release_observed` 组合双 Idle、会话/许可清零与清理状态。布尔值只表示各适配器对自己状态域的解释，不新增身份、新鲜度、D10 提交、审计完整或运动成功证明。测试使用真实内部枚举并保留“内部 2 是 EnablingHold”的反例，避免合成 wire 数值伪装成内部正例；Native CLI 未改，此软件修复不代替实机验收。

## 测试与分发

I/O真实套件覆盖短写、背压、偏移、借用flags和关闭；GNU-wrap故障套件覆盖sem/clock/EINTR/poll及写入后的mask错误，并覆盖FORTIFY别名，防止注入器漏过真实调用。支持状态、错误返回与测试覆盖不是同一事实，未适配后端不能因为公开了接口就写成运行通过。

SDK只分发编译归档，正式OSAL合同revision=3及measurement快照与归档manifest共同校验；旧前缀或旧源码头必须拒绝混用。一致重编译后使用新前缀，不用修改单个TU宏修补二进制差异。
