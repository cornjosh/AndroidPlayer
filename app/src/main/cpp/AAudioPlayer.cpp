// AAudioPlayer.cpp

#include <AAudio/AAudio.h>
#include "audioRingBuffer.h"
#include "log.h"
#define TAG "AAudioPlayer"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// 播放音频数据的线程函数
void AAudioPlayerThread(AudioRingBuffer* ringBuffer) {
    AAudioStream* stream = nullptr;

    LOGI("🔊 Starting AAudio player thread");

    // 创建 AAudio 流
    AAudioStreamBuilder* builder = {};
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);  // 立体声
    AAudioStreamBuilder_setSampleRate(builder, 44100); // 采样率

    // 尝试打开音频流
    AAudioResult result = AAudioStreamBuilder_openStream(builder, &stream);

    if (result != AAUDIO_OK) {
        LOGE("❌ Failed to open AAudio stream: %s", AAudio_convertResultToText(result));
        return;
    }

    LOGI("✅ AAudio stream successfully opened");

    const size_t bufferSize = 192000; // 每次读取的最大字节数
    uint8_t buffer[bufferSize];

    while (true) {
        // 从环形缓冲区读取数据
        size_t dataRead = ringBuffer->read(buffer, bufferSize);

        // 如果读取到数据，写入到 AAudio 流
        if (dataRead > 0) {
            LOGD("🎵 Read %zu bytes of audio data from ringBuffer", dataRead);
            AAudioStream_write(stream, buffer, dataRead, 0);
        }

        // 如果缓冲区没有数据，稍微休眠，避免高CPU占用
        if (dataRead == 0) {
            LOGD("🔄 Ring buffer is empty, sleeping...");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 关闭流
    LOGI("🛑 Closing AAudio stream");
    AAudioStream_close(stream);
    LOGI("✅ AAudio stream closed");
}
