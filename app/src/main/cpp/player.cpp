#include <jni.h>
#include <string>
#include <stdio.h>
#include <thread>
#include <android/native_window_jni.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include "frame_queue.h"

// 声明 renderer 线程入口函数（在 renderer.cpp 中实现）
extern "C" void* renderThread(void* arg);

//
// Created by zylnt on 2025/3/30.
//

FrameQueue* g_frameQueue = nullptr;

struct RenderContext {
    ANativeWindow* window;
    FrameQueue* frameQueue;
};

const char* input_file = nullptr;
const char* output_file = nullptr;
void decode();
void demux();

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv *env, jobject thiz, jstring file,
                                                 jobject surface) {
    const char* filePath = env->GetStringUTFChars(file, nullptr);

    // 初始化全局帧队列（如果还未创建）
    if (!g_frameQueue) {
        g_frameQueue = new FrameQueue();
    }

    // 从 Java Surface 获取 ANativeWindow，用于 EGL 渲染
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);

    // 启动渲染线程，将 nativeWindow 与帧队列传递给它
    RenderContext renderCtx;
    renderCtx.window = nativeWindow;
    renderCtx.frameQueue = g_frameQueue;
    std::thread render_thread(renderThread, &renderCtx);

    std::string filesDir = "/storage/emulated/0";
    std::string inputFilePath = filesDir + "/testfile.mp4";
    std::string outputFilePath = filesDir + "/output.yuv";
    input_file = inputFilePath.c_str();
    output_file = outputFilePath.c_str();

    std::thread demux_thread(demux);
    std::thread decode_thread(decode);
    demux_thread.join();
    decode_thread.join();
    render_thread.join();
    return 0;
}

void init_renderer(JNIEnv *pEnv, jobject pJobject) {

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
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeStop()
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv *env, jobject thiz, jfloat speed) {
    // TODO: implement nativeSetSpeed()
}


extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetPosition()
}



extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetDuration()
}