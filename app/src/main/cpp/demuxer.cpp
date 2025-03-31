// demuxer.cpp
#include "log.h"
#define TAG "demuxer"
#include "packetQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <thread>

void demuxThread(const char* inputPath, PacketQueue* videoQueue, PacketQueue *audioQueue, int videoStreamIndex, int audioStreamIndex) {
    AVFormatContext *formatCtx = nullptr;

    LOGI("📂 Opening input: %s", inputPath);
    if (avformat_open_input(&formatCtx, inputPath, nullptr, nullptr) != 0) {
        LOGE("❌ Failed to open input: %s", inputPath);
        return;
    }

    LOGI("🔍 Finding stream info...");
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        LOGE("❌ Failed to find stream info");
        avformat_close_input(&formatCtx);
        return;
    }

    AVPacket *packet = av_packet_alloc();

    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            AVPacket *new_packet = av_packet_alloc();
            av_packet_ref(new_packet, packet);
            LOGD("📦 Video Packet %p added to queue: time=%.3f pts=%lld dts=%lld duration=%lld size=%d",
                 new_packet,
                 packet->pts * av_q2d(formatCtx->streams[videoStreamIndex]->time_base),
                 new_packet->pts,
                 new_packet->dts,
                 new_packet->duration,
                 new_packet->size
            );
            videoQueue->push(new_packet);
        } else if (packet->stream_index == audioStreamIndex){
            AVPacket *new_packet = av_packet_alloc();
            av_packet_ref(new_packet, packet);
            LOGD("📦 Audio Packet %p added to queue: time=%.3f pts=%lld dts=%lld duration=%lld size=%d",
                 new_packet,
                 packet->pts * av_q2d(formatCtx->streams[audioStreamIndex]->time_base),
                 new_packet->pts,
                 new_packet->dts,
                 new_packet->duration,
                 new_packet->size
            );
            audioQueue->push(new_packet);
        }
        av_packet_unref(packet);
    }

    LOGI("🛑 Reached end of file. Cleaning up.");
    av_packet_free(&packet);
    avformat_close_input(&formatCtx);
    videoQueue->setFinished(true);
    audioQueue->setFinished(true);

    LOGI("✅ Demuxing thread finished");
}
