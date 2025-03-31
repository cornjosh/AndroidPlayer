#include <AAudio/AAudio.h>
#include "audioRingBuffer.h"
#include "log.h"
#define TAG "AAudioPlayer"

double getAudioClock(AAudioStream *pStruct);

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// 播放音频数据的线程函数
void AAudioPlayerThread(AudioRingBuffer* ringBuffer) {
    AAudioStream* stream = nullptr;

    LOGI("🔊 Starting AAudio player thread");

    if (ringBuffer == nullptr) {
        LOGE("❌ RingBuffer is nullptr");
        return;
    }

    // 创建 AAudioStreamBuilder 实例
    AAudioStreamBuilder *builder;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        LOGE("❌ Failed to create AAudioStreamBuilder: %s", AAudio_convertResultToText(result));
        return;
    }

    // 设置音频流参数
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);  // 立体声
    AAudioStreamBuilder_setSampleRate(builder, 44100); // 采样率
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // 或 SHARED 也行
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);


    // 尝试打开音频流
    aaudio_result_t result1 = AAudioStreamBuilder_openStream(builder, &stream);  // 改为 aaudio_result_t

    LOGI("🎛 Opened stream info:");
    LOGI("   Sample rate: %d", AAudioStream_getSampleRate(stream));
    LOGI("   Channel count: %d", AAudioStream_getChannelCount(stream));
    LOGI("   Format: %d", AAudioStream_getFormat(stream));
    LOGI("   Buffer capacity: %d", AAudioStream_getBufferCapacityInFrames(stream));
    LOGI("   Frames per burst: %d", AAudioStream_getFramesPerBurst(stream));

    AAudioStream_requestStart(stream);

    if (result1 != AAUDIO_OK) {
        LOGE("❌ Failed to open AAudio stream: %s", AAudio_convertResultToText(result));
        return;
    }

    LOGI("✅ AAudio stream successfully opened");

    AAudioStream_requestStart(stream);

    const int bufferSize = 2048;
    uint8_t buffer[bufferSize];

    while (true) {
        size_t bytesRead = ringBuffer->read(buffer, bufferSize);
        if (bytesRead > 0) {
            int framesToWrite = bytesRead / (2 * sizeof(int16_t)); // stereo, 16-bit
            AAudioStream_write(stream, buffer, framesToWrite, 100000000);
            // 计算当前音频PTS
            double audioPts = getAudioClock(stream);
            LOGD("🎧 Audio PTS: %.3f sec", audioPts);
        } else {
            if (ringBuffer->isFinished() && ringBuffer->isEmpty()) {
                LOGI("🎉 Audio ring buffer fully played!");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 关闭流
    LOGI("🛑 Closing AAudio stream");
    AAudioStream_requestStop(stream);
    AAudioStream_close(stream);
    LOGI("✅ AAudio stream closed");
}

double getAudioClock(AAudioStream *stream) {

    // 获取已经写入的帧数
    int64_t frames = AAudioStream_getFramesWritten(stream);
    int sampleRate = AAudioStream_getSampleRate(stream);

    if (sampleRate <= 0) return 0.0;

    return (double)frames / sampleRate; // 单位：秒
}

