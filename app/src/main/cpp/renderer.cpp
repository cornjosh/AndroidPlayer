// renderer.cpp
#include "RenderContext.h"
#include "log.h"
#define TAG "renderer"

#include "frameQueue.h"

#include <mutex>

extern "C" {
#include <libavutil/frame.h>
#include <android/native_window_jni.h>
}

std::mutex renderInitMutex;

static const char* vertexShaderCode = R"(
attribute vec4 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    gl_Position = aPosition;
    vTexCoord = aTexCoord;
}
)";

static const char* fragmentShaderCode = R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint createProgram(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    return program;
}

void initRenderContext(RenderContext* ctx, ANativeWindow* window, int width, int height) {
    std::lock_guard<std::mutex> lock(renderInitMutex);

    if (ctx->initialized && ctx->window == window) {
        LOGI("🛡️ Already initialized with same surface, skipping");
        return;
    }

    ctx->window = window;
    ctx->width = width;
    ctx->height = height;

    if (ctx->width <= 0 || ctx->height <= 0) {
        LOGE("❌ EGL window size invalid: %dx%d", ctx->width, ctx->height);
        return;
    }

    LOGI("✅ EGL initialized with size: %dx%d", ctx->width, ctx->height);

    ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(ctx->display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
    };
    eglChooseConfig(ctx->display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(ctx->display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    ctx->surface = eglCreateWindowSurface(ctx->display, config, window, nullptr);

    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, ctxAttribs);
    eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

    ctx->program = createProgram(vertexShaderCode, fragmentShaderCode);
    ctx->positionLoc = glGetAttribLocation(ctx->program, "aPosition");
    ctx->texCoordLoc = glGetAttribLocation(ctx->program, "aTexCoord");
    ctx->samplerLoc = glGetUniformLocation(ctx->program, "uTexture");

    GLfloat vertices[] = {
            -1.0f, -1.0f,  // bottom left
            1.0f, -1.0f,  // bottom right
            -1.0f,  1.0f,  // top left
            1.0f,  1.0f   // top right
    };

    GLfloat texCoords[] = {
            0.0f, 1.0f,  // bottom left
            1.0f, 1.0f,  // bottom right
            0.0f, 0.0f,  // top left
            1.0f, 0.0f   // top right
    };

    glGenBuffers(1, &ctx->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ctx->texCoordBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->texCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);

    glGenTextures(1, &ctx->texture);
    glBindTexture(GL_TEXTURE_2D, ctx->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ctx->initialized = true;
}

void renderFrameToSurface(AVFrame* frame, ANativeWindow* window) {
    static RenderContext ctx;
    if (!ctx.initialized || ctx.window != window) {
        LOGI("⚠️ EGL context not initialized or surface changed, reinitializing...");
        initRenderContext(&ctx, window, frame->width, frame->height);
    }

    LOGD("🖼️ Frame size: %dx%d  linesize=%d", frame->width, frame->height, frame->linesize[0]);

    glViewport(0, 0, ctx.width, ctx.height);
    glUseProgram(ctx.program);

    // 顶点坐标
    glBindBuffer(GL_ARRAY_BUFFER, ctx.vertexBuffer);
    glEnableVertexAttribArray(ctx.positionLoc);
    glVertexAttribPointer(ctx.positionLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // 纹理坐标
    glBindBuffer(GL_ARRAY_BUFFER, ctx.texCoordBuffer);
    glEnableVertexAttribArray(ctx.texCoordLoc);
    glVertexAttribPointer(ctx.texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // 上传纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGE("❌ glTexImage2D error: 0x%x", err);
    }

    glUniform1i(ctx.samplerLoc, 0);

    // 清屏并绘制
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLenum err2 = glGetError();
    if (err2 != GL_NO_ERROR) {
        LOGE("❌ glDrawArrays error: 0x%x", err2);
    }

    eglSwapBuffers(ctx.display, ctx.surface);
    EGLint swapErr = eglGetError();
    if (swapErr != EGL_SUCCESS) {
        LOGE("❌ eglSwapBuffers failed: 0x%x", swapErr);
    }
}

void renderThread(FrameQueue* frameQueue, ANativeWindow* window, AVRational time_base) {
    while (!frameQueue->isFinished()) {
        AVFrame* frame = frameQueue->pop();
        if (!frame) continue;

        // 打印调试时间戳
        double time_sec = frame->pts * av_q2d(time_base);
        LOGD("🖼️ Rendering frame %p: time=%.3f pts=%lld", frame, time_sec, frame->pts);

        // ✅ 调用此函数进行渲染
        renderFrameToSurface(frame, window);

        av_frame_free(&frame);
    }
}