// decoder.cpp
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
    AVFrame* rgbaFrame = av_frame_alloc();

    int width = codecCtx->width;
    int height = codecCtx->height;

    // 设置RGBA帧缓冲区
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(rgbaFrame->data, rgbaFrame->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);

    struct SwsContext* swsCtx = sws_getContext(
            width, height, codecCtx->pix_fmt,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    while (!packetQueue->isFinished() || (pkt = packetQueue->pop()) != nullptr) {
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
            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                LOGE("❌ Error during decoding");
                break;
            }

            LOGD("✅ Frame decoded: pts=%lld width=%d height=%d", frame->pts, frame->width, frame->height);

            // 转换为RGBA
            sws_scale(swsCtx, frame->data, frame->linesize, 0, height,
                      rgbaFrame->data, rgbaFrame->linesize);

            AVFrame* finalFrame = av_frame_alloc();
            av_frame_ref(finalFrame, rgbaFrame);
            finalFrame->pts = frame->pts;

            LOGD("🎨 Frame %p added to frameQueue", finalFrame);
            frameQueue->push(finalFrame);
        }
    }

    LOGI("🛑 Decoder thread finished");

    // 资源释放
    sws_freeContext(swsCtx);
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&rgbaFrame);
    avcodec_free_context(&codecCtx);
    frameQueue->setFinished(true);
}
