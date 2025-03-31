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
#include <libavutil/channel_layout.h>
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

    SwrContext* swrCtx = nullptr;
    int ret = swr_alloc_set_opts2(&swrCtx,
                                  &codecCtx->ch_layout,                 // 输出 layout
                                  AV_SAMPLE_FMT_S16,          // 输出格式
                                  codecCtx->sample_rate,      // 输出采样率
                                  &codecCtx->ch_layout,       // 输入 layout
                                  codecCtx->sample_fmt,       // 输入格式
                                  codecCtx->sample_rate,      // 输入采样率
                                  0, nullptr);

    if (ret < 0 || !swrCtx || swr_init(swrCtx) < 0) {
        LOGE("❌ Failed to initialize swrCtx");
        return;
    }

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
