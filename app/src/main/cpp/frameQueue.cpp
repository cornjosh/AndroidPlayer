//
// Created by zylnt on 2025/3/30.
//

#include "frameQueue.h"

FrameQueue::FrameQueue() {}

FrameQueue::~FrameQueue() {
    clear();
}

void FrameQueue::push(AVFrame *frame) {
    std::unique_lock<std::mutex> lock(mtx);
    queue.push(frame);
    cv.notify_one();
}

AVFrame* FrameQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !queue.empty() || finished; });

    if (queue.empty()) return nullptr;

    AVFrame *frame = queue.front();
    queue.pop();
    return frame;
}

void FrameQueue::clear() {
    std::unique_lock<std::mutex> lock(mtx);
    while (!queue.empty()) {
        AVFrame *frame = queue.front();
        av_frame_free(&frame);
        queue.pop();
    }
}

void FrameQueue::setFinished(bool isFinished) {
    std::unique_lock<std::mutex> lock(mtx);
    finished = isFinished;
    cv.notify_all();
}

bool FrameQueue::isFinished() const {
    std::unique_lock<std::mutex> lock(mtx);
    return finished;
}

bool FrameQueue::empty() const {
    std::unique_lock<std::mutex> lock(mtx);
    return queue.empty();
}
