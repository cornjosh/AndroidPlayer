// demuxer.cpp
#include <iostream>
#include <thread>
#include "queue.h"
#include "log.h"  // 包含日志头文件
#include "ffmpeg.h"

#define LOG_TAG "Demuxer"  // 定义日志标签

extern "C" {
#include <libavformat/avformat.h>
}


// 解封装函数，将输入文件解封装为视频数据包并放入队列
void demux(const char* input_file, VideoPacketQueue& packet_queue) {
    LOGI("Starting the demuxing process");

    // 初始化 AVFormatContext 用于处理输入文件的格式信息
    AVFormatContext* format_context = nullptr;
    int ret = avformat_open_input(&format_context, input_file, nullptr, nullptr);
    if (ret != 0) {
        LOGE("Failed to open input file: %s", input_file);
        print_ffmpeg_error(ret);
        return;
    }
    LOGV("Successfully opened input file: %s", input_file);

    // 查找输入文件的流信息
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        LOGE("Failed to find stream information");
        print_ffmpeg_error(ret);
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Successfully found stream information");

    // 遍历所有流，输出每个流的类型和编解码器 ID 信息
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        AVStream* stream = format_context->streams[i];
        LOGV("Stream #%d: Type: %s, Codec ID: %s",
             i,
             av_get_media_type_string(stream->codecpar->codec_type),
             avcodec_get_name(stream->codecpar->codec_id));
    }

    // 查找视频流的索引
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOGE("Could not find video stream in the input file");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Successfully found video stream at index: %d", video_stream_index);

    // 获取视频流的详细参数并输出日志
    AVStream* video_stream = format_context->streams[video_stream_index];
    LOGV("Video stream parameters:");
    LOGV("  Codec ID: %s", avcodec_get_name(video_stream->codecpar->codec_id));
    LOGV("  Width: %d", video_stream->codecpar->width);
    LOGV("  Height: %d", video_stream->codecpar->height);
    LOGV("  Frame rate: %d/%d", video_stream->avg_frame_rate.num, video_stream->avg_frame_rate.den);
    LOGV("  Pixel format: %d", video_stream->codecpar->format);

    // 分配 AVPacket 用于存储从文件中读取的数据包
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOGE("Failed to allocate AVPacket");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Successfully allocated AVPacket");

    // 循环读取输入文件中的数据包
    while ((ret = av_read_frame(format_context, packet)) >= 0) {
        LOGV("Read packet: stream_index=%d, size=%d, pts=%" PRId64,
             packet->stream_index, packet->size, packet->pts);

        // 如果读取的数据包是视频流的数据包
        if (packet->stream_index == video_stream_index) {
            // 克隆数据包以避免数据竞争问题
            AVPacket* new_packet = av_packet_clone(packet);
            if (!new_packet) {
                LOGE("Failed to clone AVPacket");
                continue;
            }
            // 将克隆后的数据包放入队列
            packet_queue.put(new_packet);
            LOGV("Video packet cloned and added to queue (size=%d, pts=%" PRId64,
                 new_packet->size, new_packet->pts);
        }
        // 释放当前数据包的引用
        av_packet_unref(packet);
    }

    // 处理读取文件时可能出现的错误（除了文件结束标志）
    if (ret < 0 && ret != AVERROR_EOF) {
        LOGE("Error while reading from input file");
        print_ffmpeg_error(ret);
    }

    // 释放 AVPacket 资源
    av_packet_free(&packet);
    LOGV("Successfully freed AVPacket");
    // 关闭输入文件的格式上下文
    avformat_close_input(&format_context);
    LOGV("Successfully closed input file");
    LOGI("Demuxing process completed");
}