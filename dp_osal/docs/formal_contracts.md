# 正式资源与I/O契约

## 所有权与线程

Thread是普通可连接资源包装；一个owner串行操作生命周期。借用context和callable捕获的对象需活到最终使用；isRunning不是完成屏障，detach不证明对象可销毁。函数指针/callable共用任务存储与原生创建，普通MCU线程不为Host最终清理增加堆记录或回收线程。

ThreadFinalizer启动成功才移交清理责任，失败由调用方清理；回调正常返回/支持的协作停止后先结束OSAL TLS及全部包装对象访问，再cleanup一次。Thread可以内嵌在被cleanup释放的context中，worker不可随后再访问。成功start返回前任务可能已完成，调用方如需继续访问必须保留自己的引用。cleanup不证明应用TLS或内核退出全部完成，禁止释放正在执行的外部栈/TCB；当前该能力仅POSIX实现，普通join不适用于此自动回收模式。异常终止/永不退出不承诺有限时间清理。

ThreadResources只描述调用方持有的控制资源和完成模式。静态资源与Thread需覆盖整个运行期；FreeRTOS/ESP必须等原生任务退出/挂起并由owner同步回收后才能复用。永久槽/单次启动策略由应用管理，不属于CORE契约。未支持的静态/Detached模式显式失败，不忽略外部栈而偷偷动态创建。

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

## 测试与分发

I/O真实套件覆盖短写、背压、偏移、借用flags和关闭；GNU-wrap故障套件覆盖sem/clock/EINTR/poll及写入后的mask错误，并覆盖FORTIFY别名，防止注入器漏过真实调用。支持状态、错误返回与测试覆盖不是同一事实，未适配后端不能因为公开了接口就写成运行通过。

SDK只分发编译归档，正式OSAL合同revision=2及measurement快照与归档manifest共同校验；旧前缀或旧源码头必须拒绝混用。一致重编译后使用新前缀，不用修改单个TU宏修补二进制差异。
