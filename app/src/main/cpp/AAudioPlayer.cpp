// AAudioPlayer.cpp
#include <AAudio/AAudio.h>
#include "audioRingBuffer.h"
#include "log.h"
#define TAG "AAudioPlayer"


#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "timer.h"

double getAudioClock(AAudioStream *pStruct);

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

            // 获取当前音频的PTS（播放时间）
            double audioPts = getAudioClock(stream);
            // 获取主时钟的当前时间
            double masterClock = Timer::getCurrentTime();

            // 计算音频与主时钟的时间差
            double timeToSync = audioPts - masterClock;

            // 根据时间差调整播放延迟（单位：微秒）
            int64_t delayUs = static_cast<int64_t>((framesToWrite / 44100.0) * 1000000.0);

            // 如果音频落后于主时钟，减少延迟（加速播放）
            if (timeToSync < 0) {
                delayUs += static_cast<int64_t>(-timeToSync * 1000000);
            }
                // 如果音频超前于主时钟，增加延迟（减慢播放）
            else if (timeToSync > 0) {
                delayUs -= static_cast<int64_t>(timeToSync * 1000000);
            }

            // 向音频流写入数据
            AAudioStream_write(stream, buffer, framesToWrite, delayUs);

            LOGD("🎧 Audio PTS: %.3f sec, Master Clock: %.3f sec, Delay: %ld us", audioPts, masterClock, delayUs);
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

