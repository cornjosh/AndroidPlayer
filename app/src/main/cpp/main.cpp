#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <jni.h>
#include <android/log.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>


#include "frame_queue.h"

#define LOG_TAG "Main"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

//----------------------------------------------------------------------
// 用于传输 AVPacket* 的队列
//----------------------------------------------------------------------

class VideoPacketQueue {
public:
    void put(AVPacket* packet);
    AVPacket* get();
private:
    std::queue<AVPacket*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

void VideoPacketQueue::put(AVPacket* packet) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(packet);
    cond_.notify_one();
}

AVPacket* VideoPacketQueue::get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]{ return !queue_.empty(); });
    AVPacket* packet = queue_.front();
    queue_.pop();
    return packet;
}

//----------------------------------------------------------------------
// 全局共享变量
//----------------------------------------------------------------------

//const char* input_file = nullptr;

extern const char* input_file;
extern FrameQueue* g_frameQueue;

VideoPacketQueue packet_queue;
int video_stream_index = -1;
AVCodecParameters* codec_par = nullptr; // 存储复制后的解码参数
const AVCodec* codec = nullptr;
AVCodecID codec_id;
std::mutex init_mutex;
std::condition_variable init_cond;
bool init_complete = false;

// 通过 frame_queue.h 定义的队列，用于传递解码后的 RGBA 帧
//FrameQueue* g_frameQueue = nullptr;

//----------------------------------------------------------------------
// 解封装线程：读取数据包并放入 packet_queue
//----------------------------------------------------------------------

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
            AVCodecParameters* orig_par = format_context->streams[i]->codecpar;
            codec_par = avcodec_parameters_alloc();
            if (!codec_par) {
                LOGE("Failed to allocate codec parameters");
                break;
            }
            ret = avcodec_parameters_copy(codec_par, orig_par);
            if (ret < 0) {
                LOGE("Failed to copy codec parameters");
                avcodec_parameters_free(&codec_par);
                break;
            }
            codec = avcodec_find_decoder(codec_par->codec_id);
            LOGV("Codec: %s, Width: %d, Height: %d",
                 avcodec_get_name(codec_par->codec_id), codec_par->width, codec_par->height);
            codec_id = codec_par->codec_id;
            if (!codec) {
                LOGE("Failed to find decoder for codec: %s", avcodec_get_name(codec_par->codec_id));
                avcodec_parameters_free(&codec_par);
                break;
            }
            LOGV("Found decoder: %s", codec->name);
            break;
        }
    }

    if (video_stream_index == -1 || !codec) {
        LOGE("No valid video stream found");
        avcodec_parameters_free(&codec_par);
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
                LOGV("Packet %p added to queue: pts-%lld duration-%lld", new_packet, new_packet->pts, new_packet->duration);
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

//----------------------------------------------------------------------
// 解码线程：从 packet_queue 中取出 AVPacket 解码，转换成 RGBA 后放入 frame_queue
//----------------------------------------------------------------------

void decode() {
    LOGI("Starting decoding process");

    // 等待解封装线程初始化完成
    {
        std::unique_lock<std::mutex> lock(init_mutex);
        init_cond.wait(lock, []{ return init_complete; });
    }

    codec = avcodec_find_decoder(codec_id);
    if (!codec) {
        LOGE("Decoder not found");
        return;
    }

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOGE("Failed to allocate codec context");
        return;
    }
    int ret = avcodec_parameters_to_context(codec_context, codec_par);
    LOGV("Codec: %s, Width: %d, Height: %d",
         avcodec_get_name(codec_id), codec_par->width, codec_par->height);
    if (ret < 0) {
        LOGE("Failed to copy codec parameters");
        avcodec_free_context(&codec_context);
        return;
    }
    ret = avcodec_open2(codec_context, codec, nullptr);
    if (ret < 0) {
        LOGE("Failed to open codec: %s", av_err2str(ret));
        avcodec_free_context(&codec_context);
        return;
    }

    // 初始化 swsContext 用于转换到 RGBA 格式
    SwsContext* sws_ctx = sws_getContext(codec_context->width, codec_context->height, codec_context->pix_fmt,
                                         codec_context->width, codec_context->height, AV_PIX_FMT_RGBA,
                                         SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        LOGE("Failed to initialize sws context");
        avcodec_free_context(&codec_context);
        return;
    }

    // 分配用于接收解码帧的 AVFrame
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOGE("Failed to allocate frame");
        sws_freeContext(sws_ctx);
        avcodec_free_context(&codec_context);
        return;
    }

    // 解码循环
    while (true) {
        AVPacket* packet = packet_queue.get();
        if (!packet) break;  // 退出条件：队列返回空（比如退出时调用）
        ret = avcodec_send_packet(codec_context, packet);
        if (ret < 0) {
            LOGE("Error sending packet: %s", av_err2str(ret));
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (true) {
            ret = avcodec_receive_frame(codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                LOGE("Error receiving frame: %s", av_err2str(ret));
                break;
            }

            // 为转换后的 RGBA 帧分配空间
            AVFrame* rgbaFrame = av_frame_alloc();
            if (!rgbaFrame) {
                LOGE("Failed to allocate RGBA frame");
                break;
            }
            rgbaFrame->format = AV_PIX_FMT_RGBA;
            rgbaFrame->width  = frame->width;
            rgbaFrame->height = frame->height;
            ret = av_frame_get_buffer(rgbaFrame, 32); // 32 字节对齐
            if (ret < 0) {
                LOGE("Failed to allocate buffer for RGBA frame: %s", av_err2str(ret));
                av_frame_free(&rgbaFrame);
                break;
            }

            // 转换到 RGBA 格式
            ret = sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                            rgbaFrame->data, rgbaFrame->linesize);
            if (ret <= 0) {
                LOGE("sws_scale failed: %d", ret);
                av_frame_free(&rgbaFrame);
                break;
            }

            // 将转换后的帧放入全局 FrameQueue（内部会克隆帧）
            g_frameQueue->put(rgbaFrame);
            // 释放本地创建的 rgbaFrame（FrameQueue 内部已克隆）
            av_frame_free(&rgbaFrame);

            av_frame_unref(frame);
        }
    }

    // 刷新解码器：发送空包后继续接收残留帧
    avcodec_send_packet(codec_context, nullptr);
    while (avcodec_receive_frame(codec_context, frame) == 0) {
        AVFrame* rgbaFrame = av_frame_alloc();
        if (!rgbaFrame) break;
        rgbaFrame->format = AV_PIX_FMT_RGBA;
        rgbaFrame->width  = frame->width;
        rgbaFrame->height = frame->height;
        ret = av_frame_get_buffer(rgbaFrame, 32);
        if (ret < 0) {
            LOGE("Failed to allocate buffer for RGBA frame during flush: %s", av_err2str(ret));
            av_frame_free(&rgbaFrame);
            break;
        }
        ret = sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                        rgbaFrame->data, rgbaFrame->linesize);
        if (ret <= 0) {
            LOGE("sws_scale flush failed: %d", ret);
            av_frame_free(&rgbaFrame);
            break;
        }
        g_frameQueue->put(rgbaFrame);
        av_frame_free(&rgbaFrame);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_context);
    avcodec_parameters_free(&codec_par);
    LOGI("Decoding process completed");
}

//----------------------------------------------------------------------
// JNI 接口入口
//----------------------------------------------------------------------

//extern "C"
//JNIEXPORT void JNICALL
//Java_com_example_androidplayer_MainActivity_nativeInit(JNIEnv *env, jclass clazz, jobject context) {
//    // 此处仅作为示例，假定输入文件在 /storage/emulated/0/testfile.mp4
//    std::string filesDir = "/storage/emulated/0";
//    std::string inputFilePath = filesDir + "/testfile.mp4";
//    input_file = inputFilePath.c_str();
//
//    // 初始化全局的 FrameQueue，用于传递 RGBA 帧给后续渲染模块
//    g_frameQueue = new FrameQueue();
//
//    std::thread demux_thread(demux);
//    std::thread decode_thread(decode);
//    demux_thread.join();
//    decode_thread.join();
//}
