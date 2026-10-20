/**
 * @file dp_osal_virtual.h
 * @brief 主机依赖注入/Mock使用的可选虚接口与CRTP后端借用适配器。
 * @details I*定义可替换虚接口，*Virtual持有外部Impl引用，再将调用转发到原具体对象。
 * 增加虚表间接调用，不取得Impl所有权、不额外加锁、不修改错误结果或时间单位。
 * 后端和借用上下文必须覆盖适配器使用期；std::function按值转发可能复制并分配资源。
 * 这是当前列出的可选接口子集，不包含Thread raw/owned、Queue trySend或全部新增测量/Host接口。
 * 功能宏会裁剪对应类型，消费者必须使用一致配置；生产路径优先直接使用具体CRTP类型。
 */
#ifndef DP_OSAL_VIRTUAL_H_
#define DP_OSAL_VIRTUAL_H_

#include <cstdlib>

#include "interface_chrono.h"
#include "interface_condition_variable.h"
#include "interface_memory_manager.h"
#include "interface_mutex.h"
#include "interface_queue.h"
#include "interface_rwlock.h"
#include "interface_semaphore.h"
#include "interface_spin_lock.h"
#include "interface_system.h"
#include "interface_thread.h"
#include "interface_thread_pool.h"
#include "interface_timer.h"

namespace dp::osal {

/**
 * @brief 互斥锁的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IMutex {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IMutex() = default;
    /**
     * @brief 阻塞请求获取互斥锁。
     * @return 实现报告获取成功时为true，否则不能访问受保护数据。
     * @note 锁归属、递归与阻塞上下文约束由实现保证。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool lock() = 0;
    /**
     * @brief 释放当前实现持有的互斥锁。
     * @return 实现报告解锁成功时为true。
     * @note 调用者须满足锁归属及配对要求。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool unlock() = 0;
    /**
     * @brief 尝试获取互斥锁，不请求等待未来可用。
     * @return 本次取得锁为true，否则false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryLock() = 0;
    /**
     * @brief 带相对超时请求获取互斥锁。
     * @param timeout_ms 原样转发的超时数值；形参名不执行单位转换，所选CMSIS实现可能按tick解释。
     * @return 实现报告本次取得锁时为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryLockFor(uint32_t timeout_ms) = 0;
};

/**
 * @brief 借用具体互斥锁后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class MutexVirtual : public IMutex {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit MutexVirtual(Impl &impl) : impl_(impl) {}
    /**
     * @brief 阻塞请求获取互斥锁。
     * @return 实现报告获取成功时为true，否则不能访问受保护数据。
     * @note 锁归属、递归与阻塞上下文约束由实现保证。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool lock() override { return impl_.lock(); }
    /**
     * @brief 释放当前实现持有的互斥锁。
     * @return 实现报告解锁成功时为true。
     * @note 调用者须满足锁归属及配对要求。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool unlock() override { return impl_.unlock(); }
    /**
     * @brief 尝试获取互斥锁，不请求等待未来可用。
     * @return 本次取得锁为true，否则false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryLock() override { return impl_.tryLock(); }
    /**
     * @brief 带相对超时请求获取互斥锁。
     * @param timeout_ms 原样转发的超时数值；形参名不执行单位转换，所选CMSIS实现可能按tick解释。
     * @return 实现报告本次取得锁时为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryLockFor(uint32_t timeout_ms) override { return impl_.tryLockFor(timeout_ms); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};

/**
 * @brief 计数信号量的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class ISemaphore {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~ISemaphore() = default;
    /**
     * @brief 等待并请求消费一枚信号量令牌。
     * @note void不提供独立失败结果，具体阻塞及错误处置由实现定义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void wait() = 0;
    /**
     * @brief 请求发布一枚信号量令牌。
     * @note 返回不证明已唤醒线程或底层发布成功，满计数策略由实现规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void signal() = 0;
    /**
     * @brief 尝试消费现有令牌，不请求等待未来信号。
     * @return 实现报告取得令牌时为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryWait() = 0;
    /**
     * @brief 在相对超时规则下尝试消费令牌。
     * @param timeout_ms 原样转发的等待量，实际单位须看Impl；当前POSIX毫秒、CMSIS原生tick。
     * @return 取得令牌为true，超时或实现错误为false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryWaitFor(uint32_t timeout_ms) = 0;
    /**
     * @brief 读取后端当前令牌数。
     * @return 瞬时计数，不预留令牌，后续并发可立即改变它。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int getValue() const = 0;
    /**
     * @brief 在没有并发使用者的准备阶段请求重设计数。
     * @param initialValue 符合后端非负范围与最大计数的初值。
     * @note void不报告实际重设数量，不能与等待、发送或销毁并发。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void init(int initialValue) = 0;
};

/**
 * @brief 借用具体计数信号量后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class SemaphoreVirtual : public ISemaphore {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit SemaphoreVirtual(Impl &impl) : impl_(impl) {}
    /**
     * @brief 等待并请求消费一枚信号量令牌。
     * @note void不提供独立失败结果，具体阻塞及错误处置由实现定义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void wait() override { impl_.wait(); }
    /**
     * @brief 请求发布一枚信号量令牌。
     * @note 返回不证明已唤醒线程或底层发布成功，满计数策略由实现规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void signal() override { impl_.signal(); }
    /**
     * @brief 尝试消费现有令牌，不请求等待未来信号。
     * @return 实现报告取得令牌时为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryWait() override { return impl_.tryWait(); }
    /**
     * @brief 在相对超时规则下尝试消费令牌。
     * @param timeout_ms 原样转发的等待量，实际单位须看Impl；当前POSIX毫秒、CMSIS原生tick。
     * @return 取得令牌为true，超时或实现错误为false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryWaitFor(uint32_t timeout_ms) override { return impl_.tryWaitFor(timeout_ms); }
    /**
     * @brief 读取后端当前令牌数。
     * @return 瞬时计数，不预留令牌，后续并发可立即改变它。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int getValue() const override { return impl_.getValue(); }
    /**
     * @brief 在没有并发使用者的准备阶段请求重设计数。
     * @param initialValue 符合后端非负范围与最大计数的初值。
     * @note void不报告实际重设数量，不能与等待、发送或销毁并发。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void init(int initialValue) override { impl_.init(initialValue); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};

/**
 * @brief 线程生命周期的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IThread {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IThread() = default;
    /**
     * @brief 使用std::function入口及借用参数启动线程。
     * @param name 线程名，复制与存活期由后端规定。
     * @param taskFunction 任务包装，按值传递可能复制并分配资源。
     * @param taskArgument 借用上下文，覆盖任务最后一次使用。
     * @param priority 后端优先级编码。
     * @param stack_size 栈大小请求，单位与零默认由Impl约定。
     * @param pstack 可选外部栈，存活期及对齐按后端要求。
     * @return 原样返回实现整数结果，不统一错误码或重复启动语义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority,
                      int stack_size, void *pstack) = 0;
    /**
     * @brief 请求实现停止线程。
     * @note 协作停止、终止及是否等待由Impl决定；不会自动停车或失能机械臂。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void stop() = 0;
    /**
     * @brief 按后端规则等待线程结束。
     * @note 对象及上下文仍须覆盖最后访问期，不能由当前线程错误自连接。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void join() = 0;
    /**
     * @brief 请求分离线程资源。
     * @note 不等于任务已结束，不据此立即释放借用上下文。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void detach() = 0;
    /**
     * @brief 请求挂起线程。
     * @return 实现返回的状态，不证明任意用户代码已立即停止。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int suspend() = 0;
    /**
     * @brief 请求恢复线程。
     * @return 实现返回的状态，不证明任务已获得CPU。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int resume() = 0;
    /**
     * @brief 查询实现维护的运行状态。
     * @return 瞬时状态；不是任务结束或内存可释放的同步证明。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isRunning() const = 0;
    /**
     * @brief 请求修改线程优先级。
     * @param priority 实现约定的优先级编码及权限范围。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void setPriority(int priority) = 0;
    /**
     * @brief 读取实现报告的线程优先级。
     * @return 后端编码，不能跨平台直接比较调度含义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int getPriority() const = 0;
};

/**
 * @brief 借用具体线程生命周期后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class ThreadVirtual : public IThread {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit ThreadVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 使用std::function入口及借用参数启动线程。
     * @param name 线程名，复制与存活期由后端规定。
     * @param taskFunction 任务包装，按值传递可能复制并分配资源。
     * @param taskArgument 借用上下文，覆盖任务最后一次使用。
     * @param priority 后端优先级编码。
     * @param stack_size 栈大小请求，单位与零默认由Impl约定。
     * @param pstack 可选外部栈，存活期及对齐按后端要求。
     * @return 原样返回实现整数结果，不统一错误码或重复启动语义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int start(const char *name, std::function<void(void *)> taskFunction, void *taskArgument, int priority,
              int stack_size, void *pstack) override {
        return impl_.start(name, taskFunction, taskArgument, priority, stack_size, pstack);
    }
    /**
     * @brief 请求实现停止线程。
     * @note 协作停止、终止及是否等待由Impl决定；不会自动停车或失能机械臂。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void stop() override { impl_.stop(); }
    /**
     * @brief 按后端规则等待线程结束。
     * @note 对象及上下文仍须覆盖最后访问期，不能由当前线程错误自连接。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void join() override { impl_.join(); }
    /**
     * @brief 请求分离线程资源。
     * @note 不等于任务已结束，不据此立即释放借用上下文。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void detach() override { impl_.detach(); }
    /**
     * @brief 请求挂起线程。
     * @return 实现返回的状态，不证明任意用户代码已立即停止。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int suspend() override { return impl_.suspend(); }
    /**
     * @brief 请求恢复线程。
     * @return 实现返回的状态，不证明任务已获得CPU。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int resume() override { return impl_.resume(); }
    /**
     * @brief 查询实现维护的运行状态。
     * @return 瞬时状态；不是任务结束或内存可释放的同步证明。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isRunning() const override { return impl_.isRunning(); }
    /**
     * @brief 请求修改线程优先级。
     * @param priority 实现约定的优先级编码及权限范围。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void setPriority(int priority) override { impl_.setPriority(priority); }
    /**
     * @brief 读取实现报告的线程优先级。
     * @return 后端编码，不能跨平台直接比较调度含义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int getPriority() const override { return impl_.getPriority(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};

/**
 * @brief 类型化FIFO消息队列的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 * @tparam T 消息类型，满足所选具体后端的复制与存储要求。
 */
template <typename T>
class IQueue {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IQueue() = default;
    /**
     * @brief 按实现规则发送一条消息。
     * @param message 调用期间有效、满足后端复制要求的消息。
     * @note void不提供失败状态；阻塞和容量行为由Impl规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void send(const T &message) = 0;
    /**
     * @brief 按实现规则等待并返回一条消息。
     * @return 后端提供的T对象；此接口没有独立错误状态。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual T receive() = 0;
    /**
     * @brief 不请求等待地尝试接收消息。
     * @param message 成功时写入的新消息，失败时不能当作新输入使用。
     * @return 本次接收成功为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryReceive(T &message) = 0;
    /**
     * @brief 按相对超时接收消息。
     * @param message 成功时接收消息的输出对象。
     * @param timeout_ms 原样转发的等待量，单位随后端；POSIX毫秒、CMSIS原生tick。
     * @return 成功接收为true，超时或后端失败为false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool receiveFor(T &message, uint32_t timeout_ms) = 0;
    /**
     * @brief 查询当前队列长度。
     * @return 瞬时消息数，不锁定后续接收结果。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual size_t size() const = 0;
    /**
     * @brief 请求实现丢弃队列消息。
     * @note 与并发发送/接收的关系由后端决定，不提供跨调用事务。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void clear() = 0;
};

/**
 * @brief 借用具体类型化FIFO消息队列后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam T 消息类型，满足所选具体后端的复制与存储要求。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl, typename T>
class QueueVirtual : public IQueue<T> {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit QueueVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 按实现规则发送一条消息。
     * @param message 调用期间有效、满足后端复制要求的消息。
     * @note void不提供失败状态；阻塞和容量行为由Impl规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void send(const T &message) override { impl_.send(message); }
    /**
     * @brief 按实现规则等待并返回一条消息。
     * @return 后端提供的T对象；此接口没有独立错误状态。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    T receive() override { return impl_.receive(); }
    /**
     * @brief 不请求等待地尝试接收消息。
     * @param message 成功时写入的新消息，失败时不能当作新输入使用。
     * @return 本次接收成功为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryReceive(T &message) override { return impl_.tryReceive(message); }
    /**
     * @brief 按相对超时接收消息。
     * @param message 成功时接收消息的输出对象。
     * @param timeout_ms 原样转发的等待量，单位随后端；POSIX毫秒、CMSIS原生tick。
     * @return 成功接收为true，超时或后端失败为false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool receiveFor(T &message, uint32_t timeout_ms) override { return impl_.receiveFor(message, timeout_ms); }
    /**
     * @brief 查询当前队列长度。
     * @return 瞬时消息数，不锁定后续接收结果。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    size_t size() const override { return impl_.size(); }
    /**
     * @brief 请求实现丢弃队列消息。
     * @note 与并发发送/接收的关系由后端决定，不提供跨调用事务。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void clear() override { impl_.clear(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};

#if DP_OSAL_ENABLE_RW_LOCK
/**
 * @brief 读写锁的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IRWLock {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IRWLock() = default;
    /**
     * @brief 请求共享读锁，阻塞规则由实现决定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void readLock() = 0;
    /**
     * @brief 尝试取得共享读锁。
     * @return 本次成功取得为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryReadLock() = 0;
    /**
     * @brief 带超时请求共享读锁。
     * @param timeout_ms 原样传入Impl的相对超时，包装层不转换或校验物理单位。
     * @return 取得读锁为true，否则false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool readLockFor(uint32_t timeout_ms) = 0;
    /**
     * @brief 释放一次共享读锁。
     * @note 调用者遵守后端锁归属和配对要求。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void readUnlock() = 0;
    /**
     * @brief 请求独占写锁，阻塞/公平性规则由实现决定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void writeLock() = 0;
    /**
     * @brief 尝试取得独占写锁。
     * @return 本次成功取得为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryWriteLock() = 0;
    /**
     * @brief 带超时请求独占写锁。
     * @param timeout_ms 原样传入Impl的相对超时，实际单位看后端契约。
     * @return 取得写锁为true，否则false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool writeLockFor(uint32_t timeout_ms) = 0;
    /**
     * @brief 释放一次独占写锁。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void writeUnlock() = 0;
    /**
     * @brief 查询实现报告的当前读锁数量。
     * @return 瞬时观察值，不是后续获取权限。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual size_t getReadLockCount() const = 0;
    /**
     * @brief 查询实现报告的写锁状态。
     * @return 瞬时是否持有写锁，不能替代实际获取锁。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isWriteLocked() const = 0;
};

/**
 * @brief 借用具体读写锁后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class RWLockVirtual : public IRWLock {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit RWLockVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 请求共享读锁，阻塞规则由实现决定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void readLock() override { impl_.readLock(); }
    /**
     * @brief 尝试取得共享读锁。
     * @return 本次成功取得为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryReadLock() override { return impl_.tryReadLock(); }
    /**
     * @brief 带超时请求共享读锁。
     * @param timeout_ms 原样传入Impl的相对超时，包装层不转换或校验物理单位。
     * @return 取得读锁为true，否则false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool readLockFor(uint32_t timeout_ms) override { return impl_.readLockFor(timeout_ms); }
    /**
     * @brief 释放一次共享读锁。
     * @note 调用者遵守后端锁归属和配对要求。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void readUnlock() override { impl_.readUnlock(); }
    /**
     * @brief 请求独占写锁，阻塞/公平性规则由实现决定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void writeLock() override { impl_.writeLock(); }
    /**
     * @brief 尝试取得独占写锁。
     * @return 本次成功取得为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryWriteLock() override { return impl_.tryWriteLock(); }
    /**
     * @brief 带超时请求独占写锁。
     * @param timeout_ms 原样传入Impl的相对超时，实际单位看后端契约。
     * @return 取得写锁为true，否则false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool writeLockFor(uint32_t timeout_ms) override { return impl_.writeLockFor(timeout_ms); }
    /**
     * @brief 释放一次独占写锁。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void writeUnlock() override { impl_.writeUnlock(); }
    /**
     * @brief 查询实现报告的当前读锁数量。
     * @return 瞬时观察值，不是后续获取权限。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    size_t getReadLockCount() const override { return impl_.getReadLockCount(); }
    /**
     * @brief 查询实现报告的写锁状态。
     * @return 瞬时是否持有写锁，不能替代实际获取锁。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isWriteLocked() const override { return impl_.isWriteLocked(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

#if DP_OSAL_ENABLE_SPIN_LOCK
/**
 * @brief 自旋锁的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class ISpinLock {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~ISpinLock() = default;
    /**
     * @brief 按实现策略等待并取得锁。
     * @note “自旋”名称不保证所有后端都纯忙等，具体上下文限制由Impl规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void lock() = 0;
    /**
     * @brief 尝试取得锁。
     * @return 本次成功取得为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool tryLock() = 0;
    /**
     * @brief 带超时请求取得锁。
     * @param timeout_ms 不经换算传给Impl的相对超时数值。
     * @return 取得锁为true，超时或后端失败为false。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool lockFor(uint32_t timeout_ms) = 0;
    /**
     * @brief 释放实现中的锁。
     * @note 必须遵守锁归属与配对要求。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void unlock() = 0;
    /**
     * @brief 查询实现维护的锁状态。
     * @return 瞬时观测，不构成访问受保护数据的授权。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isLocked() const = 0;
};

/**
 * @brief 借用具体自旋锁后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class SpinLockVirtual : public ISpinLock {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit SpinLockVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 按实现策略等待并取得锁。
     * @note “自旋”名称不保证所有后端都纯忙等，具体上下文限制由Impl规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void lock() override { impl_.lock(); }
    /**
     * @brief 尝试取得锁。
     * @return 本次成功取得为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool tryLock() override { return impl_.tryLock(); }
    /**
     * @brief 带超时请求取得锁。
     * @param timeout_ms 不经换算传给Impl的相对超时数值。
     * @return 取得锁为true，超时或后端失败为false。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool lockFor(uint32_t timeout_ms) override { return impl_.lockFor(timeout_ms); }
    /**
     * @brief 释放实现中的锁。
     * @note 必须遵守锁归属与配对要求。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void unlock() override { impl_.unlock(); }
    /**
     * @brief 查询实现维护的锁状态。
     * @return 瞬时观测，不构成访问受保护数据的授权。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isLocked() const override { return impl_.isLocked(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

#if DP_OSAL_ENABLE_CONDITION_VAR
/**
 * @brief 条件变量的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IConditionVariable {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IConditionVariable() = default;
    /**
     * @brief 配合具体Mutex等待条件通知。
     * @param mutex 所选具体Mutex对象，不是任意IMutex；持锁/释放重获规则依后端。
     * @note 调用者仍须循环检查业务谓词，通知不等于条件已满足。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void wait(Mutex &mutex) = 0;
    /**
     * @brief 配合具体Mutex按相对超时等待。
     * @param mutex 等待所使用的具体Mutex，生命周期覆盖等待。
     * @param timeout_ms 原样转发的相对等待数值，实际单位按Impl解释。
     * @return 实现报告的等待结果，不能直接等同业务谓词已成立。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool waitFor(Mutex &mutex, uint32_t timeout_ms) = 0;
    /**
     * @brief 请求通知一个等待者。
     * @note 不保证目标立即运行，也不保存业务条件本身。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void notifyOne() = 0;
    /**
     * @brief 请求通知全部等待者，醒来后仍须检查各自谓词。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void notifyAll() = 0;
    /**
     * @brief 查询实现报告的等待者数量。
     * @return 后端瞬时计数，不能据此保证未来通知一定有接收者。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int getWaitCount() const = 0;
};

/**
 * @brief 借用具体条件变量后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class ConditionVariableVirtual : public IConditionVariable {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit ConditionVariableVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 配合具体Mutex等待条件通知。
     * @param mutex 所选具体Mutex对象，不是任意IMutex；持锁/释放重获规则依后端。
     * @note 调用者仍须循环检查业务谓词，通知不等于条件已满足。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void wait(Mutex &mutex) override { impl_.wait(mutex); }
    /**
     * @brief 配合具体Mutex按相对超时等待。
     * @param mutex 等待所使用的具体Mutex，生命周期覆盖等待。
     * @param timeout_ms 原样转发的相对等待数值，实际单位按Impl解释。
     * @return 实现报告的等待结果，不能直接等同业务谓词已成立。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool waitFor(Mutex &mutex, uint32_t timeout_ms) override { return impl_.waitFor(mutex, timeout_ms); }
    /**
     * @brief 请求通知一个等待者。
     * @note 不保证目标立即运行，也不保存业务条件本身。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void notifyOne() override { impl_.notifyOne(); }
    /**
     * @brief 请求通知全部等待者，醒来后仍须检查各自谓词。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void notifyAll() override { impl_.notifyAll(); }
    /**
     * @brief 查询实现报告的等待者数量。
     * @return 后端瞬时计数，不能据此保证未来通知一定有接收者。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int getWaitCount() const override { return impl_.getWaitCount(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

#if DP_OSAL_ENABLE_TIMER
/**
 * @brief OS软件定时器的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class ITimer {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~ITimer() = default;
    /**
     * @brief 把间隔、周期标记及回调转发到实现。
     * @param interval_ms 间隔数值，形参名为毫秒但此层不做后端tick转换。
     * @param periodic 是否请求周期触发。
     * @param callback 按值传递的回调，复制可能分配；执行线程/ISR上下文由后端规定。
     * @note 这是OS软件定时器包装，与无后台线程的StaticTimer不同。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void start(uint32_t interval_ms, bool periodic, std::function<void()> callback) = 0;
    /**
     * @brief 请求停止底层定时器。
     * @note 不额外等待已开始的回调，也不自动释放回调上下文；保证范围看Impl。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void stop() = 0;
    /**
     * @brief 查询实现报告的运行状态。
     * @return 瞬时状态，不单独证明回调已经退出。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isRunning() const = 0;
    /**
     * @brief 查询实现报告的剩余时间。
     * @return 后端数值，单位及停止态返回约定由Impl定义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t getRemainingTime() const = 0;
    /**
     * @brief 请求重置实现中的定时器状态或期限。
     * @note 具体重置含义与已运行回调关系按Impl，不由包装层扩展。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void reset() = 0;
};

/**
 * @brief 借用具体OS软件定时器后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class TimerVirtual : public ITimer {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit TimerVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 把间隔、周期标记及回调转发到实现。
     * @param interval_ms 间隔数值，形参名为毫秒但此层不做后端tick转换。
     * @param periodic 是否请求周期触发。
     * @param callback 按值传递的回调，复制可能分配；执行线程/ISR上下文由后端规定。
     * @note 这是OS软件定时器包装，与无后台线程的StaticTimer不同。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void start(uint32_t interval_ms, bool periodic, std::function<void()> callback) override {
        impl_.start(interval_ms, periodic, callback);
    }
    /**
     * @brief 请求停止底层定时器。
     * @note 不额外等待已开始的回调，也不自动释放回调上下文；保证范围看Impl。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void stop() override { impl_.stop(); }
    /**
     * @brief 查询实现报告的运行状态。
     * @return 瞬时状态，不单独证明回调已经退出。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isRunning() const override { return impl_.isRunning(); }
    /**
     * @brief 查询实现报告的剩余时间。
     * @return 后端数值，单位及停止态返回约定由Impl定义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t getRemainingTime() const override { return impl_.getRemainingTime(); }
    /**
     * @brief 请求重置实现中的定时器状态或期限。
     * @note 具体重置含义与已运行回调关系按Impl，不由包装层扩展。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void reset() override { impl_.reset(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

#if DP_OSAL_ENABLE_THREAD_POOL
/**
 * @brief 线程池的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IThreadPool {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IThreadPool() = default;
    /**
     * @brief 请求按指定线程数及线程属性启动池。
     * @param num_threads 初始线程数量请求。
     * @param priority 后端线程优先级。
     * @param stack_size 后端栈大小请求，单位及默认由Impl规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void start(uint32_t num_threads, int priority, int stack_size) = 0;
    /**
     * @brief 请求停止池，任务排空/取消/等待策略由Impl保证。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void stop() = 0;
    /**
     * @brief 请求挂起池。
     * @return 后端整数状态，不证明所有任务已经立刻停止。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int suspend() = 0;
    /**
     * @brief 请求恢复池。
     * @return 后端整数状态。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int resume() = 0;
    /**
     * @brief 查询池是否处于实现定义的已启动状态。
     * @return 后端状态观测值。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isStarted() const = 0;
    /**
     * @brief 查询实现维护的挂起状态。
     * @return 后端状态观测值，不是任务停止屏障。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool isSuspended() const = 0;
    /**
     * @brief 按值传递任务包装并提交借用参数与优先级。
     * @param task_function 可调用任务，复制可能分配。
     * @param task_argument 借用上下文，覆盖排队及执行最后使用期。
     * @param priority 任务优先级编码，排序规则由Impl决定。
     * @return 后端返回的任务编号/结果，失败编码与编号有效域由实现定义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t submit(std::function<void(void *)> task_function, void *task_argument, int priority) = 0;
    /**
     * @brief 请求修改池的线程优先级。
     * @param priority 后端优先级编码。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void setPriority(int priority) = 0;
    /**
     * @brief 读取后端报告的池优先级。
     * @return 实现约定的整数优先级。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual int getPriority() const = 0;
    /**
     * @brief 查询当前排队任务数。
     * @return 瞬时队列长度，不代表已完成任务数。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual size_t getTaskQueueSize() = 0;
    /**
     * @brief 查询后端报告的活跃线程数。
     * @return 实现定义的活跃数量，本包装不推导忙闲或调度状态。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t getActiveThreadCount() const = 0;
    /**
     * @brief 按任务编号请求取消。
     * @param task_id 由后端返回并仍适用于当前池的任务编号。
     * @return 实现报告取消成功为true；已开始任务是否可取消由Impl规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool cancelTask(uint32_t task_id) = 0;
    /**
     * @brief 按任务包装引用请求取消。
     * @param task_function 待匹配的任务对象，匹配与可取消条件由Impl规定。
     * @return 后端报告取消成功为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool cancelTask(std::function<void(void *)> &task_function) = 0;
    /**
     * @brief 设置任务失败通知回调。
     * @param callback 按值传递的回调，触发上下文、参数内容与存活期按Impl契约。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void setTaskFailureCallback(std::function<void(void *)> callback) = 0;
    /**
     * @brief 请求配置线程数上限。
     * @param max_threads 后端接受的上限值，与最小值的一致性由实现约束。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void setMaxThreads(uint32_t max_threads) = 0;
    /**
     * @brief 读取后端线程数上限。
     * @return 实现当前配置，不表示当前真实线程数。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t getMaxThreads() const = 0;
    /**
     * @brief 请求配置线程数下限。
     * @param min_threads 后端接受的下限值，是否立即扩缩由实现决定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void setMinThreads(uint32_t min_threads) = 0;
    /**
     * @brief 读取后端线程数下限。
     * @return 实现当前配置，不保证全部线程此时已创建或获调度。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t getMinThreads() const = 0;
};

/**
 * @brief 借用具体线程池后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class ThreadPoolVirtual : public IThreadPool {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit ThreadPoolVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 请求按指定线程数及线程属性启动池。
     * @param num_threads 初始线程数量请求。
     * @param priority 后端线程优先级。
     * @param stack_size 后端栈大小请求，单位及默认由Impl规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void start(uint32_t num_threads, int priority, int stack_size) override { impl_.start(num_threads, priority, stack_size); }
    /**
     * @brief 请求停止池，任务排空/取消/等待策略由Impl保证。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void stop() override { impl_.stop(); }
    /**
     * @brief 请求挂起池。
     * @return 后端整数状态，不证明所有任务已经立刻停止。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int suspend() override { return impl_.suspend(); }
    /**
     * @brief 请求恢复池。
     * @return 后端整数状态。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int resume() override { return impl_.resume(); }
    /**
     * @brief 查询池是否处于实现定义的已启动状态。
     * @return 后端状态观测值。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isStarted() const override { return impl_.isStarted(); }
    /**
     * @brief 查询实现维护的挂起状态。
     * @return 后端状态观测值，不是任务停止屏障。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool isSuspended() const override { return impl_.isSuspended(); }
    /**
     * @brief 按值传递任务包装并提交借用参数与优先级。
     * @param task_function 可调用任务，复制可能分配。
     * @param task_argument 借用上下文，覆盖排队及执行最后使用期。
     * @param priority 任务优先级编码，排序规则由Impl决定。
     * @return 后端返回的任务编号/结果，失败编码与编号有效域由实现定义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t submit(std::function<void(void *)> task_function, void *task_argument, int priority) override {
        return impl_.submit(task_function, task_argument, priority);
    }
    /**
     * @brief 请求修改池的线程优先级。
     * @param priority 后端优先级编码。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void setPriority(int priority) override { impl_.setPriority(priority); }
    /**
     * @brief 读取后端报告的池优先级。
     * @return 实现约定的整数优先级。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    int getPriority() const override { return impl_.getPriority(); }
    /**
     * @brief 查询当前排队任务数。
     * @return 瞬时队列长度，不代表已完成任务数。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    size_t getTaskQueueSize() override { return impl_.getTaskQueueSize(); }
    /**
     * @brief 查询后端报告的活跃线程数。
     * @return 实现定义的活跃数量，本包装不推导忙闲或调度状态。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t getActiveThreadCount() const override { return impl_.getActiveThreadCount(); }
    /**
     * @brief 按任务编号请求取消。
     * @param task_id 由后端返回并仍适用于当前池的任务编号。
     * @return 实现报告取消成功为true；已开始任务是否可取消由Impl规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool cancelTask(uint32_t task_id) override { return impl_.cancelTask(task_id); }
    /**
     * @brief 按任务包装引用请求取消。
     * @param task_function 待匹配的任务对象，匹配与可取消条件由Impl规定。
     * @return 后端报告取消成功为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool cancelTask(std::function<void(void *)> &task_function) override { return impl_.cancelTask(task_function); }
    /**
     * @brief 设置任务失败通知回调。
     * @param callback 按值传递的回调，触发上下文、参数内容与存活期按Impl契约。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void setTaskFailureCallback(std::function<void(void *)> callback) override { impl_.setTaskFailureCallback(callback); }
    /**
     * @brief 请求配置线程数上限。
     * @param max_threads 后端接受的上限值，与最小值的一致性由实现约束。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void setMaxThreads(uint32_t max_threads) override { impl_.setMaxThreads(max_threads); }
    /**
     * @brief 读取后端线程数上限。
     * @return 实现当前配置，不表示当前真实线程数。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t getMaxThreads() const override { return impl_.getMaxThreads(); }
    /**
     * @brief 请求配置线程数下限。
     * @param min_threads 后端接受的下限值，是否立即扩缩由实现决定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void setMinThreads(uint32_t min_threads) override { impl_.setMinThreads(min_threads); }
    /**
     * @brief 读取后端线程数下限。
     * @return 实现当前配置，不保证全部线程此时已创建或获调度。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t getMinThreads() const override { return impl_.getMinThreads(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

#if DP_OSAL_ENABLE_CHRONO
/**
 * @brief tick时钟的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IChrono {
public:
    using TimePoint = uint32_t; ///< 可回绕的后端原生tick，频率由ticksPerSecond报告。
    using Duration = double; ///< elapsed返回的浮点秒数。

    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IChrono() = default;
    /**
     * @brief 读取当前后端原生tick。
     * @return 可回绕的32位tick，单位需结合ticksPerSecond，不能固定当毫秒。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual TimePoint now() const = 0;
    /**
     * @brief 查询原生tick每秒频率。
     * @return 后端频率Hz；旧Mock未实现本能力时返回0，表示不可用。
     */
    virtual uint32_t ticksPerSecond() const { return 0U; }
    /**
     * @brief 请求等待到绝对tick期限。
     * @param absolute_deadline 与now同域、满足半圈比较前提的32位期限。
     * @note 旧Mock未实现本能力时明确终止测试进程，不伪装等待完成；真实适配器覆盖此方法。
     */
    virtual void sleepUntil(TimePoint absolute_deadline) const {
        (void)absolute_deadline;
        std::abort();
    }
    /**
     * @brief 把两个tick之间的差换算为秒。
     * @param start 起始tick。
     * @param end 结束tick，须符合后端回绕窗口约束。
     * @return 浮点秒数。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual Duration elapsed(const TimePoint &start, const TimePoint &end) const = 0;
    /**
     * @brief 按后端频率将tick换为整秒表示。
     * @param time_point 原生tick。
     * @return 秒数，不构成单调起点到UTC的真实校准。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual std::time_t to_time_t(const TimePoint &time_point) const = 0;
    /**
     * @brief 把秒数按后端频率缩放成tick。
     * @param time 待转换秒数，范围由调用者按Impl约束。
     * @return 后端32位tick，溢出/负值处置依实现。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual TimePoint from_time_t(std::time_t time) const = 0;
    /**
     * @brief 将tick交给后端生成显示文本。
     * @param time_point 待格式化的原生tick。
     * @return 后端字符串，日历外观不代表已同步真实日期。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual std::string to_string(const TimePoint &time_point) const = 0;
};

/**
 * @brief 借用具体tick时钟后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class ChronoVirtual : public IChrono {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit ChronoVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 读取当前后端原生tick。
     * @return 可回绕的32位tick，单位需结合ticksPerSecond，不能固定当毫秒。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    TimePoint now() const override { return impl_.now(); }
    /**
     * @brief 查询原生tick每秒频率。
     * @return 后端频率Hz，供调用者换算。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t ticksPerSecond() const override { return impl_.ticksPerSecond(); }
    /**
     * @brief 请求等待到绝对tick期限。
     * @param absolute_deadline 与now同域、满足半圈比较前提的32位期限。
     * @note 已过期限及底层等待失败处置由Impl决定，此层不提供额外状态。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void sleepUntil(TimePoint absolute_deadline) const override { impl_.sleepUntil(absolute_deadline); }
    /**
     * @brief 把两个tick之间的差换算为秒。
     * @param start 起始tick。
     * @param end 结束tick，须符合后端回绕窗口约束。
     * @return 浮点秒数。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    Duration elapsed(const TimePoint &start, const TimePoint &end) const override { return impl_.elapsed(start, end); }
    /**
     * @brief 按后端频率将tick换为整秒表示。
     * @param time_point 原生tick。
     * @return 秒数，不构成单调起点到UTC的真实校准。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    std::time_t to_time_t(const TimePoint &time_point) const override { return impl_.to_time_t(time_point); }
    /**
     * @brief 把秒数按后端频率缩放成tick。
     * @param time 待转换秒数，范围由调用者按Impl约束。
     * @return 后端32位tick，溢出/负值处置依实现。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    TimePoint from_time_t(std::time_t time) const override { return impl_.from_time_t(time); }
    /**
     * @brief 将tick交给后端生成显示文本。
     * @param time_point 待格式化的原生tick。
     * @return 后端字符串，日历外观不代表已同步真实日期。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    std::string to_string(const TimePoint &time_point) const override { return impl_.to_string(time_point); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

/**
 * @brief 系统/调度服务的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class ISystem {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~ISystem() = default;
    /**
     * @brief 请求后端启动调度器，宿主环境可为空操作。
     * @note 是否返回与失败处置由Impl规定。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void StartScheduler() = 0;
    /**
     * @brief 按毫秒请求当前线程休眠。
     * @param milliseconds 相对毫秒数，调度与时钟粒度影响真实恢复时刻。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void sleep_ms(uint32_t milliseconds) const = 0;
    /**
     * @brief 按整秒请求当前线程休眠。
     * @param seconds 相对秒数，范围及停止唤醒规则看Impl。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void sleep(uint32_t seconds) const = 0;
    /**
     * @brief 读取后端换算后的系统毫秒计数。
     * @return 可回绕的32位值，不用于真实日期或跨域比较。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual uint32_t get_tick_ms() const = 0;
    /**
     * @brief 进入后端定义的临界区。
     * @note 当前POSIX为空操作，CMSIS锁调度器不等于屏蔽所有中断；与退出配对。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void enter_critical() const = 0;
    /**
     * @brief 退出后端定义的临界区。
     * @note 嵌套及恢复要求由后端规定，包装层不保存先前状态。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void exit_critical() const = 0;
    /**
     * @brief 获取系统说明文本。
     * @return 后端借用字符串指针，调用者不释放。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual const char *get_system_info() const = 0;
#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    /**
     * @brief 在快照开关启用时写出有界线程诊断。
     * @param buf 调用者提供至少max项的输出区。
     * @param max 允许写入的最大条目数。
     * @return 实际写入条目数，各字段有效性和时间域仍须单独检查。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const = 0;
#endif
};

/**
 * @brief 借用具体系统/调度服务后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class SystemVirtual : public ISystem {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit SystemVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 请求后端启动调度器，宿主环境可为空操作。
     * @note 是否返回与失败处置由Impl规定。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void StartScheduler() override { impl_.StartScheduler(); }
    /**
     * @brief 按毫秒请求当前线程休眠。
     * @param milliseconds 相对毫秒数，调度与时钟粒度影响真实恢复时刻。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void sleep_ms(uint32_t milliseconds) const override { impl_.sleep_ms(milliseconds); }
    /**
     * @brief 按整秒请求当前线程休眠。
     * @param seconds 相对秒数，范围及停止唤醒规则看Impl。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void sleep(uint32_t seconds) const override { impl_.sleep(seconds); }
    /**
     * @brief 读取后端换算后的系统毫秒计数。
     * @return 可回绕的32位值，不用于真实日期或跨域比较。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    uint32_t get_tick_ms() const override { return impl_.get_tick_ms(); }
    /**
     * @brief 进入后端定义的临界区。
     * @note 当前POSIX为空操作，CMSIS锁调度器不等于屏蔽所有中断；与退出配对。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void enter_critical() const override { impl_.enter_critical(); }
    /**
     * @brief 退出后端定义的临界区。
     * @note 嵌套及恢复要求由后端规定，包装层不保存先前状态。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void exit_critical() const override { impl_.exit_critical(); }
    /**
     * @brief 获取系统说明文本。
     * @return 后端借用字符串指针，调用者不释放。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    const char *get_system_info() const override { return impl_.get_system_info(); }
#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    /**
     * @brief 在快照开关启用时写出有界线程诊断。
     * @param buf 调用者提供至少max项的输出区。
     * @param max 允许写入的最大条目数。
     * @return 实际写入条目数，各字段有效性和时间域仍须单独检查。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const override {
        return impl_.get_thread_snapshot(buf, max);
    }
#endif

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};

#if DP_OSAL_ENABLE_MEMORY_MANAGER
/**
 * @brief 内存管理的可替换纯虚接口。
 * @note 用于Mock/依赖注入；纯虚声明本身不实现OS行为或验证安全属性。
 */
class IMemoryManager {
public:
    /**
     * @brief 虚析构支持经接口指针销毁派生适配器或Mock，资源清理由派生类负责。
     */
    virtual ~IMemoryManager() = default;
    /**
     * @brief 按实现约定准备内存块管理器。
     * @param block_size 单块大小请求，单位字节。
     * @param block_count 块数量请求。
     * @return 实现报告初始化成功为true。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual bool initialize(size_t block_size, size_t block_count) = 0;
    /**
     * @brief 请求分配内存。
     * @param size 请求字节数。
     * @return 后端指针，失败约定及零字节行为由Impl定义。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void *allocate(size_t size) = 0;
    /**
     * @brief 把先前分配的指针归还对应实现。
     * @param ptr 满足该后端归属、有效性及重复释放约束的指针。
     * @note 包装不检测越界、归属或二次释放。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void deallocate(void *ptr) = 0;
    /**
     * @brief 请求调整内存块大小。
     * @param ptr 原内存块，归属与失败后有效性按后端契约。
     * @param new_size 目标字节数。
     * @return 后端返回的新指针/失败结果，不能在不知道实现规则时提前释放旧块。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void *reallocate(void *ptr, size_t new_size) = 0;
    /**
     * @brief 请求分配满足对齐要求的内存。
     * @param size 请求字节数。
     * @param alignment 对齐字节数，合法范围与幂次约束由Impl检查。
     * @return 后端指针，包装不额外验证对齐。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual void *allocateAligned(size_t size, size_t alignment) = 0;
    /**
     * @brief 查询后端报告的已分配字节数。
     * @return 实现统计值，本包装不自行计数或证明无泄露。
     * @details 纯虚契约，实际执行及错误处理由派生实现提供。
     */
    virtual size_t getAllocatedSize() const = 0;
};

/**
 * @brief 借用具体内存管理后端的虚调用适配器。
 * @note 所有操作转发到impl_；外部保证后端存活并满足线程/资源契约。
 * @tparam Impl 被借用的具体OSAL后端，存活期覆盖所有适配器调用。
 */
template <typename Impl>
class MemoryManagerVirtual : public IMemoryManager {
public:
    /**
     * @brief 借用后端对象构造适配器，不复制或接管后端。
     * @param impl 外部持有且覆盖适配器使用期的具体后端。
     */
    explicit MemoryManagerVirtual(Impl &impl) : impl_(impl) {}

    /**
     * @brief 按实现约定准备内存块管理器。
     * @param block_size 单块大小请求，单位字节。
     * @param block_count 块数量请求。
     * @return 实现报告初始化成功为true。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    bool initialize(size_t block_size, size_t block_count) override { return impl_.initialize(block_size, block_count); }
    /**
     * @brief 请求分配内存。
     * @param size 请求字节数。
     * @return 后端指针，失败约定及零字节行为由Impl定义。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void *allocate(size_t size) override { return impl_.allocate(size); }
    /**
     * @brief 把先前分配的指针归还对应实现。
     * @param ptr 满足该后端归属、有效性及重复释放约束的指针。
     * @note 包装不检测越界、归属或二次释放。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void deallocate(void *ptr) override { impl_.deallocate(ptr); }
    /**
     * @brief 请求调整内存块大小。
     * @param ptr 原内存块，归属与失败后有效性按后端契约。
     * @param new_size 目标字节数。
     * @return 后端返回的新指针/失败结果，不能在不知道实现规则时提前释放旧块。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void *reallocate(void *ptr, size_t new_size) override { return impl_.reallocate(ptr, new_size); }
    /**
     * @brief 请求分配满足对齐要求的内存。
     * @param size 请求字节数。
     * @param alignment 对齐字节数，合法范围与幂次约束由Impl检查。
     * @return 后端指针，包装不额外验证对齐。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    void *allocateAligned(size_t size, size_t alignment) override { return impl_.allocateAligned(size, alignment); }
    /**
     * @brief 查询后端报告的已分配字节数。
     * @return 实现统计值，本包装不自行计数或证明无泄露。
     * @details 原样转发到借用后端impl_，不新增重试、超时换算或资源所有权。
     */
    size_t getAllocatedSize() const override { return impl_.getAllocatedSize(); }

private:
    Impl &impl_; ///< 借用后端引用，不由适配器delete；外部保证存活与并发协调。
};
#endif

} // namespace dp::osal

#endif  // DP_OSAL_VIRTUAL_H_
