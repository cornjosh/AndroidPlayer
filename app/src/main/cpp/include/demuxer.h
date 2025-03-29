//
// Created by zylnt on 2025/3/29.
//

#ifndef ANDROIDPLAYER_DEMUXER_H
#define ANDROIDPLAYER_DEMUXER_H


#include "queue.h"

extern "C" {
#include <libavformat/avformat.h>
}

void demux(const char* input_file, VideoPacketQueue& packet_queue);

#endif //ANDROIDPLAYER_DEMUXER_H
