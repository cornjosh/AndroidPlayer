// demuxer.cpp
#include <iostream>
#include <thread>
#include "queue.h"
#include "log.h"

#define LOG_TAG "Demuxer"  // 定义日志标签


extern "C" {
#include <libavformat/avformat.h>
}


// 解封装函数，将输入文件解封装为视频数据包并放入队列
void demux() {
    LOGI("Starting demuxing process");

    AVFormatContext* format_context = nullptr;
    int ret = avformat_open_input(&format_context, input_file, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Failed to open input file: %s", input_file);
        return;
    }
    LOGV("Successfully opened input file");

    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        LOGE("Failed to find stream information");
        avformat_close_input(&format_context);
        return;
    }

    // 查找视频流
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            codec_par = format_context->streams[i]->codecpar;
            codec = avcodec_find_decoder(codec_par->codec_id);
            if (!codec) {
                LOGE("Failed to find decoder for codec: %s",
                     avcodec_get_name(codec_par->codec_id));
                break;
            }
            LOGV("Found decoder: %s", codec->name);
            break;
        }
    }

    if (video_stream_index == -1 || !codec) {
        LOGE("No valid video stream found");
        avformat_close_input(&format_context);
        return;
    }

    // 读取数据包并放入队列
    AVPacket* packet = av_packet_alloc();
    while ((ret = av_read_frame(format_context, packet)) >= 0) {
        if (packet->stream_index == video_stream_index) {
            AVPacket* new_packet = av_packet_clone(packet);
            if (new_packet) {
                packet_queue.put(new_packet);
                LOGV("Packet added to queue (size=%d)", new_packet->size);
            }
        }
        av_packet_unref(packet);
    }

    if (ret != AVERROR_EOF) {
        LOGE("Error reading packets: %s", av_err2str(ret));
    }

    av_packet_free(&packet);
    avformat_close_input(&format_context);
    LOGI("Demuxing process completed");

    // 通知解码线程初始化完成
    {
        std::lock_guard<std::mutex> lock(init_mutex);
        init_complete = true;
    }
    init_cond.notify_one();
}