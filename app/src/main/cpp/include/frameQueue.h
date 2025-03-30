//
// Created by zylnt on 2025/3/30.
//

#ifndef ANDROIDPLAYER_FRAMEQUEUE_H
#define ANDROIDPLAYER_FRAMEQUEUE_H


extern "C" {
#include "libavutil/frame.h"
}

#include <queue>
#include <mutex>
#include <condition_variable>

class FrameQueue {
public:
    FrameQueue();
    ~FrameQueue();

    void push(AVFrame *frame);
    AVFrame* pop();
    void clear();
    void setFinished(bool isFinished);
    bool isFinished() const;

private:
    std::queue<AVFrame*> queue;
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;
};


#endif //ANDROIDPLAYER_FRAMEQUEUE_H
