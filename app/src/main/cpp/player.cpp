//
// player.cpp
// Created by zylnt on 2025/3/30.
//

#include <jni.h>
#include <string>
#include <thread>
#include "log.h"
#define TAG "player"
#include "packetQueue.h"
#include "frameQueue.h"
#include "audioRingBuffer.h"
#include "timer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <android/native_window_jni.h>
}

// 全局资源
static PacketQueue* packetQueue = nullptr;
static FrameQueue* frameQueue = nullptr;
static AVFormatContext* formatCtx = nullptr;
static int videoStreamIndex = -1;
static int audioStreamIndex = -1;
static AVRational videoTimeBase;
static ANativeWindow* nativeWindow = nullptr;
static std::string videoPath;
static PacketQueue* audioPacketQueue = nullptr;
static AudioRingBuffer* audioRingBuffer = new AudioRingBuffer(9600000);
static Timer timer;
static bool isInited = false; // 是否初始化完成




// 线程句柄
static std::thread demuxerThread;
static std::thread decoderThread;
static std::thread rendererThread;
static std::thread audioDecoderThread;
static std::thread aAudioPlayerThread;

extern void demuxThread(const char* path, PacketQueue* videoQueue, PacketQueue* audioQueue, int videoStreamIndex, int audioStreamIndex);
extern void decodeThread(PacketQueue* packetQueue, FrameQueue* frameQueue, AVCodecParameters* codecpar);
extern void renderThread(FrameQueue* frameQueue, ANativeWindow* window, AVRational time_base);
extern void audioDecodeThread(PacketQueue* packetQueue, AudioRingBuffer* ringBuffer, AVCodecParameters* codecpar);
extern void AAudioPlayerThread(AudioRingBuffer* ringBuffer);


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv *env, jobject thiz, jstring file,
                                                 jobject surface) {
    if (isInited){
        timer.resume();
        return 0;
    }
    Timer::isPlaying = true; // 设置为正在播放

    // 处理文件路径
    const char* src = env->GetStringUTFChars(file, nullptr);
    videoPath = src;
    env->ReleaseStringUTFChars(file, src);
    LOGI("📁 nativeSetDataSource: %s", videoPath.c_str());

    // 设置 surface
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (!nativeWindow) {
        LOGE("❌ ANativeWindow_fromSurface failed! surface is null.");
        return -1;
    }

    LOGI("✅ nativeSetSurface success: window=%p", nativeWindow);

    LOGI("▶️ nativeStart");

    // 初始化全局状态
    avformat_network_init();

    packetQueue = new PacketQueue();        // video
    audioPacketQueue = new PacketQueue();   // ✅ audio
    frameQueue = new FrameQueue();

    // 打开视频获取 AVFormatContext
    if (avformat_open_input(&formatCtx, videoPath.c_str(), nullptr, nullptr) != 0) {
        LOGE("❌ Failed to open input file.");
        return -1;
    }

    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        LOGE("❌ Failed to find stream info.");
        return -2;
    }

    // 找到视频流和音频流索引
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            videoTimeBase = formatCtx->streams[i]->time_base;
            LOGI("🎥 Video stream index: %d", videoStreamIndex);
        }
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioStreamIndex = i;
            LOGI("🎵 Audio stream index: %d", audioStreamIndex);
        }
    }

    if (videoStreamIndex == -1) {
        LOGE("❌ No video stream found.");
        return -3;
    }

    if (audioStreamIndex == -1) {
        LOGE("❌ No audio stream found.");
        return -4;
    }

    LOGI("📦 Starting demux/decode/render threads...");

    demuxerThread = std::thread(demuxThread, videoPath.c_str(), packetQueue, audioPacketQueue, videoStreamIndex, audioStreamIndex);
    decoderThread = std::thread(decodeThread, packetQueue, frameQueue,
                                formatCtx->streams[videoStreamIndex]->codecpar);
    rendererThread = std::thread(renderThread, frameQueue, nativeWindow, videoTimeBase);
    audioDecoderThread = std::thread(audioDecodeThread, audioPacketQueue, audioRingBuffer,
                                formatCtx->streams[audioStreamIndex]->codecpar);
    aAudioPlayerThread = std::thread(AAudioPlayerThread, audioRingBuffer);

    timer.setCurrentTime(0); // 设置初始时间为 0
    timer.setTimeSpeed(1.0); // 设置时间倍率为 1.0
    timer.start(); // 启动计时器线程
    LOGI("⏱️ Timer started with initial time: %.3f", Timer::getCurrentTime());


    // 可选：detach 或 join 管理线程生命周期
    demuxerThread.detach();
    decoderThread.detach();
    rendererThread.detach();
    audioDecoderThread.detach();
    aAudioPlayerThread.detach();

    isInited = true;

    return 0;
}



extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv *env, jobject thiz, jboolean p) {
    timer.pause();
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv *env, jobject thiz, jdouble position) {
    timer.setCurrentTime(position);
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv *env, jobject thiz) {
    LOGI("🛑 nativeStop called");
//    isPlaying = false;

    // 停止主时钟
    timer.pause();
    timer.setCurrentTime(0);
    Timer::isPlaying = false;

    // 释放 native window
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
        LOGI("🧹 Released ANativeWindow");
    }

    // 关闭并释放 AVFormatContext
    if (formatCtx) {
        avformat_close_input(&formatCtx);  // 自动释放 streams
        formatCtx = nullptr;
        LOGI("🧹 Closed AVFormatContext");
    }

    // 释放 PacketQueue（video）
    if (packetQueue) {
        delete packetQueue;
        packetQueue = nullptr;
        LOGI("🧹 Deleted video PacketQueue");
    }

    // 释放 PacketQueue（audio）
    if (audioPacketQueue) {
        delete audioPacketQueue;
        audioPacketQueue = nullptr;
        LOGI("🧹 Deleted audio PacketQueue");
    }

    // 释放 FrameQueue
    if (frameQueue) {
        delete frameQueue;
        frameQueue = nullptr;
        LOGI("🧹 Deleted FrameQueue");
    }

    // 重建 AudioRingBuffer（或你也可以添加 clear 函数）
    if (audioRingBuffer) {
        delete audioRingBuffer;
        audioRingBuffer = new AudioRingBuffer(9600000);  // 预分配一样大小
        LOGI("🔄 Reset AudioRingBuffer");
    }

    // 清空视频路径和流索引
    videoPath.clear();
    videoStreamIndex = -1;
    audioStreamIndex = -1;

    isInited = false;

    LOGI("✅ All playback resources cleaned up");
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv *env, jobject thiz, jfloat speed) {
    timer.setTimeSpeed(speed);
    return 0;
}


extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv *env, jobject thiz) {
    return timer.getCurrentTime();
}



extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv *env, jobject thiz) {
    if (formatCtx == nullptr) {
        LOGE("❌ Format context is null, cannot get duration");
        return -1; // 错误处理：返回 -1 表示无法获取时长
    }

    // 获取视频时长（单位：微秒），并转换为秒
    double durationInSeconds = formatCtx->duration / AV_TIME_BASE;
    LOGI("⏳ Video duration: %.3f seconds", durationInSeconds);

    return durationInSeconds;
}