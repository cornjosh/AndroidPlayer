//
// Created by zylnt on 2025/3/29.
//

#ifndef ANDROIDPLAYER_LOG_H

#include <android/log.h>

#define ANDROIDPLAYER_LOG_H

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


#endif //ANDROIDPLAYER_LOG_H
