//
// Created by zylnt on 2025/3/30.
//

#ifndef ANDROIDPLAYER_RENDERCONTEXT_H
#define ANDROIDPLAYER_RENDERCONTEXT_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>

struct RenderContext {
    ANativeWindow* window = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;

    GLuint program = 0;
    GLuint texture = 0;
    GLuint vertexBuffer = 0;
    GLuint texCoordBuffer = 0;
    GLint positionLoc = -1;
    GLint texCoordLoc = -1;
    GLint samplerLoc = -1;

    int width = 0;
    int height = 0;

    bool initialized = false;
};

#endif //ANDROIDPLAYER_RENDERCONTEXT_H
