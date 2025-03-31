//
// audioRingBuffer.cpp
// Created by zylnt on 2025/3/31.
//

#include "audioRingBuffer.h"
#include "log.h"
#define TAG "AudioRingBuffer"
#include <cstring>
#include <algorithm>

AudioRingBuffer::AudioRingBuffer(size_t cap) : capacity(cap) {
    buffer = new uint8_t[capacity];
}

AudioRingBuffer::~AudioRingBuffer() {
    delete[] buffer;
}

void AudioRingBuffer::write(const uint8_t* data, size_t len) {
    std::unique_lock<std::mutex> lock(mutex);
    for (size_t i = 0; i < len; ++i) {
        if (size >= capacity) break;
        buffer[writePos] = data[i];
        writePos = (writePos + 1) % capacity;
        size++;
    }
    cond.notify_all();
    LOGD("💾 Written to ringBuffer, size=%zu, current size=%zu", len, size);
}

size_t AudioRingBuffer::read(uint8_t* out, size_t len) {
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this]() { return size > 0 || finished; });

    size_t toRead = std::min(len, size);
    for (size_t i = 0; i < toRead; ++i) {
        out[i] = buffer[readPos];
        readPos = (readPos + 1) % capacity;
        size--;
    }
    LOGD("🎵 Read %zu bytes from ringBuffer, current size=%zu", toRead, size);

    return toRead;
}


void AudioRingBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    readPos = writePos = size = 0;
}

void AudioRingBuffer::setFinished(bool val) {
    std::lock_guard<std::mutex> lock(mutex);
    finished = val;
    cond.notify_all();
}

bool AudioRingBuffer::isFinished() {
    std::lock_guard<std::mutex> lock(mutex);
    return finished;
}

bool AudioRingBuffer::isEmpty() {
    std::lock_guard<std::mutex> lock(mutex);
    return size == 0;
}
