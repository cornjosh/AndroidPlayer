//
// Created by zylnt on 2025/3/31.
//
#include "timer.h"
#include "log.h"
#define TAG "timer"

// 初始化静态变量
std::atomic<double> Timer::currentTime(0.0);
double baseTime = 0.0;  // 初始时间偏移

Timer::Timer() : running(false), timeSpeed(1.0) {} // 默认时间倍率为 1.0

Timer::~Timer() {
    if (running) {
        stop();
    }
}

void Timer::start() {
    if (!running) {
        baseTime = currentTime.load();  // <- 重要
        running = true;
        timerThread = std::thread(&Timer::updateTime, this); // 启动计时器线程
    }
}

void Timer::stop() {
    if (running) {
        running = false;
        if (timerThread.joinable()) {
            timerThread.join();  // 等待线程结束
        }
    }
}

void Timer::updateTime() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();

    while (running) {
        auto now = steady_clock::now();
        double elapsedSeconds = duration_cast<duration<double>>(now - startTime).count();

        double adjustedTime = baseTime + elapsedSeconds * timeSpeed;

        setCurrentTime(adjustedTime);

        std::this_thread::sleep_for(milliseconds(50));
        LOGD("⏱️ Current Time: %.6f seconds", adjustedTime);
    }
}

double Timer::getCurrentTime() {
    return currentTime.load();
}

void Timer::setCurrentTime(double time) {
    currentTime.store(time);  // 更新当前时间
}

void Timer::setTimeSpeed(double speed) {
    if (speed <= 0) {
        LOGE("❌ Invalid time speed: %f. Must be greater than 0.", speed);
        return;
    }
    timeSpeed = speed;
    LOGI("⏱️ Time speed set to: %f", timeSpeed);
}
