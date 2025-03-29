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

// 打印错误码对应的错误信息
void print_ffmpeg_error(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    LOGE("FFmpeg error: %s", errbuf);
}

void demux(const char* input_file, VideoPacketQueue& packet_queue) {
    LOGI("Starting the demuxing process");  // 开始解复用过程的信息日志

    // 打开输入文件
    AVFormatContext* format_context = nullptr;
    int ret = avformat_open_input(&format_context, input_file, nullptr, nullptr);
    if (ret != 0) {
        LOGE("Could not open input file");
        print_ffmpeg_error(ret);
        return;
    }
    LOGV("Input file opened successfully");  // 成功打开输入文件的详细日志


    // 查找流信息
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        LOGE("Could not find stream information");
        print_ffmpeg_error(ret);
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Stream information found successfully");  // 成功找到流信息的详细日志

    // 遍历所有流
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        AVStream* stream = format_context->streams[i];
        LOGV("Stream #%d: Type: %s, Codec ID: %s",
             i,
             av_get_media_type_string(stream->codecpar->codec_type),
             avcodec_get_name(stream->codecpar->codec_id));
    }

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOGE("Could not find video stream");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Video stream found successfully, index: %d", video_stream_index);  // 成功找到视频流的详细日志

    // 获取视频流参数
    AVStream* video_stream = format_context->streams[video_stream_index];
    LOGV("Video stream parameters:");
    LOGV("  Codec ID: %s", avcodec_get_name(video_stream->codecpar->codec_id));
    LOGV("  Width: %d", video_stream->codecpar->width);
    LOGV("  Height: %d", video_stream->codecpar->height);
    LOGV("  Frame rate: %d/%d", video_stream->avg_frame_rate.num, video_stream->avg_frame_rate.den);
    LOGV("  Pixel format: %d", video_stream->codecpar->format);

    // 分配数据包
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOGE("Could not allocate AVPacket");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("AVPacket allocated successfully");  // 成功分配 AVPacket 的详细日志

    // 读取数据包
    while (av_read_frame(format_context, packet) >= 0) {
        LOGV("Read packet: stream_index=%d, size=%d, pts=%" PRId64,
             packet->stream_index, packet->size, packet->pts);

        if (packet->stream_index == video_stream_index) {
            AVPacket* new_packet = av_packet_clone(packet);
            if (!new_packet) {
                LOGE("Could not clone AVPacket");
                continue;
            }
            packet_queue.put(new_packet);
            LOGV("Video packet cloned and added to queue (size=%d, pts=%" PRId64,
                 new_packet->size, new_packet->pts);
        }
        av_packet_unref(packet);
    }

    // 处理读取错误
    if (ret < 0) {
        LOGE("Error reading from input file");
        print_ffmpeg_error(ret);
    }

    av_packet_free(&packet);
    LOGV("AVPacket freed successfully");  // 成功释放 AVPacket 的详细日志
    avformat_close_input(&format_context);
    LOGV("Input file closed successfully");  // 成功关闭输入文件的详细日志
    LOGI("Demuxing process completed");  // 解复用过程完成的信息日志
}