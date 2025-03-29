//
// Created by zylnt on 2025/3/29.
//
#include <iostream>
#include <thread>
#include "queue.h"

extern "C" {
#include <libavformat/avformat.h>
}

void demux(const char* input_file, VideoPacketQueue& packet_queue) {
    AVFormatContext* format_context = nullptr;
    if (avformat_open_input(&format_context, input_file, nullptr, nullptr) != 0) {
        std::cerr << "Could not open input file" << std::endl;
        return;
    }

    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        return;
    }

    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        return;
    }

    AVPacket* packet = av_packet_alloc();
    while (av_read_frame(format_context, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            AVPacket* new_packet = av_packet_clone(packet);
            packet_queue.put(new_packet);
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&format_context);
}