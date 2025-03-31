//
// Created by zylnt on 2025/3/31.
//
#include "timer.h"
#include "log.h"
#define TAG "timer"

std::atomic<double> Timer::currentTime(0.0);  // 当前时间
double baseTime = 0.0;  // 初始时间偏移
std::mutex Timer::controlMutex; // 保护暂停和恢复操作的互斥锁
std::condition_variable Timer::pauseCond; // 用于控制暂停和恢复
bool Timer::paused = false; // 当前是否处于暂停状态

Timer::Timer() : running(false), timeSpeed(1.0) {} // 默认时间倍率为 1.0

Timer::~Timer() {
    if (running) {
        stop();
    }
}

void Timer::start() {
    if (!running) {
        baseTime = currentTime.load();  // 初始时间偏移
        running = true;
        paused = false;
        timerThread = std::thread(&Timer::updateTime, this); // 启动计时器线程
    }
}

void Timer::stop() {
    if (running) {
        running = false;
        paused = false;
        if (timerThread.joinable()) {
            timerThread.join();  // 等待线程结束
        }
    }
}

void Timer::pause() {
    std::lock_guard<std::mutex> lock(controlMutex);
    if (!paused) {
        paused = true;
        pauseCond.notify_all();  // 通知线程暂停
        LOGI("⏸️ Timer paused");
    }
}

void Timer::resume() {
    std::lock_guard<std::mutex> lock(controlMutex);
    if (paused) {
        paused = false;
        pauseCond.notify_all();  // 通知线程恢复
        LOGI("▶️ Timer resumed");
    }
}

void Timer::seekTo(double time) {
    std::lock_guard<std::mutex> lock(controlMutex);
    setCurrentTime(time);
    LOGI("🎯 Timer seeked to %.3f", time);
}

void Timer::setTimeSpeed(double speed) {
    if (speed <= 0) {
        LOGE("❌ Invalid time speed: %f. Must be greater than 0.", speed);
        return;
    }
    timeSpeed = speed;
    LOGI("⏱️ Time speed set to: %f", timeSpeed);
}

double Timer::getTimeSpeed() const {
    return timeSpeed;
}

double Timer::getCurrentTime() {
    return currentTime.load();
}

void Timer::setCurrentTime(double time) {
    currentTime.store(time);  // 更新当前时间
}

void Timer::updateTime() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    double baseTime = getCurrentTime(); // 从当前时间作为基准时间

    while (running) {
        {
            std::unique_lock<std::mutex> lock(controlMutex);
            if (paused) {
                pauseCond.wait(lock, [&]() { return !paused || !running; });
                startTime = steady_clock::now();
                baseTime = getCurrentTime();
                continue;
            }
        }

        auto now = steady_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        double adjustedTime = baseTime + elapsed * timeSpeed;

        setCurrentTime(adjustedTime);

        std::this_thread::sleep_for(milliseconds(5)); // 每5毫秒更新一次
    }

    LOGI("🛑 Timer stopped");
}
