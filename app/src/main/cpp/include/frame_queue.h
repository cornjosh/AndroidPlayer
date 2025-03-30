//
// Created by zylnt on 2025/3/30.
//

#ifndef ANDROIDPLAYER_FRAME_QUEUE_H
#define ANDROIDPLAYER_FRAME_QUEUE_H

#include <queue>
#include <pthread.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

class FrameQueue {
private:
    std::queue<AVFrame*> queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_exit;

public:
    FrameQueue() {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);
        is_exit = false;
    }

    ~FrameQueue() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    void put(AVFrame* frame) {
        pthread_mutex_lock(&mutex);
        if (!is_exit) {
            queue.push(av_frame_clone(frame));
            pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&mutex);
    }

    AVFrame* get() {
        pthread_mutex_lock(&mutex);
        while (queue.empty() && !is_exit) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (queue.empty()) {
            pthread_mutex_unlock(&mutex);
            return nullptr;
        }
        AVFrame* frame = queue.front();
        queue.pop();
        pthread_mutex_unlock(&mutex);
        return frame;
    }

    void exit() {
        pthread_mutex_lock(&mutex);
        is_exit = true;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
};


#endif //ANDROIDPLAYER_FRAME_QUEUE_H
