//
// Created by zylnt on 2025/3/30.
//

#ifndef ANDROIDPLAYER_PACKETQUEUE_H
#define ANDROIDPLAYER_PACKETQUEUE_H


extern "C" {
#include "libavcodec/avcodec.h"
}

#include <queue>
#include <mutex>
#include <condition_variable>

class PacketQueue {
public:
    PacketQueue();
    ~PacketQueue();

    void push(AVPacket *pkt);
    AVPacket* pop();
    void clear();
    void setFinished(bool isFinished);
    bool isFinished() const;
    bool empty() const;

private:
    std::queue<AVPacket*> queue;
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;
};


#endif //ANDROIDPLAYER_PACKETQUEUE_H
