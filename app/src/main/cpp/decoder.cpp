// decoder.cpp
#include <iostream>
#include <thread>
#include "queue.h"
#include "log.h"  // 包含日志头文件
#include "ffmpeg.h"

#define LOG_TAG "Decoder"  // 定义日志标签

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}


void decode(VideoPacketQueue& packet_queue, const char* output_file) {
    LOGI("Starting the decoding process");  // 开始解码过程的信息日志

    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("Could not find codec");
        return;
    }
    LOGV("Codec found successfully");  // 成功找到解码器的详细日志

    // 分配解码器上下文
    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOGE("Could not allocate codec context");
        return;
    }
    LOGV("Codec context allocated successfully");  // 成功分配解码器上下文的详细日志

    // 打开解码器
    int ret = avcodec_open2(codec_context, codec, nullptr);
    if (ret < 0) {
        LOGE("Could not open codec");
        print_ffmpeg_error(ret);
        avcodec_free_context(&codec_context);
        return;
    }
    LOGV("Codec opened successfully");  // 成功打开解码器的详细日志

    // 分配视频帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOGE("Could not allocate video frame");
        avcodec_free_context(&codec_context);
        return;
    }
    LOGV("Video frame allocated successfully");  // 成功分配视频帧的详细日志

    // 打开输出文件
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        LOGE("Could not open output file");
        av_frame_free(&frame);
        avcodec_free_context(&codec_context);
        return;
    }
    LOGV("Output file opened successfully");  // 成功打开输出文件的详细日志

    // 从队列中获取数据包并解码
    while (true) {
        AVPacket* packet = packet_queue.get();
        if (!packet) {
            break;
        }

        // 发送数据包到解码器
        ret = avcodec_send_packet(codec_context, packet);
        if (ret < 0) {
            LOGE("Error sending packet to decoder");
            print_ffmpeg_error(ret);
            av_packet_free(&packet);
            continue;
        }

        // 接收解码后的帧
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                LOGE("Error receiving frame from decoder");
                print_ffmpeg_error(ret);
                break;
            }

            // 将帧以 YUV420P 格式写入文件
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

        av_packet_free(&packet);
    }

    // 刷新解码器
    avcodec_send_packet(codec_context, nullptr);
    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_context, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE("Error receiving frame from decoder");
            print_ffmpeg_error(ret);
            break;
        }

        // 将帧以 YUV420P 格式写入文件
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

    // 释放资源
    fclose(output);
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    LOGI("Decoding process completed");  // 解码过程完成的信息日志
}    