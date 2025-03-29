//
// Created by zylnt on 2025/3/29.
//

#include "queue.h"
#include "log.h"  // 包含日志头文件

#define LOG_TAG "VideoPacketQueue"  // 定义日志标签

VideoPacketQueue::VideoPacketQueue() {
    LOGV("VideoPacketQueue constructor called");  // 构造函数调用的详细日志
}

VideoPacketQueue::~VideoPacketQueue() {
    LOGV("VideoPacketQueue destructor called");  // 析构函数调用的详细日志
    std::lock_guard<std::mutex> lock(mutex);
    while (!queue.empty()) {
        AVPacket* packet = queue.front();
        queue.pop();
        av_packet_free(&packet);
    }
    LOGV("All packets in the queue have been freed");  // 队列中所有包已释放的详细日志
}

void VideoPacketQueue::put(AVPacket* packet) {
    LOGV("Attempting to put a packet into the queue");  // 尝试将包放入队列的详细日志
    std::unique_lock<std::mutex> lock(mutex);
    queue.push(packet);
    cond.notify_one();
    LOGV("A packet has been successfully put into the queue");  // 包成功放入队列的详细日志
}

int VideoPacketQueue::get(AVPacket* packet, bool block) {
    LOGV("Attempting to get a packet from the queue, block mode: %s", block ? "true" : "false");  // 尝试从队列获取包的详细日志
    std::unique_lock<std::mutex> lock(mutex);
    if (block) {
        while (queue.empty()) {
            LOGV("Queue is empty, waiting for a packet...");  // 队列空，等待包的详细日志
            cond.wait(lock);
        }
    } else {
        if (queue.empty()) {
            LOGE("Queue is empty and non-blocking mode, cannot get a packet");  // 队列空且非阻塞模式，无法获取包的错误日志
            return -1;
        }
    }
    AVPacket* pkt = queue.front();
    queue.pop();
    av_packet_move_ref(packet, pkt);
    av_packet_free(&pkt);
    LOGV("A packet has been successfully retrieved from the queue");  // 包成功从队列获取的详细日志
    return 0;
}