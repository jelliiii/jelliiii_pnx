#include "tx_api.h"
#include "main.h"
#include "topic.hpp"
#include "include/driver_ps2.hpp"
#include "usart.h" // 假设你用了串口打印调试
#include <stdio.h>
#include "msg.hpp"

PS2_Data_TypeDef ps2;
constexpr float MAX_VX = 2.0f; // 前进最大 2 m/s
constexpr float MAX_VY = 2.0f; // 平移最大 2 m/s
constexpr float MAX_WZ = 5.0f; // 旋转最大 5 rad/s

static ChassisCommand new_cmd{0.0f, 0.0f, 0.0f};

static float normalize_stick(uint8_t raw)
{
    constexpr int center = 128;
    constexpr int deadzone = 5;

    const int value = static_cast<int>(raw) - center;

    if (value > deadzone)
    {
        return static_cast<float>(value - deadzone) / static_cast<float>(127 - deadzone);
    }

    if (value < -deadzone)
    {
        return static_cast<float>(value + deadzone) / static_cast<float>(128 - deadzone);
    }

    return 0.0f;
}

void ps2_task_loop()
{
    PS2_ReadData(&ps2);
    // 2. 简单的逻辑判断示例
    // 判断按键：如果是红灯模式 (0x73)，摇杆数据才有效
    if (ps2.mode == 0x73)
    {
        // 假定：右推LX增大，前推LY减小，右推RX增大。
        new_cmd.vx = normalize_stick(ps2.LX) * MAX_VX;
        new_cmd.vy = -normalize_stick(ps2.LY) * MAX_VY;
        new_cmd.wz = -normalize_stick(ps2.RX) * MAX_WZ;
    }
    else
    {
        // 不在模拟模式，或者接收机掉线，速度保持为 0 (安全)
        new_cmd.vx = 0.0f;
        new_cmd.vy = 0.0f;
        new_cmd.wz = 0.0f;
    }
    // 4. 发布给底盘（发之前确保 chassis_cmd_topic.init() 已经被调用过了）
    chassis_cmd_topic.publish(new_cmd);
}

void ps2_thread_entry(ULONG thread_input)
{
    PS2_Init();

    while (1)
    {
        ps2_task_loop();
        tx_thread_sleep(10); // 10ms 读取一次
    }
}
