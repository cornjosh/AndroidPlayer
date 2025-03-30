#include <iostream>
#include <thread>
#include <queue>
#include <fstream>
#include <jni.h>
#include <android/log.h>

// 引入FFmpeg C库
extern "C" {
#include <libavformat/avformat.h>  // 用于媒体格式处理
#include <libavcodec/avcodec.h>  // 用于编解码处理
#include <libavutil/imgutils.h>  // 用于图像工具
}

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define LOG_TAG "Main"

// 队列类定义
class VideoPacketQueue {
public:
    void put(AVPacket* packet);
    AVPacket* get();
private:
    std::queue<AVPacket*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

// 全局共享变量
const char* input_file = nullptr;
const char* output_file = nullptr;
VideoPacketQueue packet_queue;
int video_stream_index = -1;
AVCodecParameters* codec_par = nullptr; // 用于存储复制后的参数
const AVCodec* codec = nullptr;
std::mutex init_mutex;
std::condition_variable init_cond;
bool init_complete = false;
AVCodecID codec_id;

// 解封装函数
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
            AVCodecParameters* original_codec_par = format_context->streams[i]->codecpar;
            codec_par = avcodec_parameters_alloc();
            if (!codec_par) {
                LOGE("Failed to allocate codec parameters");
                break;
            }
            ret = avcodec_parameters_copy(codec_par, original_codec_par);
            if (ret < 0) {
                LOGE("Failed to copy codec parameters");
                avcodec_parameters_free(&codec_par);
                break;
            }
            codec = avcodec_find_decoder(codec_par->codec_id);
            LOGV("Codec parameters Codec: %s, Width: %d, Height: %d",
                 avcodec_get_name(codec_par->codec_id), codec_par->width, codec_par->height);
            codec_id = codec_par->codec_id;
            if (!codec) {
                LOGE("Failed to find decoder for codec: %s",
                     avcodec_get_name(codec_par->codec_id));
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

// 解码函数
void decode() {
    LOGI("Starting decoding process");

    // 等待解封装线程初始化完成
    std::unique_lock<std::mutex> lock(init_mutex);
    init_cond.wait(lock, []{ return init_complete; });

    codec = avcodec_find_decoder(codec_id);

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOGE("Failed to allocate codec context");
        return;
    }

    int ret = avcodec_parameters_to_context(codec_context, codec_par);
    LOGV("Codec parameters Codec: %s, Width: %d, Height: %d",
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

    std::ofstream output(output_file, std::ios::binary);
    if (!output.is_open()) {
        LOGE("Failed to open output file");
        avcodec_free_context(&codec_context);
        return;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOGE("Failed to allocate frame");
        output.close();
        avcodec_free_context(&codec_context);
        return;
    }

    // 解码循环
    while (true) {
        AVPacket* packet = packet_queue.get();
        if (!packet) break;
        LOGV("Packet %p get form queue: pts-%lld duration-%lld", packet, packet->pts, packet->duration);
        ret = avcodec_send_packet(codec_context, packet);
        if (ret < 0) {
            LOGE("Error sending packet: %s", av_err2str(ret));
            av_packet_unref(packet);
            continue;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_context, frame);
            if (ret == AVERROR(EAGAIN)) break;
            if (ret < 0) {
                LOGE("Error receiving frame: %s", av_err2str(ret));
                break;
            }

            // 写入YUV420P数据
            if (frame->format == AV_PIX_FMT_YUV420P) {
                for (int i = 0; i < frame->height; i++) {
                    output.write((char*)frame->data[0] + i * frame->linesize[0],
                                 frame->width);
                }
                for (int i = 0; i < frame->height / 2; i++) {
                    output.write((char*)frame->data[1] + i * frame->linesize[1],
                                 frame->width / 2);
                }
                for (int i = 0; i < frame->height / 2; i++) {
                    output.write((char*)frame->data[2] + i * frame->linesize[2],
                                 frame->width / 2);
                }
                LOGV("Frame written to file");
            }
            av_frame_unref(frame);
        }

        av_packet_unref(packet);
    }

    // 刷新解码器
    avcodec_send_packet(codec_context, nullptr);
    while (avcodec_receive_frame(codec_context, frame) == 0) {
        if (frame->format == AV_PIX_FMT_YUV420P) {
            // 写入残留帧
        }
        av_frame_unref(frame);
    }

    output.close();
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    avcodec_parameters_free(&codec_par); // 释放复制的参数
    LOGI("Decoding process completed");
}

// 队列实现
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


extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_MainActivity_nativeInit(JNIEnv *env, jclass clazz, jobject context) {

    std::string filesDir = "/storage/emulated/0";
    std::string inputFilePath = filesDir + "/testfile.mp4";
    std::string outputFilePath = filesDir + "/output.yuv";
    input_file = inputFilePath.c_str();
    output_file = outputFilePath.c_str();

    std::thread demux_thread(demux);
    std::thread decode_thread(decode);
    demux_thread.join();
    decode_thread.join();
}