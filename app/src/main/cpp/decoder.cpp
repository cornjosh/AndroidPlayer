//
// Created by zylnt on 2025/3/29.
//

#include <iostream>
#include <thread>
#include "queue.h"
#include "log.h"  // 包含日志头文件

#define LOG_TAG "Decoder"  // 定义日志标签

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

void decode(VideoPacketQueue& packet_queue, const char* output_file) {
    LOGI("Starting the decoding process");  // 输出信息日志，表示解码过程开始
    const AVCodec* codec = avcodec_find_decoder_by_name("h264");
    if (!codec) {
        LOGE("Codec not found");
        return;
    }
    LOGV("Codec found successfully");  // 输出详细日志，表示成功找到编解码器

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOGE("Could not allocate codec context");
        return;
    }
    LOGV("Codec context allocated successfully");  // 输出详细日志，表示成功分配编解码器上下文

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        LOGE("Could not open codec");
        return;
    }
    LOGV("Codec opened successfully");  // 输出详细日志，表示成功打开编解码器

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOGE("Could not allocate video frame");
        return;
    }
    LOGV("Video frame allocated successfully");  // 输出详细日志，表示成功分配视频帧

    FILE* output = fopen(output_file, "wb");
    if (!output) {
        LOGE("Could not open output file");
        return;
    }
    LOGV("Output file opened successfully");  // 输出详细日志，表示成功打开输出文件

    AVPacket packet;
    while (packet_queue.get(&packet, true) == 0) {
        if (avcodec_send_packet(codec_context, &packet) < 0) {
            LOGE("Error sending packet to decoder");
            continue;
        }

        while (avcodec_receive_frame(codec_context, frame) == 0) {
            for (int i = 0; i < frame->height; i++) {
                fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, output);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                fwrite(frame->data[1] + i * frame->linesize[1], 1, frame->width / 2, output);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                fwrite(frame->data[2] + i * frame->linesize[2], 1, frame->width / 2, output);
            }
        }
        av_packet_unref(&packet);
    }

    fclose(output);
    LOGV("Output file closed successfully");  // 输出详细日志，表示成功关闭输出文件
    av_frame_free(&frame);
    LOGV("Video frame freed successfully");  // 输出详细日志，表示成功释放视频帧
    avcodec_free_context(&codec_context);
    LOGV("Codec context freed successfully");  // 输出详细日志，表示成功释放编解码器上下文
    LOGI("Decoding process completed");  // 输出信息日志，表示解码过程完成
}