//
// Created by zylnt on 2025/3/29.
//

#include <iostream>
#include <thread>
#include "queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

void decode(VideoPacketQueue& packet_queue, const char* output_file) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "Codec not found" << std::endl;
        return;
    }

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        std::cerr << "Could not allocate codec context" << std::endl;
        return;
    }

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return;
    }

    FILE* output = fopen(output_file, "wb");
    if (!output) {
        std::cerr << "Could not open output file" << std::endl;
        return;
    }

    AVPacket packet;
    while (packet_queue.get(&packet, true) == 0) {
        if (avcodec_send_packet(codec_context, &packet) < 0) {
            std::cerr << "Error sending packet to decoder" << std::endl;
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
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
}