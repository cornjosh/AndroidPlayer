// demuxer.cpp
#include "log.h"
#define TAG "demuxer"
#include "packetQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <thread>

void demuxThread(const char* inputPath, PacketQueue* queue, int* videoStreamIndexOut) {
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

    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex == -1) {
        LOGE("❌ No video stream found");
        avformat_close_input(&formatCtx);
        return;
    }

    *videoStreamIndexOut = videoStreamIndex;
    LOGI("🎥 Video stream index: %d", videoStreamIndex);

    AVPacket *packet = av_packet_alloc();

    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            AVPacket *new_packet = av_packet_alloc();
            av_packet_ref(new_packet, packet);
            LOGD("📦 Packet %p added to queue: time=%.3f pts=%lld dts=%lld duration=%lld size=%d",
                 new_packet,
                 packet->pts * av_q2d(formatCtx->streams[videoStreamIndex]->time_base),
                 new_packet->pts,
                 new_packet->dts,
                 new_packet->duration,
                 new_packet->size
            );
            queue->push(new_packet);
        }
        av_packet_unref(packet);
    }

    LOGI("🛑 Reached end of file. Cleaning up.");
    av_packet_free(&packet);
    avformat_close_input(&formatCtx);
    queue->setFinished(true);

    LOGI("✅ Demuxing thread finished");
}
