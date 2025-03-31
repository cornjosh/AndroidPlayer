//
// Created by zylnt on 2025/3/31.
//

#ifndef ANDROIDPLAYER_TIMER_H
#define ANDROIDPLAYER_TIMER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>

class Timer {
public:
    Timer();  // 构造函数
    ~Timer(); // 析构函数
    void start(); // 启动计时器
    void stop();  // 停止计时器
    static double getCurrentTime(); // 获取当前时间
    static void setCurrentTime(double time); // 设置当前时间
    void setTimeSpeed(double speed); // 设置时间倍率

private:
    static std::atomic<double> currentTime; // 当前时间，原子变量
    static std::mutex timeMutex; // 保护时间的互斥锁
    double timeSpeed; // 时间倍率
    bool running; // 是否正在运行
    std::thread timerThread; // 定时器线程
    void updateTime(); // 更新时间
};

#endif //ANDROIDPLAYER_TIMER_H
