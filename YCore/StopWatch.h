#pragma once
#include<chrono>

class Stopwatch {
private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>; // 以秒为单位的浮点型时长

    TimePoint m_start;       // 记录启动或恢复时的时间点
    Duration m_accumulated;  // 记录暂停前已经累计的时间
    bool m_isRunning;        // 标记秒表是否正在运行

public:
    // 构造函数：初始化并自动开始计时
    Stopwatch() : m_accumulated(Duration::zero()), m_isRunning(false) 
    {

    }

    // 启动 / 重新开始计时
    void Start() {
        if (!m_isRunning) {
            m_start = Clock::now();
            m_isRunning = true;
        }
    }

    // 暂停计时
    void Pause() {
        if (m_isRunning) {
            m_accumulated += Clock::now() - m_start;
            m_isRunning = false;
        }
    }

    // 恢复计时
    void Resume() {
        if (!m_isRunning) {
            m_start = Clock::now();
            m_isRunning = true;
        }
    }

    // 重置秒表
    void Reset() {
        m_accumulated = Duration::zero();
        m_isRunning = false;
        m_start = Clock::now();
    }

    // 获取当前经过的总时间（秒）
    double ElapsedSeconds() const {
        Duration elapsed = m_accumulated;
        if (m_isRunning) {
            elapsed += Clock::now() - m_start;
        }
        return elapsed.count();
    }

    // 获取当前经过的总时间（毫秒）
    long long ElapsedMillis() const {
        double seconds = ElapsedSeconds();
        return static_cast<long long>(seconds * 1000);
    }
};