//  decoder.cpp

#include "log.h"
#define TAG "decoder"
#include "packetQueue.h"
#include "frameQueue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <thread>
#include "timer.h"

void decodeThread(PacketQueue* packetQueue, FrameQueue* frameQueue, AVCodecParameters* codecpar) {
    LOGI("🔧 Starting decoder thread");

    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        LOGE("❌ Decoder not found");
        return;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx || avcodec_parameters_to_context(codecCtx, codecpar) < 0) {
        LOGE("❌ Failed to create codec context");
        return;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        LOGE("❌ Failed to open codec");
        return;
    }

    LOGI("✅ Decoder initialized");

    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    struct SwsContext* swsCtx = nullptr;

    int width = codecCtx->width;
    int height = codecCtx->height;
    enum AVPixelFormat srcFormat = codecCtx->pix_fmt;

    // 初始化 sws 上下文
    swsCtx = sws_getContext(
            width, height, srcFormat,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx) {
        LOGE("❌ Failed to create SwsContext");
        return;
    }

    while (!(packetQueue->isFinished() && packetQueue->empty() )) {
        pkt = packetQueue->pop();
        if (!pkt) continue;

        LOGD("📦 Packet %p send from queue: time=%.3f pts=%lld dts=%lld duration=%lld size=%d",
             pkt,
             pkt->pts * av_q2d(codecCtx->time_base),
             pkt->pts,
             pkt->dts,
             pkt->duration,
             pkt->size
        );

        int ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_free(&pkt);

        if (ret < 0) {
            LOGE("❌ Error sending packet to decoder");
            continue;
        }

        while (ret >= 0) {
            if (!Timer::isPlaying){
                return;
            }
            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            else if (ret < 0) {
                LOGE("❌ Error during decoding");
                break;
            }

            LOGD("✅ Frame decoded: pts=%lld  size=%dx%d  format=%d",
                 frame->pts, frame->width, frame->height, frame->format);

            // ✅ 创建新的 RGBA 帧（每一帧独立）
            AVFrame* rgbaFrame = av_frame_alloc();
            rgbaFrame->format = AV_PIX_FMT_RGBA;
            rgbaFrame->width = frame->width;
            rgbaFrame->height = frame->height;

            int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, frame->width, frame->height, 1);
            uint8_t* buffer = (uint8_t*)av_malloc(numBytes);
            av_image_fill_arrays(rgbaFrame->data, rgbaFrame->linesize, buffer, AV_PIX_FMT_RGBA, frame->width, frame->height, 1);

            // ✅ 执行转换
            sws_scale(swsCtx,
                      frame->data, frame->linesize,
                      0, frame->height,
                      rgbaFrame->data, rgbaFrame->linesize);

            if (rgbaFrame->linesize[0] <= 0 || rgbaFrame->data[0] == nullptr) {
                LOGE("❌ Invalid RGBA frame! Skipping...");
                av_frame_free(&rgbaFrame);
                av_free(buffer);
                continue;
            }

            rgbaFrame->pts = frame->pts;
            rgbaFrame->pts = frame->pts;
            rgbaFrame->pkt_dts = frame->pkt_dts;
            rgbaFrame->repeat_pict = frame->repeat_pict;

            LOGD("🎨 RGBA frame %p pushed to queue: size=%dx%d  linesize=%d",
                 rgbaFrame, rgbaFrame->width, rgbaFrame->height, rgbaFrame->linesize[0]);

            frameQueue->push(rgbaFrame);  // ✅ 拷贝后的帧，safe push
        }
    }

    LOGI("🛑 Decoder thread finished");

    // 清理资源
    sws_freeContext(swsCtx);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    frameQueue->setFinished(true);
}
