//
// Created by zylnt on 2025/3/29.
//
#include <iostream>
#include <thread>
#include "queue.h"
#include "log.h"  // 包含日志头文件

#define LOG_TAG "Demuxer"  // 定义日志标签

extern "C" {
#include <libavformat/avformat.h>
}

void demux(const char* input_file, VideoPacketQueue& packet_queue) {
    LOGI("Starting the demuxing process");  // 开始解复用过程的信息日志
    AVFormatContext* format_context = nullptr;
    if (avformat_open_input(&format_context, input_file, nullptr, nullptr) != 0) {
        LOGE("Could not open input file");
        return;
    }
    LOGV("Input file opened successfully");  // 成功打开输入文件的详细日志

    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        LOGE("Could not find stream information");
        return;
    }
    LOGV("Stream information found successfully");  // 成功找到流信息的详细日志

    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOGE("Could not find video stream");
        return;
    }
    LOGV("Video stream found successfully, index: %d", video_stream_index);  // 成功找到视频流的详细日志

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOGE("Could not allocate AVPacket");
        return;
    }
    LOGV("AVPacket allocated successfully");  // 成功分配 AVPacket 的详细日志

    while (av_read_frame(format_context, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            AVPacket* new_packet = av_packet_clone(packet);
            if (!new_packet) {
                LOGE("Could not clone AVPacket");
                continue;
            }
            packet_queue.put(new_packet);
            LOGV("Video packet cloned and added to queue");  // 成功克隆并添加视频包到队列的详细日志
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    LOGV("AVPacket freed successfully");  // 成功释放 AVPacket 的详细日志
    avformat_close_input(&format_context);
    LOGV("Input file closed successfully");  // 成功关闭输入文件的详细日志
    LOGI("Demuxing process completed");  // 解复用过程完成的信息日志
}