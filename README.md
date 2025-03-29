# Day12

## 作业要求

- 交叉编译 FFmpeg 静态库，并打包成一个动态库 `libffmpeg.so`，并且可以在 Android 上运行
- 创建 `queue.h` 和 `queue.cpp` 文件，实现一个线程安全的队列
- 创建 `demuxer.cpp` 文件，实现 `demux` 线程，对原视频进行解封装，得到 `video packet`
- 得到的 `video packet` 需要放入到 `queue` 中
- 创建 `decoder.cpp` 文件，实现 `decode` 线程，对 `video packet` 进行解码，得到 `video frame`
- 需要从 `queue` 中获取 `video packet` 进行解码
- 解码后的 `video frame` 需要以 `YUV420P` 格式存储到文件中

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