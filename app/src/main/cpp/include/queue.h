//
// Created by zylnt on 2025/3/29.
//

#ifndef QUEUE_H
#define QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

class VideoPacketQueue {
public:
    VideoPacketQueue();
    ~VideoPacketQueue();

    void put(AVPacket* packet);
    int get(AVPacket* packet, bool block);

private:
    std::queue<AVPacket*> queue;
    std::mutex mutex;
    std::condition_variable cond;
};

#endif // QUEUE_H