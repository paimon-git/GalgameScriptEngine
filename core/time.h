#pragma once

// 时间管理：帧间隔（秒）与累计运行时间
class Time
{
public:
    void update();

    float delta() const { return delta_; }
    double elapsed() const { return elapsed_; }
    int frame() const { return frame_; }

private:
    float delta_ = 0.0f;
    double elapsed_ = 0.0;
    int frame_ = 0;
};
