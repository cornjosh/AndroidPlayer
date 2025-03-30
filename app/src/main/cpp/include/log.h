//
// Created by zylnt on 2025/3/29.
//

#ifndef ANDROIDPLAYER_LOG_H
#define ANDROIDPLAYER_LOG_H

#include <android/log.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)


#endif //ANDROIDPLAYER_LOG_H

