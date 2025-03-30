#include <jni.h>

//
// Created by zylnt on 2025/3/30.
//

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv *env, jobject thiz, jstring file,
                                                 jobject surface) {
    // TODO: implement nativePlay()
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv *env, jobject thiz, jboolean p) {
    // TODO: implement nativePause()
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv *env, jobject thiz, jdouble position) {
    // TODO: implement nativeSeek()
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeStop()
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv *env, jobject thiz, jfloat speed) {
    // TODO: implement nativeSetSpeed()
}


extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetPosition()
}



extern "C"
JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeGetDuration()
}