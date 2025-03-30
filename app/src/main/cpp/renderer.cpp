// renderer.cpp
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include "frame_queue.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

// 与 player.cpp 中定义相同的共享上下文结构体
struct RenderContext {
    ANativeWindow* window;
    FrameQueue* frameQueue;
};

// 顶点着色器，用于传递顶点坐标和纹理坐标
static const char* vertexShaderSource =
        "attribute vec4 aPosition;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";

// 片段着色器，采样纹理并输出颜色
static const char* fragmentShaderSource =
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform sampler2D sTexture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(sTexture, vTexCoord);\n"
        "}\n";

// 编译 shader 的工具函数
GLuint loadShader(GLenum type, const char* shaderSrc) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    // 此处可添加编译错误检查
    return shader;
}

extern "C" void* renderThread(void* arg) {
    RenderContext* ctx = (RenderContext*) arg;

    // 初始化 EGL 环境
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);

    EGLSurface eglSurface = eglCreateWindowSurface(display, config, ctx->window, nullptr);
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    eglMakeCurrent(display, eglSurface, eglSurface, eglContext);

    // 编译 shader 并创建 shader program
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint positionLoc = glGetAttribLocation(program, "aPosition");
    GLint texCoordLoc = glGetAttribLocation(program, "aTexCoord");
    GLint samplerLoc  = glGetUniformLocation(program, "sTexture");

    // 创建纹理对象
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 定义一个全屏四边形的顶点数据
    // 每个顶点：3个坐标分量 + 2个纹理坐标
    GLfloat vertices[] = {
            //  X,     Y,    Z,      U,   V
            -1.0f,  1.0f, 0.0f,    0.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,    0.0f, 1.0f,
            1.0f,  1.0f, 0.0f,    1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,    1.0f, 1.0f,
    };

    // 用于判断纹理是否已初始化
    bool textureInitialized = false;
    int frameWidth = 640, frameHeight = 480;

    // 渲染循环
    while (true) {
        // 从队列中获取一帧，若队列已退出且空，则返回 nullptr
        AVFrame* frame = ctx->frameQueue->get();
        if (!frame) {
            // 当队列退出且空时，退出渲染循环
            break;
        }

        // 假设 AVFrame 已转换为 RGBA 格式
        frameWidth = frame->width;
        frameHeight = frame->height;

        glBindTexture(GL_TEXTURE_2D, textureID);
        if (!textureInitialized) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
            textureInitialized = true;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
        }

        // 清屏并绘制四边形
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);

        glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
        glEnableVertexAttribArray(positionLoc);

        glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices + 3);
        glEnableVertexAttribArray(texCoordLoc);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(samplerLoc, 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        eglSwapBuffers(display, eglSurface);

        // 渲染完毕后释放该 AVFrame
        av_frame_free(&frame);
    }

    // 清理 OpenGL 和 EGL 相关资源
    glDeleteTextures(1, &textureID);
    glDeleteProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, eglSurface);
    eglDestroyContext(display, eglContext);
    eglTerminate(display);

    return nullptr;
}
