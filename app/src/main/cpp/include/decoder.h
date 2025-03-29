//
// Created by zylnt on 2025/3/29.
//

#ifndef ANDROIDPLAYER_DECODER_H
#define ANDROIDPLAYER_DECODER_H

#include "queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

void decode(VideoPacketQueue& packet_queue, const char* output_file);

#endif //ANDROIDPLAYER_DECODER_H
