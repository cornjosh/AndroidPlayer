#include <jni.h>
#include <string>
#include <thread>
#include "log.h"
#define TAG "player"
#include "packetQueue.h"
#include "frameQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <android/native_window_jni.h>
}

//
// Created by zylnt on 2025/3/30.
//

// 全局资源
static PacketQueue* packetQueue = nullptr;
static FrameQueue* frameQueue = nullptr;
static AVFormatContext* formatCtx = nullptr;
static int videoStreamIndex = -1;
static AVRational videoTimeBase;
static ANativeWindow* nativeWindow = nullptr;
static std::string videoPath;

// 线程句柄
static std::thread demuxerThread;
static std::thread decoderThread;
static std::thread rendererThread;

extern void demuxThread(const char* path, PacketQueue* queue, int* streamIndexOut);
extern void decodeThread(PacketQueue* packetQueue, FrameQueue* frameQueue, AVCodecParameters* codecpar);
extern void renderThread(FrameQueue* frameQueue, ANativeWindow* window, AVRational time_base);


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv *env, jobject thiz, jstring file,
                                                 jobject surface) {
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
//    av_register_all();
    avformat_network_init();

    packetQueue = new PacketQueue();
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

    // 找到视频流索引
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            videoTimeBase = formatCtx->streams[i]->time_base;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        LOGE("❌ No video stream found.");
        return -3;
    }

    LOGI("📦 Starting demux/decode/render threads...");

    demuxerThread = std::thread(demuxThread, videoPath.c_str(), packetQueue, &videoStreamIndex);
    decoderThread = std::thread(decodeThread, packetQueue, frameQueue,
                                formatCtx->streams[videoStreamIndex]->codecpar);
    rendererThread = std::thread(renderThread, frameQueue, nativeWindow, videoTimeBase);

    // 可选：detach 或 join 管理线程生命周期
    demuxerThread.detach();
    decoderThread.detach();
    rendererThread.detach();

    return 0;
}



extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv *env, jobject thiz, jboolean p) {
    // TODO: implement nativePause()
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv *env, jobject thiz, jdouble position) {
    // TODO: implement nativeSeek()
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeStop()
    return 0;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv *env, jobject thiz, jfloat speed) {
    // TODO: implement nativeSetSpeed()
    return 0;
}


extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetPosition()
    return 0;
}



extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetDuration()
    return 0;
}