// queue.cpp
#include "queue.h"

VideoPacketQueue::VideoPacketQueue() {}

VideoPacketQueue::~VideoPacketQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        AVPacket* packet = queue_.front();
        queue_.pop();
        av_packet_free(&packet);
    }
}

void VideoPacketQueue::put(AVPacket* packet) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(packet);
    cond_.notify_one();
}

AVPacket* VideoPacketQueue::get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return!queue_.empty(); });
    AVPacket* packet = queue_.front();
    queue_.pop();
    return packet;
}