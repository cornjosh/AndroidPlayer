//
// Created by zylnt on 2025/3/29.
//

#include <iostream>
#include <thread>

#include "queue.h"
#include "demuxer.h"
#include "decoder.h"
#include <jni.h>
#include "log.h"

#define LOG_TAG "Main"

extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_MainActivity_nativeInit(JNIEnv *env, jclass clazz) {
    const char* input_file = "testfile.mp4";
    const char* output_file = "output.yuv";

    LOGI("Starting video processing...");

    // 创建队列
    VideoPacketQueue packet_queue;

    // 创建解封装线程
    std::thread demux_thread(demux, input_file, std::ref(packet_queue));

    // 创建解码线程
    std::thread decode_thread(decode, std::ref(packet_queue), output_file);

    // 等待解封装线程完成
    demux_thread.join();

    // 等待解码线程完成
    decode_thread.join();
}