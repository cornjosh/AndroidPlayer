// queue.h
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
    AVPacket* get();

private:
    std::queue<AVPacket*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

#endif // QUEUE_H