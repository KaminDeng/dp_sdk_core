//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_POSIX_QUEUE_H_
#define OSAL_POSIX_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "interface_queue.h"
#include "osal_debug.h"

namespace osal {

template <typename T>
class OSALMessageQueue : public QueueBase<OSALMessageQueue<T>, T> {
    friend class QueueBase<OSALMessageQueue<T>, T>;

public:
    OSALMessageQueue() = default;

    ~OSALMessageQueue() = default;

private:
    void doSend(const T &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        OSAL_LOGD("Message sent\n");
        condVar_.notify_one();
    }

    bool doTrySend(const T &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        OSAL_LOGD("Message try-sent\n");
        condVar_.notify_one();
        return true;
    }

    T doReceive() {
        std::unique_lock<std::mutex> lock(mutex_);
        condVar_.wait(lock, [this] { return !queue_.empty(); });
        T message = queue_.front();
        queue_.pop();
        OSAL_LOGD("Message received\n");
        return message;
    }

    bool doTryReceive(T &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        message = queue_.front();
        queue_.pop();
        OSAL_LOGD("Message try-received\n");
        return true;
    }

    bool doReceiveFor(T &message, uint32_t timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condVar_.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return !queue_.empty(); })) {
            return false;
        }
        message = queue_.front();
        queue_.pop();
        OSAL_LOGD("Message received with timeout\n");
        return true;
    }

    size_t doSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void doClear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
        OSAL_LOGD("Message queue cleared\n");
    }

    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condVar_;
};

}  // namespace osal

#endif  // OSAL_POSIX_QUEUE_H_
