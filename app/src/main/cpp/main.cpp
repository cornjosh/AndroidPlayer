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

// 获取应用的内部存储目录
std::string getFilesDir(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getFilesDirMethod = env->GetMethodID(contextClass, "getFilesDir", "()Ljava/io/File;");
    jobject fileObject = env->CallObjectMethod(context, getFilesDirMethod);

    jclass fileClass = env->GetObjectClass(fileObject);
    jmethodID getPathMethod = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
    jstring pathString = (jstring)env->CallObjectMethod(fileObject, getPathMethod);

    const char* path = env->GetStringUTFChars(pathString, nullptr);
    std::string result(path);
    env->ReleaseStringUTFChars(pathString, path);

    return result;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_MainActivity_nativeInit(JNIEnv *env, jclass clazz, jobject context) {

    std::string filesDir = "/storage/emulated/0";
    std::string inputFilePath = filesDir + "/testfile.mp4";
    std::string outputFilePath = filesDir + "/output.yuv";
    std::string testFilePath = filesDir + "/test.txt";


    const char* input_file = inputFilePath.c_str();
    const char* output_file = outputFilePath.c_str();

    LOGI("Starting video processing...");
    LOGI("Input file path: %s", input_file);
    LOGI("Output file path: %s", output_file);

    char WriteFileFolder[100];

    sprintf(WriteFileFolder, "%s/DocumentTest", testFilePath.c_str());

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