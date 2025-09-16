# AndroidPlayer - 音视频播放器学习项目

## 项目简介

这是一个从零开始构建的 Android 音视频播放器项目，完整记录了我在多媒体开发领域的学习成长历程。项目从基础的 FFmpeg 交叉编译开始，逐步实现了完整的音视频同步播放功能，包含了大量真实的技术挑战和解决方案。

通过这个项目，我深入学习了 FFmpeg、音视频同步、多线程编程和 Android Native 开发等技术栈，更重要的是在解决实际问题的过程中不断提升了编程能力和系统性思维。

## 学习成果展示

![应用截图](img/屏幕截图%202025-04-01%20140858.png)

**最新功能演示：** [录屏演示](录屏4.mp4)

## 开发历程回顾

### 🎯 **Day12: 基础架构搭建 - 从零开始的探索**

这一阶段是整个项目的起点，也是我第一次真正接触音视频开发的复杂性。


**学习任务和挑战：**
- 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`
- 创建线程安全的包队列 (`packetQueue`)
- 实现 demux 线程进行视频解封装
- 实现 decode 线程进行视频解码
- 解码后的 video frame 以 YUV420P 格式存储到文件

**实际实现过程：**

#### 初始化项目
- 使用示例代码创建 Android Studio 项目
- 使用 git 管理代码，设置 remote
- 遇到第一个问题：编译错误，缺少 `log.h` 文件
- **解决：** 创建 `log.h` 文件，添加 `#include <android/log.h>` 头文件

#### FFmpeg 交叉编译挑战
修改 `ffmpeg.sh` 脚本，这是我第一次深度接触交叉编译：

```bash
echo "开始编译ffmpeg so"
# NDK路径
NDK=/home/ubuntu2204/Android/Sdk/ndk/21.3.6528147
PLATFORM=$NDK/platforms/android-21/arch-arm64
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
PREFIX=/home/ubuntu2204/code/day9/build/ffmpeg/android_outputS/usr/local

$TOOLCHAIN/bin/aarch64-linux-android-ld \
-rpath-link=$PLATFORM/usr/lib \
-L$PLATFORM/usr/lib \
-L$PREFIX/lib \
-soname libffmpeg.so -shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o \
$PREFIX/libffmpeg.so \
    $PREFIX/lib/libavcodec.a \
    $PREFIX/lib/libavfilter.a \
    $PREFIX/lib/libswresample.a \
    $PREFIX/lib/libavformat.a \
    $PREFIX/lib/libavutil.a \
    $PREFIX/lib/libswscale.a \
    -lc -lm -lz -ldl -llog --dynamic-linker=/system/bin/linker \
    $TOOLCHAIN/lib/gcc/aarch64-linux-android/4.9.x/libgcc_real.a
```

**学习收获：**
- 理解了静态库和动态库的区别及链接过程
- 掌握了 NDK 工具链的使用方法
- 学会了处理 FFmpeg 头文件的正确方式

#### 线程安全队列实现
这是我第一次设计真正的多线程数据结构：

```cpp
void VideoPacketQueue::put(AVPacket* packet) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(packet);
    cond_.notify_one();
}

AVPacket* VideoPacketQueue::get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return!queue_.empty(); });
    AVPacket* packet = queue_.front();
    queue_.pop();
    return packet;
}
```

**作业完成录屏：** [录屏](录屏.mp4)

![Day12成果](img/屏幕截图%202025-03-29%20084654.png)

---

### 🚀 **Day13: 视频渲染实现 - 进入图形编程世界**

**学习任务扩展：**
- 创建 `frameQueue` 用于存放 video frame
- 实现 `renderer` 线程，使用 OpenGL 渲染到 SurfaceView
- 创建 `player.cpp` 负责 Java 层交互，控制三个线程协作

这一阶段让我接触到了 OpenGL ES 和 Android 图形系统的复杂性。

**作业完成录屏：** [录屏2](录屏2.mp4)

---

### 🎵 **Day14: 音视频同步 - 攻克核心难题**

这是整个项目中最具挑战性的阶段，也是我学习成长最显著的部分。

**核心任务：**
- 创建 `audioPacketQueue` 和 `audioDecoder`
- 实现 `audioRingBuffer` 环形缓冲区
- 创建 `AAudioPlayer` 使用 AAudio 播放音频
- 实现音视频同步机制
- 附加功能：精确 seek 和倍速播放

**最终实现功能：**

- [x] 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`
- [x] 创建 `log.h` 文件，实现 `LOGE`、`LOGI` 等日志打印宏
- [x] 两个 `packetQueue`：`videoPacketQueue` 和 `audioPacketQueue`，分别存放 video/audio packet
- [x] 一个 `frameQueue`，用于存放 video frame
- [x] 一个 `audioRingBuffer`，用于存放 audio pcm
- [x] **demux 线程**：对原视频进行解封装，得到 video/audio packet 并放入对应队列
- [x] **decode 线程**：从 videoPacketQueue 获取 video packet 解码成 video frame，放入 frameQueue
- [x] **audioDecode 线程**：从 audioPacketQueue 获取 audio packet 解码成 audio pcm，放入 audioRingBuffer
- [x] **renderer 线程**：从 frameQueue 获取 video frame，使用 OpenGL 渲染到 SurfaceView
- [x] **AAudioPlayer 线程**：从 audioRingBuffer 获取 audio pcm，使用 AAudio 播放音频
- [x] **Timer 线程**：负责控制总体的播放进度和音视频同步

### 附加功能实现
- [x] 使用 Timer 实现进度条的显示功能
- [x] 使用 Timer 实现非精确 seek 功能
- [x] 使用 Timer 实现倍速播放功能（暂时只能视频倍速）
- [x] 使用 Timer 实现暂停和继续播放功能
- [x] 使用 isInit 实现播放器的初始化和释放功能（优雅的退出）

**最终演示：** [录屏4](录屏4.mp4)

**音视频同步成功：** [录屏3](录屏3.mp4)

---

## 🔧 核心技术挑战与成长历程

这个项目中遇到的真实技术难题，记录了我解决问题的完整思路和成长过程：

### 1. AAudio 播放器无法播放音频 - 初次接触音频API的困惑

**问题描述：** 最初遇到的一个主要问题是音频无法播放。通过逐步排查和调试，发现：
- `AAudioStream` 未正确设置音频流格式，或者流未正确打开
- 尝试创建 `AAudioStreamBuilder` 并设置音频流参数时，某些地方处理不当，例如未正确调用 `AAudioStreamBuilder_openStream` 或者音频格式设置不匹配

**我的解决过程：**
- 使用 `AAudio` API 创建流时，确保格式、通道数、采样率等参数正确设置，并调用 `AAudioStream_requestStart` 启动流
- 增加适当的日志输出，以便在调试过程中及时发现问题

**学习收获：** 第一次深入了解 Android 音频架构，理解了底层音频流的生命周期管理。

### 2. 音频无法正确播放和流的关闭

**遇到的问题：** 播放音频时，发现尽管缓冲区内的数据已被读取，但音频播放仍存在问题，音频流似乎未正常工作

**解决策略：**
- 通过调试日志查看音频流的状态，并验证 `AAudioStream_write` 是否成功写入数据
- 确保使用完音频流后，调用 `AAudioStream_requestStop` 和 `AAudioStream_close` 正确关闭流

### 3. 获取和同步音频的 PTS - 时间戳计算的学习

**技术挑战：** 实现音视频同步时，关键问题是如何从音频流中获取正确的播放时间戳（PTS）。尝试使用 `AAudioStream_getFramesWritten` 和 `AAudioStream_getSampleRate` 计算音频的 PTS

**解决方案：**
- 使用 `AAudioStream_getFramesWritten` 获取已写入的帧数，并通过音频流的采样率计算出时间戳（PTS）
- 确保音频的时间戳更新与实际播放同步，避免播放过程中出现音频时滞或同步问题

### 4. 音视频同步问题 - 多媒体编程的核心难题

**复杂性认知：** 实现音视频同步时，希望通过获取音频的 PTS 来同步音频和视频播放。然而，音视频线程的同步是复杂过程，尤其要确保时间戳的一致性并避免两者不同步

**创新解决：**
- 设计一个 **中央时间更新时间线程**，该线程定期更新时间戳，并通过条件变量通知音视频线程
- 在视频和音频线程中，通过比较当前时间戳与各自的数据 `PTS`，确保视频和音频播放同步

### 5. 线程安全的时间戳变量 - 并发编程实践

**技术学习：** 为确保多个线程安全访问时间戳变量（`currentTime`），使用 `std::atomic` 存储全局的时间戳，并利用 `std::mutex` 和 `std::condition_variable` 控制时间的同步

**实现方案：**
- 通过 `std::atomic<double>` 存储时间戳，保证线程间的安全访问
- 使用 `std::condition_variable` 同步不同线程之间的操作，确保音频和视频线程在读取时间戳时不会发生竞争

### 6. 音频和视频线程的时间戳同步

**架构设计：** 音频和视频线程都需依据中央时间戳进行同步。为保证音频和视频同步播放，设计一个专门更新时间的线程来维护音视频的统一时间进度

**技术实现：**
- 每隔一定时间（每 5 毫秒），时间线程更新时间戳，音频和视频线程通过读取该时间戳决定播放进度
- 使用 `std::this_thread::sleep_for` 模拟音视频的帧率，并确保两个线程能根据更新的时间戳决定是否继续播放或等待

### 7. 编译成功但播放音视频依旧不同步

**持续问题：** 成功编译并能播放音频和视频后，仍面临音视频不同步的问题，主要是音频的 PTS 和视频的播放进度不一致，导致两者不同步

**系统性解决：**
- 引入音视频同步机制，确保音频和视频根据各自的 PTS 计算出合适的播放时间
- 通过调整线程等待时间，确保音频和视频线程可根据时间戳的变化进行播放

### 8. 解码线程（decodeThread）退出后播放中断 - 线程生命周期管理

**关键问题：** 实现视频播放时，遇到一个关键问题：**解码线程（`decodeThread`）执行完后终止，导致视频播放中断**。原本的代码在解码线程完成后便退出，使播放器未继续处理后续的视频帧

**深度调试：**
- 调试发现 `decodeThread` 线程的退出条件设置不合理，导致其处理完所有的包后直接退出
- 为让解码线程继续工作，修改退出条件，确保解码线程只有在队列完全消费完且无更多数据时才退出
- 增加一个机制，当 `packetQueue` 中无更多数据时，解码线程稍作等待，直到有新数据进来，避免线程提前退出

### 9. 解码帧的内存泄漏问题 - 内存管理的重要性

**严重问题：** 播放过程中，设备死机很大程度上是由内存泄漏引起的，特别是在解码阶段，未及时释放已解码的 `AVFrame`。这导致内存不断积累，最终设备内存溢出，系统被强制杀掉

**系统性解决：**
- 加强内存管理，每次解码并将帧推入队列后，确保及时释放不再使用的帧
- 特别是使用完 `AVFrame` 后，通过 `av_frame_free()` 确保内存得到正确释放
- 为避免每次都创建和销毁大量 `AVFrame` 对象，实现帧的复用机制，避免频繁的内存分配和释放

### 10. Surface 未正确显示导致黑屏 - UI布局问题

**渲染问题：** 将渲染帧通过 OpenGL 渲染到 `SurfaceView` 时，发现渲染输出黑屏。经过调试，发现是因为 `SurfaceView` 未正确设置约束，导致渲染的 Surface 尺寸为 0x0

**UI调试：**
- 确保 `SurfaceView` 使用 `ConstraintLayout` 时，添加正确的约束（`layout_constraintTop_toTopOf`、`layout_constraintStart_toStartOf` 等），使 `SurfaceView` 能够正确渲染到屏幕上
- 同时，在 `SurfaceView` 中设置调试背景色，确保视图显示正常，并进一步验证 `Surface` 在渲染时的宽高正确

### 11. EGL 初始化重复问题 - OpenGL上下文管理

**图形渲染问题：** 实现渲染后，发现在播放过程中，`EGL` 被重复初始化，导致渲染输出异常或黑屏。重复初始化 EGL 会破坏现有的上下文，导致图形渲染失败

**技术方案：**
- 使用 `std::mutex` 加锁，确保每次只初始化一次 `EGL` 上下文
- 每次渲染前，检查 `EGL` 是否已经初始化，仅在必要时重新初始化
- 此外，增加对 `Surface` 和 `EGLContext` 的管理，确保在 `Surface` 或视图尺寸变化时，能够安全地重建 `EGL` 上下文

### 12. 重复解码导致的性能问题 - 性能优化实践

**性能瓶颈：** 随着播放时间的增加，还遇到了解码性能逐渐下降的问题，尤其是帧队列中积压了大量待处理的解码帧，导致解码线程长时间未释放 CPU 资源，影响了播放流畅性

**优化策略：**
- 为避免解码线程被帧队列中的数据积压，设计一个最大缓存机制，限制帧队列中最多缓存的帧数，避免解码线程长时间被阻塞
- 另外，通过减少渲染线程的渲染频率（如调整帧率）来减轻 GPU 和 CPU 的负担，提高整体性能

### 13. 解码帧的 linesize 为 0 - 数据格式处理

**数据问题：** 播放过程中，遇到解码帧的 `linesize[0]` 值为 0 的问题，导致图像无法正确渲染。经过调试，发现是在解码和转换为 RGBA 格式的过程中，未正确处理帧的数据格式或内存分配问题

**解决方案：**
- 确保每次解码后都为 RGBA 帧分配新的内存，并使用 `sws_scale()` 转换为 RGBA 格式
- 同时，增加 `linesize` 的有效性检查，确保每帧的 `linesize` 不为零，避免空帧的渲染

---

## 💡 项目架构与技术总结

通过整个开发过程，我逐步搭建了一个完整的多线程音视频播放器架构：

```
┌─────────────────────────────────────────┐
│              Android App                │
│  ┌─────────────┐  ┌─────────────────────┐ │
│  │MainActivity │  │    Player.java      │ │
│  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────┘
                    │ JNI
┌─────────────────────────────────────────┐
│               C++ Core                  │
│  ┌─────────────┐  ┌─────────────────────┐ │
│  │ Demux Thread│  │    Timer Thread     │ │
│  └─────────────┘  └─────────────────────┘ │
│  ┌─────────────┐  ┌─────────────────────┐ │
│  │Decode Thread│  │   Render Thread     │ │
│  └─────────────┘  └─────────────────────┘ │
│  ┌─────────────┐  ┌─────────────────────┐ │
│  │AudioDec Thrd│  │  AAudio Thread      │ │
│  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────┐
│            FFmpeg + System              │
│   FFmpeg Libraries    │   AAudio API    │
│   OpenGL ES          │   Surface API    │
└─────────────────────────────────────────┘
```

### 学习总结

通过这个项目，我不仅实现了一个功能完整的音视频播放器，更重要的是：

**技术成长：**
- 掌握了 FFmpeg 的使用和多媒体编程基础
- 理解了音视频同步的核心原理和实现方法
- 学会了复杂多线程程序的设计和调试
- 熟悉了 Android Native 开发和 OpenGL 图形编程

**解决问题的能力：**
- 从最初的编译错误到最终的复杂同步问题，每个挑战都锻炼了我的问题分析和解决能力
- 学会了使用日志、调试器等工具进行系统性的问题定位
- 培养了面对复杂技术问题时的耐心和持续学习的态度

**系统性思维：**
- 理解了从需求分析到架构设计，再到具体实现的完整开发流程
- 学会了在复杂系统中平衡功能实现、性能优化和代码可维护性

通过整个过程，逐步解决了音视频播放中的多个技术难题，从音频播放、音视频同步到渲染线程的内存管理、线程同步等问题。通过调试、日志和条件变量等手段逐步排除错误，最终实现了基础的音视频播放功能。

---

*这个项目记录了我在音视频开发领域从零基础到能够独立解决复杂技术问题的完整学习历程。每一个挑战都是宝贵的学习机会，每一次突破都让我对技术的理解更加深入。*
