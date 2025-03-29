//
// Created by zylnt on 2025/3/29.
//

#ifndef ANDROIDPLAYER_FFMPEG_H
#define ANDROIDPLAYER_FFMPEG_H


#include <libavutil/error.h>
#include "log.h"

void print_ffmpeg_error(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    LOGE("FFmpeg error: %s", errbuf);
}


#endif //ANDROIDPLAYER_FFMPEG_H
