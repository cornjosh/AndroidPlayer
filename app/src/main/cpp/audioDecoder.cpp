//
// audioDecoder.cpp
// Created by zylnt on 2025/3/31.
//

#include "log.h"
#define TAG "audioDecoder"
#include "packetQueue.h"
#include "audioRingBuffer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

void audioDecodeThread(PacketQueue* packetQueue, AudioRingBuffer* ringBuffer, AVCodecParameters* codecpar) {
    LOGI("🔊 Starting audio decoder thread");

    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        LOGE("❌ Audio decoder not found");
        return;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    avcodec_open2(codecCtx, codec, nullptr);

    SwrContext* swrCtx = swr_alloc_set_opts(
            nullptr,
            AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, codecpar->sample_rate,
            codecpar->channel_layout, codecpar->format, codecpar->sample_rate,
            0, nullptr);
    swr_init(swrCtx);

    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = nullptr;

    uint8_t* outBuffer = (uint8_t*) av_malloc(192000);  // 最大缓冲

    while (!(packetQueue->isFinished() && packetQueue->empty())) {
        pkt = packetQueue->pop();
        if (!pkt) continue;

        if (avcodec_send_packet(codecCtx, pkt) < 0) {
            av_packet_free(&pkt);
            continue;
        }
        av_packet_free(&pkt);

        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            int outSamples = swr_convert(swrCtx, &outBuffer, 192000,
                                         (const uint8_t **)frame->data, frame->nb_samples);
            int outSize = outSamples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 2;  // stereo

            ringBuffer->write(outBuffer, outSize);
        }
    }

    av_free(outBuffer);
    av_frame_free(&frame);
    swr_free(&swrCtx);
    avcodec_free_context(&codecCtx);

    LOGI("🛑 Audio decoder finished");
    ringBuffer->setFinished(true);
}
