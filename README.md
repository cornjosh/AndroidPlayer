# Day14

## 作业要求

- 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`，并且可以在 Android 上调用
- 使用 FFmpeg 进行下面的操作
- 创建 `log.h` 文件，实现 `LOGE`、`LOGI` 等日志打印宏
- 创建 `packetQueue.h` 和 `packetQueue.cpp` 文件，实现一个线程安全的包队列
- 创建 `demuxer.cpp` 文件，实现 `demux` 线程，对原视频进行解封装，得到 `video packet`
- 得到的 `video packet` 需要放入到 `videoPacketQueue` 中
- 创建 `decoder.cpp` 文件，实现 `decode` 线程，对 `video packet` 进行解码，得到 `video frame`
- 需要从 `videoPacketQueue` 中获取 `video packet` 进行解码
- 解码后的 `video frame` 需要以 `RGBA` 格式放入到 `frameQueue` 中


- 创建 `frameQueue.h` 和 `frameQueue.cpp` 文件，实现一个线程安全的帧队列
- 创建 `renderer.cpp` 文件，实现从 `frameQueue` 中获取 `video frame`，并使用 `OpenGL` 渲染到指定的
  `SurfaceView` 上
- 创建 `player.cpp` 文件，负责和 Java 层进行交互，并控制三个进程分别进行 `demuxe`,`decode`,`render`

- 创建 `audioPacketQueue`，实现将 `demux` 线程中获取的 `audio packet` 放入到 `audioPacketQueue` 中
- 创建 `audioDecoder.cpp`，实现 `audioDecode` 线程，负责对 `audio packet` 进行解码，得到 `audio pcm`
- 创建 `audioRingBuffer.h` 和 `audioRingBuffer.cpp` 文件，实现一个环形缓冲区，用于存储 `audio pcm`
- 将解码后的 `audio pcm` 放入到 `audioRingBuffer` 中
- 在 `player.cpp` 中实现 `audioDecode` 线程，负责和 Java 层进行交互，并控制 `audioDecode` 线程进行解码
- 创建 `AAudioPlayer.cpp` 文件，从环形缓冲区中获取 `audio pcm`，并使用 `AAudio` 播放音频
- 在 `player.cpp` 中实现 `AAudioPlayer` 线程，负责和 Java 层进行交互，并控制 `AAudioPlayer` 线程进行播放
- 实现 `audio` 和 `video` 的同步

- 附加功能
    - 实现精确 `seek` 和 非精确 `seek` 功能
    - 实现倍速播放

## 作业录屏

[录屏4](录屏4.mp4)

## 已实现功能

- [x] 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`
- [x] 创建 `log.h` 文件，实现 `LOGE`、`LOGI` 等日志打印宏
- [x] 两个 `packetQueue`，一个 `videoPacketQueue` 和一个 `audioPacketQueue`，分别用于存放 `video packet` 和
  `audio packet`
- [x] 一个 `frameQueue`，用于存放 `video frame`
- [x] 一个 `audioRingBuffer`，用于存放 `audio pcm`
- [x] 一个 `demux` 线程，对原视频进行解封装，得到 `video packet` 和 `audio packet`，并
  放入到 `videoPacketQueue` 和 `audioPacketQueue` 中
- [x] 一个 `decode` 线程，从 `videoPacketQueue` 中获取 `video packet` 进行解码，得到 `video frame`，
  并放入到 `frameQueue` 中
- [x] 一个 `audioDecode` 线程，从 `audioPacketQueue` 中获取 `audio packet` 进行解码，得到 `audio pcm`，
  并放入到 `audioRingBuffer` 中
- [x] 一个 `renderer` 线程，从 `frameQueue` 中获取 `video frame`，并使用 `OpenGL` 渲染到指定的
  `SurfaceView` 上
- [x] 一个 `AAudioPlayer` 线程，从 `audioRingBuffer` 中获取 `audio pcm`，并使用 `AAudio` 播放音频
- [x] 一个 `Timer` 线程，负责控制总体的播放进度
- [x] 使用 `Timer` 线程控制 `video` 和 `audio` 的同步播放

### 附加功能

- [x] 使用 `Timer` 实现进度条的显示功能
- [x] 使用 `Timer` 实现非精确 `seek` 功能
- [x] 使用 `Timer` 实现倍速播放功能（暂时只能视频倍速）
- [x] 使用 `Timer` 实现暂停和继续播放功能
- [x] 使用 `isInit` 实现播放器的初始化和释放功能（优雅的退出）

## 主要难点

### 1. **AAudio 播放器无法播放音频**
最初遇到的一个主要问题是音频无法播放。通过逐步排查和调试，发现：
- `AAudioStream` 未正确设置音频流格式，或者流未正确打开
- 尝试创建 `AAudioStreamBuilder` 并设置音频流参数时，某些地方处理不当，例如未正确调用 `AAudioStreamBuilder_openStream` 或者音频格式设置不匹配

**解决方案**：
- 使用 `AAudio` API 创建流时，确保格式、通道数、采样率等参数正确设置，并调用 `AAudioStream_requestStart` 启动流
- 增加适当的日志输出，以便在调试过程中及时发现问题

### 2. **音频无法正确播放和流的关闭**
播放音频时，发现尽管缓冲区内的数据已被读取，但音频播放仍存在问题，音频流似乎未正常工作

**解决方案**：
- 通过调试日志查看音频流的状态，并验证 `AAudioStream_write` 是否成功写入数据
- 确保使用完音频流后，调用 `AAudioStream_requestStop` 和 `AAudioStream_close` 正确关闭流

### 3. **获取和同步音频的 PTS**
实现音视频同步时，关键问题是如何从音频流中获取正确的播放时间戳（PTS）。尝试使用 `AAudioStream_getFramesWritten` 和 `AAudioStream_getSampleRate` 计算音频的 PTS（播放时间戳）

**解决方案**：
- 使用 `AAudioStream_getFramesWritten` 获取已写入的帧数，并通过音频流的采样率计算出时间戳（PTS）
- 确保音频的时间戳更新与实际播放同步，避免播放过程中出现音频时滞或同步问题

### 4. **音视频同步问题**
实现音视频同步时，希望通过获取音频的 PTS 来同步音频和视频播放。然而，音视频线程的同步是复杂过程，尤其要确保时间戳的一致性并避免两者不同步

**解决方案**：
- 设计一个 **中央时间更新时间线程**，该线程定期更新时间戳，并通过条件变量通知音视频线程
- 在视频和音频线程中，通过比较当前时间戳与各自的数据 `PTS`，确保视频和音频播放同步

### 5. **线程安全的时间戳变量**
为确保多个线程安全访问时间戳变量（`currentTime`），使用 `std::atomic` 存储全局的时间戳，并利用 `std::mutex` 和 `std::condition_variable` 控制时间的同步

**解决方案**：
- 通过 `std::atomic<double>` 存储时间戳，保证线程间的安全访问
- 使用 `std::condition_variable` 同步不同线程之间的操作，确保音频和视频线程在读取时间戳时不会发生竞争

### 6. **音频和视频线程的时间戳同步**
音频和视频线程都需依据中央时间戳进行同步。为保证音频和视频同步播放，设计一个专门更新时间的线程来维护音视频的统一时间进度，使音频和视频线程可基于同一时间戳调整播放速度，避免因时差导致不同步

**解决方案**：
- 每隔一定时间（每 5 毫秒），时间线程更新时间戳，音频和视频线程通过读取该时间戳决定播放进度
- 使用 `std::this_thread::sleep_for` 模拟音视频的帧率，并确保两个线程能根据更新的时间戳决定是否继续播放或等待

### 7. **编译成功但播放音视频依旧不同步**
成功编译并能播放音频和视频后，仍面临音视频不同步的问题，主要是音频的 PTS 和视频的播放进度不一致，导致两者不同步

**解决方案**：
- 引入音视频同步机制，确保音频和视频根据各自的 PTS 计算出合适的播放时间
- 通过调整线程等待时间，确保音频和视频线程可根据时间戳的变化进行播放

### 8. **解码线程（`decodeThread`）退出后播放中断**
实现视频播放时，遇到一个关键问题：**解码线程（`decodeThread`）执行完后终止，导致视频播放中断**。原本的代码在解码线程完成后便退出，使播放器未继续处理后续的视频帧

**解决方案**：
- 调试发现 `decodeThread` 线程的退出条件设置不合理，导致其处理完所有的包后直接退出。为让解码线程继续工作，修改退出条件，确保解码线程只有在队列完全消费完且无更多数据时才退出
- 增加一个机制，当 `packetQueue` 中无更多数据时，解码线程稍作等待，直到有新数据进来，避免线程提前退出

### 9. **解码帧的内存泄漏问题**
播放过程中，设备死机很大程度上是由内存泄漏引起的，特别是在解码阶段，未及时释放已解码的 `AVFrame`。这导致内存不断积累，最终设备内存溢出，系统被强制杀掉

**解决方案**：
- 加强内存管理，每次解码并将帧推入队列后，确保及时释放不再使用的帧。特别是使用完 `AVFrame` 后，通过 `av_frame_free()` 确保内存得到正确释放
- 为避免每次都创建和销毁大量 `AVFrame` 对象，实现帧的复用机制，避免频繁的内存分配和释放

### 10. **Surface 未正确显示导致黑屏**
将渲染帧通过 OpenGL 渲染到 `SurfaceView` 时，发现渲染输出黑屏。经过调试，发现是因为 `SurfaceView` 未正确设置约束，导致渲染的 Surface 尺寸为 0x0

**解决方案**：
- 确保 `SurfaceView` 使用 `ConstraintLayout` 时，添加正确的约束（`layout_constraintTop_toTopOf`、`layout_constraintStart_toStartOf` 等），使 `SurfaceView` 能够正确渲染到屏幕上
- 同时，在 `SurfaceView` 中设置调试背景色，确保视图显示正常，并进一步验证 `Surface` 在渲染时的宽高正确

### 11. **EGL 初始化重复问题**
实现渲染后，发现在播放过程中，`EGL` 被重复初始化，导致渲染输出异常或黑屏。重复初始化 EGL 会破坏现有的上下文，导致图形渲染失败

**解决方案**：
- 使用 `std::mutex` 加锁，确保每次只初始化一次 `EGL` 上下文。每次渲染前，检查 `EGL` 是否已经初始化，仅在必要时重新初始化
- 此外，增加对 `Surface` 和 `EGLContext` 的管理，确保在 `Surface` 或视图尺寸变化时，能够安全地重建 `EGL` 上下文

### 12. **重复解码导致的性能问题**
随着播放时间的增加，还遇到了解码性能逐渐下降的问题，尤其是帧队列中积压了大量待处理的解码帧，导致解码线程长时间未释放 CPU 资源，影响了播放流畅性

**解决方案**：
- 为避免解码线程被帧队列中的数据积压，设计一个最大缓存机制，限制帧队列中最多缓存的帧数，避免解码线程长时间被阻塞
- 另外，通过减少渲染线程的渲染频率（如调整帧率）来减轻 GPU 和 CPU 的负担，提高整体性能

### 13. **解码帧的 `linesize` 为 0**
播放过程中，遇到解码帧的 `linesize[0]` 值为 0 的问题，导致图像无法正确渲染。经过调试，发现是在解码和转换为 RGBA 格式的过程中，未正确处理帧的数据格式或内存分配问题

**解决方案**：
- 确保每次解码后都为 RGBA 帧分配新的内存，并使用 `sws_scale()` 转换为 RGBA 格式
- 同时，增加 `linesize` 的有效性检查，确保每帧的 `linesize` 不为零，避免空帧的渲染

### 总结：
通过整个过程，逐步解决了音视频播放中的多个技术难题，从音频播放、音视频同步到渲染线程的内存管理、线程同步等问题。通过调试、日志和条件变量等手段逐步排除错误，最终实现了基础的音视频播放功能







## 以实现视频和音频的同步播放的目标

[录屏3](录屏3.mp4)


# Day13

## 作业要求

- 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`，并且可以在 Android 上运行
- 创建 `log.h` 文件，实现 `LOGE`、`LOGI` 等日志打印宏
- 创建 `packetQueue.h` 和 `packetQueue.cpp` 文件，实现一个线程安全的包队列
- 创建 `demuxer.cpp` 文件，实现 `demux` 线程，对原视频进行解封装，得到 `video packet`
- 得到的 `video packet` 需要放入到 `queue` 中
- 创建 `decoder.cpp` 文件，实现 `decode` 线程，对 `video packet` 进行解码，得到 `video frame`
- 需要从 `queue` 中获取 `video packet` 进行解码
- 解码后的 `video frame` 需要以 `RGBA` 格式放入到 `frameQueue` 中


- 创建 `frameQueue.h` 和 `frameQueue.cpp` 文件，实现一个线程安全的帧队列
- 创建 `renderer.cpp` 文件，实现从 `frameQueue` 中获取 `video frame`，并使用 `OpenGL` 渲染到指定的
  `SurfaceView` 上
- 创建 `player.cpp` 文件，负责和 Java 层进行交互，并控制三个进程分别进行 `demuxe`,`decode`,`render`

## 作业完成录屏

[录屏](录屏1.mp4)


# Day12

## 作业要求

- 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`，并且可以在 Android 上运行
- 创建 `log.h` 文件，实现 `LOGE`、`LOGI` 等日志打印宏
- 创建 `queue.h` 和 `queue.cpp` 文件，实现一个线程安全的队列
- 创建 `demuxer.cpp` 文件，实现 `demux` 线程，对原视频进行解封装，得到 `video packet`
- 得到的 `video packet` 需要放入到 `queue` 中
- 创建 `decoder.cpp` 文件，实现 `decode` 线程，对 `video packet` 进行解码，得到 `video frame`
- 需要从 `queue` 中获取 `video packet` 进行解码
- 解码后的 `video frame` 需要以 `YUV420` 格式存储到文件中

![屏幕截图 2025-03-29 084654.png](img/%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE%202025-03-29%20084654.png)

## 作业完成录屏

[录屏](录屏.mp4)

## 初始化项目

- 使用示例代码创建一个 Android Studio 项目
- 使用 git 管理代码，设置 remote
- 尝试编译运行示例代码，发现编译错误，没有 `log.h` 文件
- 创建 `log.h` 文件，添加 `#include <android/log.h>` 头文件
- 创建 `/src/main/assets/testfile.mp4` 文件，添加测试视频文件

## 交叉编译 FFmpeg

- 修改之前作业中的 `ffmpeg.sh` 脚本，把动态库的编译和安装部分注释掉，改为静态库的编译和安装，并生成
  `libavcodec.a`、`libavformat.a`、`libavutil.a`、`libswscale.a`、`libswresample.a`、`libavfilter.a` 等静态库文件
- 编译 FFmpeg 的时候，使用 `--enable-static` 和 `--disable-shared` 选项
- 然后将静态文件链接到一个动态库 `libffmpeg.so` 中，代码如下

```bash
echo "开始编译ffmpeg so"
# NDK路径.
NDK=/home/ubuntu2204/Android/Sdk/ndk/21.3.6528147
PLATFORM=$NDK/platforms/android-21/arch-arm64
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
PREFIX=/home/ubuntu2204/code/day9/build/ffmpeg/android_outputS/usr/local
# 输出PREFIX的值用于调试
echo "PREFIX: $PREFIX"
# 设置LIBRARY_PATH环境变量
export LIBRARY_PATH=$LIBRARY_PATH:$PREFIX/lib
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
# 检查libffmpeg.so是否生成成功
if [ -f "$PREFIX/libffmpeg.so" ]; then
    $TOOLCHAIN/bin/aarch64-linux-android-strip  $PREFIX/libffmpeg.so
    echo "完成编译ffmpeg so"
else
    echo "编译失败，未生成libffmpeg.so文件"
fi
```

### 处理 FFmpeg 头文件

- 将 FFmpeg 的头文件复制到 `jniLibs` 目录下，方便后续的编译和使用
- 更新 `CMakeLists.txt` 文件，添加 FFmpeg 的头文件路径

```cmake
include_directories(${ffmpeg_head_dir}/../jniLibs/include)
```

### TODO
- [x] 交叉编译 FFmpeg 静态库
- [x] 打包成一个动态库
- [ ] 按需打包：等待后面的作业需求

## 实现一个线程安全的队列

- 创建 `queue.h` 和 `queue.cpp` 文件
- 实现一个线程安全的队列，使用 `std::mutex` 和 `std::condition_variable` 来实现线程安全
- 使用 `std::queue` 来存储数据
- 实现 `put` 和 `pop` 方法

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

## 实现 demux 线程

- 创建 `demuxer.cpp` 文件
- 实现 `demux` 线程，对原视频进行解封装，得到 `video packet`
- 得到的 `video packet` 需要放入到 `queue` 中
- 使用 FFmpeg 的 `av_read_frame` 函数来读取视频数据
- 使用 `av_packet_split_side_data` 函数来分离视频数据
- 遍历所有流，查找视频流的索引
- 将视频流放入到 `queue` 中
- 其他流丢弃

## 实现 decode 线程

- 创建 `decoder.cpp` 文件
- 实现 `decode` 线程，对 `video packet` 进行解码，得到 `video frame`
- 需要从 `queue` 中获取 `video packet` 进行解码
- 解码后的 `video frame` 需要以 `YUV420P` 格式存储到文件中
- 使用 FFmpeg 的 `avcodec_decode_video2` 函数来解码视频数据
- 使用 `av_frame_get_buffer` 函数来分配内存
- 将 queue 中的 `video packet` 进行解码
- 将解码后的 `video frame` 直接写入到文件中
- 最后关闭文件，释放内存

## 创建线程

- 在 `main.cpp` 中创建 `demux` 和 `decode` 线程
- 使用 `std::thread` 来创建线程
- 使用 `std::thread::join` 来等待线程结束
- 在 mainActivity 中调用 `main` 函数
- 暂时将 `main` 函数放在主线程中运行，后期会改为在子线程中运行，避免阻塞 UI 线程
