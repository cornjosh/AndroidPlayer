//
// Created by zylnt on 2025/3/29.
//

#include "queue.h"

VideoPacketQueue::VideoPacketQueue() {}

VideoPacketQueue::~VideoPacketQueue() {
    std::lock_guard<std::mutex> lock(mutex);
    while (!queue.empty()) {
        AVPacket* packet = queue.front();
        queue.pop();
        av_packet_free(&packet);
    }
}

void VideoPacketQueue::put(AVPacket* packet) {
    std::unique_lock<std::mutex> lock(mutex);
    queue.push(packet);
    cond.notify_one();
}

int VideoPacketQueue::get(AVPacket* packet, bool block) {
    std::unique_lock<std::mutex> lock(mutex);
    if (block) {
        while (queue.empty()) {
            cond.wait(lock);
        }
    } else {
        if (queue.empty()) {
            return -1;
        }
    }
    AVPacket* pkt = queue.front();
    queue.pop();
    av_packet_move_ref(packet, pkt);
    av_packet_free(&pkt);
    return 0;
}
