//
// Created by zylnt on 2025/3/30.
//

#include "packetQueue.h"

PacketQueue::PacketQueue() {}

PacketQueue::~PacketQueue() {
    clear();
}

void PacketQueue::push(AVPacket *pkt) {
    std::unique_lock<std::mutex> lock(mtx);
    queue.push(pkt);
    cv.notify_one();
}

AVPacket* PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !queue.empty() || finished; });

    if (queue.empty()) return nullptr;

    AVPacket *pkt = queue.front();
    queue.pop();
    return pkt;
}

void PacketQueue::clear() {
    std::unique_lock<std::mutex> lock(mtx);
    while (!queue.empty()) {
        AVPacket *pkt = queue.front();
        av_packet_free(&pkt);
        queue.pop();
    }
}

void PacketQueue::setFinished(bool isFinished) {
    std::unique_lock<std::mutex> lock(mtx);
    finished = isFinished;
    cv.notify_all();
}

bool PacketQueue::isFinished() const {
    std::unique_lock<std::mutex> lock(mtx);
    return finished;
}