# Day12

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

### TODO
- [x] 交叉编译 FFmpeg 静态库
- [x] 打包成一个动态库
- [ ] 按需打包：等待后面的作业需求

