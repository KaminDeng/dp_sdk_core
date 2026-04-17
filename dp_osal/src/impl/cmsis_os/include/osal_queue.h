//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_CMSIS_QUEUE_H_
#define OSAL_CMSIS_QUEUE_H_

#include "interface_queue.h"
#include "osal.h"
#include "osal_debug.h"

namespace osal {

template <typename T>
class OSALMessageQueue : public QueueBase<OSALMessageQueue<T>, T> {
    friend class QueueBase<OSALMessageQueue<T>, T>;

public:
    explicit OSALMessageQueue(uint32_t queue_size = 16) {
        osMessageQueueAttr_t queueAttr = {};
        queueAttr.name = "MessageQueue";
        queue_ = osMessageQueueNew(queue_size, sizeof(T), &queueAttr);
        if (queue_ == nullptr) {
            OSAL_LOGE("Failed to create message queue\n");
            // 处理消息队列创建失败的情况
        }
    }

    ~OSALMessageQueue() {
        if (queue_ != nullptr) {
            osMessageQueueDelete(queue_);
        }
    }

private:
    void doSend(const T &message) {
        if (osMessageQueuePut(queue_, &message, 0, osWaitForever) != osOK) {
            OSAL_LOGE("Failed to send message\n");
        } else {
            OSAL_LOGD("Message sent\n");
        }
    }

    bool doTrySend(const T &message) {
        if (osMessageQueuePut(queue_, &message, 0, 0) == osOK) {
            OSAL_LOGD("Message try-sent\n");
            return true;
        }
        OSAL_LOGD("Message try-send failed (queue full)\n");
        return false;
    }

    T doReceive() {
        T message;
        if (osMessageQueueGet(queue_, &message, nullptr, osWaitForever) != osOK) {
            OSAL_LOGE("Failed to receive message\n");
            // 处理接收消息失败的情况
        } else {
            OSAL_LOGD("Message received\n");
        }
        return message;
    }

    bool doTryReceive(T &message) {
        if (osMessageQueueGet(queue_, &message, nullptr, 0) != osOK) {
            return false;
        }
        OSAL_LOGD("Message try-received\n");
        return true;
    }

    bool doReceiveFor(T &message, uint32_t timeout) {
        if (osMessageQueueGet(queue_, &message, nullptr, timeout) != osOK) {
            return false;
        }
        OSAL_LOGD("Message received with timeout\n");
        return true;
    }

    [[nodiscard]] size_t doSize() const { return osMessageQueueGetCount(queue_); }

    void doClear() {
        while (osMessageQueueGetCount(queue_) > 0) {
            T message;
            osMessageQueueGet(queue_, &message, nullptr, 0);
        }
        OSAL_LOGD("Message queue cleared\n");
    }
    osMessageQueueId_t queue_;
};

}  // namespace osal

#endif  // OSAL_CMSIS_QUEUE_H_
