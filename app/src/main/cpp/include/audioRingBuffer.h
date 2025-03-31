//
// audioRingBuffer.h
// Created by zylnt on 2025/3/31.
//

#ifndef ANDROIDPLAYER_AUDIORINGBUFFER_H
#define ANDROIDPLAYER_AUDIORINGBUFFER_H


#include <mutex>
#include <condition_variable>

class AudioRingBuffer {
public:
    AudioRingBuffer(size_t capacity = 96000000000); // ~5秒音频
    ~AudioRingBuffer();

    void write(const uint8_t* data, size_t len);
    size_t read(uint8_t* buffer, size_t len);
    void clear();

    void setFinished(bool val);
    bool isFinished();
    bool isEmpty();

private:
    uint8_t* buffer;
    size_t capacity;
    size_t readPos = 0;
    size_t writePos = 0;
    size_t size = 0;

    std::mutex mutex;
    std::condition_variable cond;
    bool finished = false;
};


#endif //ANDROIDPLAYER_AUDIORINGBUFFER_H
