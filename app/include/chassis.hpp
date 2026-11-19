#pragma once

#include "dji_motor.hpp"
#include "m2006.hpp"
#include "dji_manager.hpp"
#include "status.hpp"

class Chassis
{
public:
    struct chassis_config
    {
        float wheel_radius;
        float a; // 半轮距
        float b; // 半轴距
        float max_wheel_speed;
        int direction[4] = {1, 1, 1, 1}; // 实际调的时候可修改 1/-1，
    };

    motor::dji::manager chassis_motor_manager;
    motor::dji::m2006 motors[4]; // motor.cmd.velocity = direction[i] * wheel_target[i];

private:
    chassis_config config;
    float vx = 0.0f; // m/s
    float vy = 0.0f; // m/s
    float wz = 0.0f; // rad/s

    void solve_wheel_speed(); // 解算各电机速度并写入各个电机的 cmd

public:
    Chassis(chassis_config& _config) : config(_config) {};
    void set_chassis_vel(float _vx, float _vy, float _wz); // 设置底盘速度并限速
    void stop();
    void update(uint32_t now_us);
    bool all_online();
};

void Chassis::set_chassis_vel(float _vx, float _vy, float _wz)
{
    vx = _vx;
    vy = _vy;
    wz = _wz;
}

bool Chassis::all_online()
{
    bool all_online = true;
    for (auto& m : motors)
    {
        if (m.motor_state != motor::state::online)
        {
            all_online = false;
            break;
        }
    }
    return all_online;
}

void Chassis::solve_wheel_speed()
{ // 考虑限幅
    const float rotate = wz * (config.a + config.b);
    const float inverse_radius = 1.0f / config.wheel_radius;

    // 严格对应截图中 vw1、vw2、vw3、vw4。
    // 截图是轮缘线速度，除以半径后变成rad/s。
    float wheel_speed[4]{(vy - vx + rotate) * inverse_radius, (vy + vx - rotate) * inverse_radius,
        (vy - vx - rotate) * inverse_radius, (vy + vx + rotate) * inverse_radius};

    // 四轮同比例限幅，保持运动比例。
    float peak = 0.0f;

    for (float speed : wheel_speed)
    {
        const float magnitude = std::fabs(speed);

        if (magnitude > peak)
        {
            peak = magnitude;
        }
    }

    float scale = 1.0f;

    if (peak > config.max_wheel_speed)
    {
        scale = config.max_wheel_speed / peak;
    }

    for (uint8_t i = 0; i < 4; ++i)
    {
        motors[i].cmd.velocity = static_cast<float>(config.direction[i]) * wheel_speed[i] * scale;
    }
}

void Chassis::update(uint32_t now_us)
{
    // 1. 读取四轮最新反馈
    for (auto& m : motors)
    {
        chassis_motor_manager.update_feedback(m);
    }
    // 2. 检查指令和电机状态，设置模式及目标
    // 3. 解算、限幅，把目标写入各电机cmd(先判断是否在线)
    if (all_online())
    {
        solve_wheel_speed();
        for (auto& m : motors)
        {
            m.motor_mode = motor::mode::vel;
        }
    }
    else
    {
        for (auto& m : motors)
        {
            m.cmd.velocity = 0.0f;
            m.motor_mode = motor::mode::relax;
        }
    }
    // 4. 更新电机控制输出
    chassis_motor_manager.update();

    // 5. 按发送调度发送电流指令
    chassis_motor_manager.transmit(now_us);
}

void Chassis::stop()
{
    vx = 0.0f;
    vy = 0.0f;
    wz = 0.0f;

    for (auto& m : motors)
    {
        m.cmd.velocity = 0.0f;
        m.motor_mode = motor::mode::relax;
    }
}
