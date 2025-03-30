// main.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cinttypes>
#include <jni.h>

// 引入FFmpeg C库
extern "C" {
#include <libavformat/avformat.h>  // 用于媒体格式处理
#include <libavcodec/avcodec.h>  // 用于编解码处理
#include <libavutil/imgutils.h>  // 用于图像工具
}

// 日志宏定义
#define LOGI(fmt, ...) std::printf("[INFO] " fmt "\n", ##__VA_ARGS__)   // 普通信息日志
#define LOGV(fmt, ...) std::printf("[VERBOSE] " fmt "\n", ##__VA_ARGS__) // 详细信息日志
#define LOGE(fmt, ...) std::printf("[ERROR] " fmt "\n", ##__VA_ARGS__)   // 错误日志

// 视频解码主函数
void decodede() {

    // 设置输入输出文件路径
    std::string filesDir = "/storage/emulated/0";          // Android存储路径
    std::string inputFilePath = filesDir + "/testfile.mp4";// 输入MP4文件路径
    std::string outputFilePath = filesDir + "/output.yuv";// 输出YUV文件路径

    const char* input_file = inputFilePath.c_str();        // C风格字符串
    const char* output_file = outputFilePath.c_str();      // C风格字符串

    LOGI("Starting video processing...");
    LOGI("Input file path: %s", input_file);
    LOGI("Output file path: %s", output_file);

    // 初始化FFmpeg格式上下文
    AVFormatContext* format_context = nullptr;             // 格式上下文指针
    int ret = avformat_open_input(&format_context, input_file, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Failed to open input file: %s", input_file);
        return;
    }
    LOGV("Successfully opened input file: %s", input_file);

    // 获取流信息
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        LOGE("Failed to find stream information");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Stream information found");

    // 查找视频流索引
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        // 检查流类型是否为视频
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        LOGE("Could not find video stream in the input file");
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Video stream found at index: %d", video_stream_index);

    // 获取视频流参数
    AVStream* video_stream = format_context->streams[video_stream_index];
    LOGV("Video stream parameters: Codec: %s, Width: %d, Height: %d",
         avcodec_get_name(video_stream->codecpar->codec_id),  // 获取编解码器名称
         video_stream->codecpar->width,                       // 视频宽度
         video_stream->codecpar->height);                      // 视频高度

    // 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        LOGE("Failed to find decoder for codec id: %s", avcodec_get_name(video_stream->codecpar->codec_id));
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Found decoder: %s", codec->name);

    // 创建编解码上下文
    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOGE("Failed to allocate the codec context");
        avformat_close_input(&format_context);
        return;
    }

    // 复制编解码参数
    ret = avcodec_parameters_to_context(codec_context, video_stream->codecpar);
    if (ret < 0) {
        LOGE("Failed to copy codec parameters to decoder context");
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 打开编解码器
    ret = avcodec_open2(codec_context, codec, nullptr);
    if (ret < 0) {
        LOGE("Failed to open codec");
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Codec successfully opened");

    // 打开输出文件（二进制模式）
    std::ofstream output_stream(output_file, std::ios::binary);
    if (!output_stream.is_open()) {
        LOGE("Failed to open output file: %s", output_file);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }
    LOGV("Output file opened: %s", output_file);

    // 分配帧和数据包内存
    AVFrame* frame = av_frame_alloc();           // 存储解码后的视频帧
    if (!frame) {
        LOGE("Failed to allocate AVFrame");
        output_stream.close();
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    AVPacket* packet = av_packet_alloc();         // 存储输入的压缩数据包
    if (!packet) {
        LOGE("Failed to allocate AVPacket");
        av_frame_free(&frame);
        output_stream.close();
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 主循环：读取并解码数据包
    while ((ret = av_read_frame(format_context, packet)) >= 0) {
        if (packet->stream_index == video_stream_index) {  // 只处理视频流
            ret = avcodec_send_packet(codec_context, packet);
            if (ret < 0) {
                LOGE("Error sending packet for decoding");
                av_packet_unref(packet);  // 释放数据包引用
                continue;
            }

            // 循环接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_context, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;  // 需要更多数据或解码完成
                } else if (ret < 0) {
                    LOGE("Error while decoding");
                    break;
                }

                // 只处理YUV420P格式的帧
                if (frame->format == AV_PIX_FMT_YUV420P) {
                    // 写入Y分量（亮度）
                    for (int i = 0; i < frame->height; i++) {
                        output_stream.write(
                                (char*)frame->data[0] + i * frame->linesize[0],  // 行起始地址
                                frame->width  // 每行数据长度
                        );
                    }

                    // 写入U分量（色度）
                    for (int i = 0; i < frame->height / 2; i++) {
                        output_stream.write(
                                (char*)frame->data[1] + i * frame->linesize[1],
                                frame->width / 2
                        );
                    }

                    // 写入V分量（色度）
                    for (int i = 0; i < frame->height / 2; i++) {
                        output_stream.write(
                                (char*)frame->data[2] + i * frame->linesize[2],
                                frame->width / 2
                        );
                    }

                    LOGV("Decoded frame written to file");
                } else {
                    LOGE("Frame format is not YUV420P, skipping");
                }

                av_frame_unref(frame);  // 释放帧引用
            }
        }

        av_packet_unref(packet);  // 释放数据包引用
    }

    // 刷新解码器，处理残留帧
    avcodec_send_packet(codec_context, nullptr);  // 发送空数据包触发刷新
    while (avcodec_receive_frame(codec_context, frame) == 0) {
        if (frame->format == AV_PIX_FMT_YUV420P) {
            // 写入YUV数据（同上）
            for (int i = 0; i < frame->height; i++) {
                output_stream.write((char*)frame->data[0] + i * frame->linesize[0], frame->width);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                output_stream.write((char*)frame->data[1] + i * frame->linesize[1], frame->width / 2);
            }
            for (int i = 0; i < frame->height / 2; i++) {
                output_stream.write((char*)frame->data[2] + i * frame->linesize[2], frame->width / 2);
            }
            LOGV("Flushed decoded frame written to file");
        }
        av_frame_unref(frame);
    }

    // 释放资源
    av_packet_free(&packet);      // 释放数据包内存
    av_frame_free(&frame);        // 释放帧内存
    avcodec_free_context(&codec_context); // 释放编解码上下文（自动关闭编解码器）
    avformat_close_input(&format_context); // 关闭输入文件
    output_stream.close();        // 关闭输出文件

    LOGI("Decoding process completed");
}