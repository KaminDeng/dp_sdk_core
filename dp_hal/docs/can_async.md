# 经典 CAN 异步能力

`dp_can.h` 的 `CanBase` 保持原接口。需要预留、真实发送终态或捕获的 RX 时，单独包含 `dp_can_async.h`；旧 port 不必实现它。选定平台的 `DpCanAsync` 可以是 `UnsupportedCanAsync`，这不改变该平台已有普通 CAN 的可用性。全零能力/诊断值不代表硬件健康。

公共层只有值、能力与 CRTP 静态派发。它不创建 worker、队列、消费者或运行时接口表。电机命令、查询批次、业务超时与安全策略由调用者负责；寄存器、IRQ、厂商句柄及私态由 port 负责。

## 值与时序

先检查实际 `capabilities()`，再按平台要求初始化。容量由 `max_in_flight/max_reservations` 给出，不假设三个邮箱。缺可选能力明确返回 `kNotSupported`，不偷偷改用普通发送、轮询线程或另一种取消方式。

`reserve` 成功后，token 属于同一实例/boot 的单 owner；不能跨任务、重启或实例借用。token 仅能移动构造，不能复制或赋值；析构不撤销。显式 `cancelReservation` 成功使其无效。`submitReserved(std::move(token),tx)` 不论接纳或拒绝均消耗 token，port 必须清理对应预留，不能释放别人的预留或换槽。`submitDirect` 的前置条件是没有活动 reservation。

提交 `kOk` 表示硬件接纳。硬件终态入环后，槽仍被占用；唯一 owner 消费且完整身份检查通过后才可复用。取消是请求，`Sent` 可以先完成；`requestAbortAllBestEffort` 的 `kOk` 只说明调用已转发。输入在提交返回前复制，返回值由调用者拥有，禁止保存驱动环槽指针。

F405 的终态 `*_us` 和 `measurement_ingress_us` 是 TIM5 1MHz 的 32位模域。RX `received_at_us` 是 HAL 32位毫秒 tick 乘1000，64位类型没有扩展回绕；不能与 TIM5、DWT 或 Host 时间直接相减。其他 port 必须写明自己的来源、单位、域、位宽与回绕范围。

逐调用 `*Observed` 是可选能力 bit7，只有真正保留同次选槽、提交、消费和取消原点的 port 才能置位；不支持时显式拒绝。`valid=false` 时其他观察字段无效；不能把0或事后读钟当原点。token/消费/时钟观察不建立额外持久表。实现可选调用时须提供对应私有 do*；不用这些调用的实现不被强制新增 do*。

`kBusy` 是暂时无资源；`kInvalidArg` 是非法值；`kNotSupported` 是缺能力/帧型；`kError` 可伴随锁存故障。通过诊断 `fault_flags` 区分 terminal 环满、身份错误与 bus-off。F405 保留的 `event_overflow_count` 是原共享 event-fault 饱和总计，不能称为纯环满次数。诊断是非原子联合快照，不负责业务恢复或清锁存。

## 离线非 Dummy 示例

示例同时调用旧 `CanBase` 和一个显式单槽模型。请求标记、会话、代次/序号由调用者传入，不包含轴数、电机命令或业务调度。

```sh
c++ -std=c++17 -Wall -Wextra -Werror -UNDEBUG \
  -Icore/dp_sdk_core/dp_hal/include \
  core/dp_sdk_core/dp_hal/examples/can_async_no_hardware.cpp \
  -o /tmp/can-async-example
/tmp/can-async-example
```

输出 `PASS ordinary CanBase + one-slot async example (software only)`。它只证明公共 API 的调用与值寿命，模型的 `simulate_completion` 不证明真实发送或 ISR。

## 新 port

1. 新建仅含中性 HAL 头与值声明的 port 类，继承 `CanAsyncBase<Impl>`；do* 为 private 并 friend Base。C/vendor/BSP 仅放实现文件。
2. 按真实容量/帧型/取消能力填写 caps。使用端须处理所有返回类别；不支持的操作明确拒绝。保留已有 `CanBase` 的行为与 ABI。
3. 在原硬件入口内完成预留选槽、输入复制、TXRQ/doorbell、ISR 终态交付及完整身份检查。覆盖成功返回前 IRQ、错代/重复终态与取消竞争；不能引入第二消费者、影子表或隐式队列。
4. 明确 owner/ISR/初始化及并发前提、输入/事件值寿命与时钟域。输出只在成功时作为新证据；任何锁存交付故障都阻止 refill。
5. 分别提供模型、真实绑定叶层、交叉编译、最终链接与实机证据。模型通过不升级成平台实机支持；经典 CAN 的8字节值不表示 CAN FD 支持。后续 FD 应另审帧存储、DLC编码、资源及终态边界，不扩大现有 F405 帧对象。

| 当前范围 | 已有证据 | 边界 |
| --- | --- | --- |
| 公共 CRTP / Unsupported | Host token、参数与旧 CanBase 调用 | 不接设备 |
| 单槽/三槽与缺能力 | 显式 Host 模型 | 非生产 Host CAN 后端 |
| F405 实际 C++ 绑定 | 实际源码 + C 硬件叶替身12场景；ARM对象 | 真实 C/最终链接/实机另外记录 |
| F405 原 C Guard/选槽 | 实际 C 函数 + 寄存器/IRQ叶刺激 | 不是物理 bxCAN 认证 |
| 其他选定平台 | `DpCanAsync=UnsupportedCanAsync` | 不声称已有异步实机支持；普通 CAN 保持原实现 |

同条件性能、最终 RAM/栈/池门禁及物理验证以实施报告为准；本说明不授予 G2/G3 通过。
