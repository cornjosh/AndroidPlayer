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

    LOGI("🎧 Input Audio Info: sample_rate=%d, channels=%d, format=%d",
         codecCtx->sample_rate, codecCtx->ch_layout.nb_channels, codecCtx->sample_fmt);

    SwrContext* swrCtx = nullptr;
    AVChannelLayout outChLayout = AV_CHANNEL_LAYOUT_STEREO;

//    LOGD("🎧 swr_alloc_set_opts2 configuration: ");
//    LOGD("  Input channel layout: %llu", codecCtx->ch_layout.u.mask); // 打印输入的通道布局
//    LOGD("  Input sample format: %d", codecCtx->sample_fmt); // 打印输入格式
//    LOGD("  Input sample rate: %d", codecCtx->sample_rate); // 打印输入采样率
//
//    LOGD("  Output channel layout: %llu", outChLayout.u.mask); // 输出布局，假设为立体声
//    LOGD("  Output sample format: %d", AV_SAMPLE_FMT_S16); // 输出格式，假设为 S16
//    LOGD("  Output sample rate: %d", 44100); // 假设输出采样率和输入相同
    int ret = swr_alloc_set_opts2(&swrCtx,
                                  &outChLayout,   // 输出 layout
                                  AV_SAMPLE_FMT_S16,         // 输出格式
                                  44100,                // 输出采样率
                                  &codecCtx->ch_layout,      // 输入 layout
                                  codecCtx->sample_fmt,      // 输入格式
                                  codecCtx->sample_rate,     // 输入采样率
                                  0, nullptr);

    if (ret < 0 || !swrCtx || swr_init(swrCtx) < 0) {
        LOGE("❌ Failed to initialize swrCtx");
        return;
    } else {
        LOGI("✅ swrCtx initialized successfully");
    }

    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = nullptr;

    uint8_t* outBuffer = (uint8_t*) av_malloc(192000);  // 最大缓冲

    while (!(packetQueue->isFinished() && packetQueue->empty())) {
        pkt = packetQueue->pop();
        if (!pkt) continue;

        LOGD("📦 Audio packet pts=%lld size=%d", pkt->pts, pkt->size);

        if (avcodec_send_packet(codecCtx, pkt) < 0) {
            LOGE("❌ Failed to send packet to decoder");
            av_packet_free(&pkt);
            continue;
        }
        av_packet_free(&pkt);

        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            double pts_seconds = frame->pts * av_q2d(codecCtx->time_base);

            LOGD("🧩 Decoded frame: nb_samples=%d, channels=%d, format=%d, time=%.3f",
                 frame->nb_samples, frame->ch_layout.nb_channels, frame->format, pts_seconds);

            int outSamples = swr_convert(swrCtx, &outBuffer, 192000,
                                         (const uint8_t **)frame->data, frame->nb_samples);

            if (outSamples <= 0) {
                LOGE("❌ swr_convert failed or returned 0 samples");
                continue;
            }

            int outSize = outSamples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * codecCtx->ch_layout.nb_channels;
            LOGD("🎵 Converted PCM: samples=%d, outSize=%d, pts=%.3f sec", outSamples, outSize, pts_seconds);

            // 写入环形缓冲区
            ringBuffer->write(outBuffer, outSize);
            LOGD("💾 PCM written to ringBuffer, size=%d", outSize);
        }

    }

    av_free(outBuffer);
    av_frame_free(&frame);
    swr_free(&swrCtx);
    avcodec_free_context(&codecCtx);

    LOGI("🛑 Audio decoder finished");
    ringBuffer->setFinished(true);
}
