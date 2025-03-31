//
// Created by zylnt on 2025/3/31.
//

#ifndef ANDROIDPLAYER_TIMER_H
#define ANDROIDPLAYER_TIMER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>

class Timer {
public:
    Timer();                      // 构造函数
    ~Timer();                     // 析构函数

    void start();                 // 启动计时器线程
    void stop();                  // 停止计时器线程
    void pause();                 // 暂停计时器
    void resume();                // 恢复计时器
    void seekTo(double time);     // 跳转到指定时间（单位：秒）
    void setTimeSpeed(double speed); // 设置时间倍率
    double getTimeSpeed() const;     // 获取当前时间倍率

    static double getCurrentTime();           // 获取当前时间
    static void setCurrentTime(double time);  // 设置当前时间

private:
    void updateTime();            // 更新时间的内部线程函数

    // 静态时间数据（用于多个线程共享）
    static std::atomic<double> currentTime;  // 当前播放时间（单位：秒）
    static std::mutex controlMutex;          // 控制暂停/恢复的互斥锁
    static std::condition_variable pauseCond;// 暂停条件变量
    static bool paused;                      // 是否处于暂停状态
    static bool isPlaying;                  // 是否正在播放

    // 非静态成员（每个 Timer 实例自己维护）
    double timeSpeed;             // 时间倍率
    bool running;                 // 是否正在运行
    std::thread timerThread;      // 计时器线程
};

#endif //ANDROIDPLAYER_TIMER_H
