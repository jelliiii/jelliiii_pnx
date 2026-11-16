#include "tx_api.h"
#include "main.h"

#include "include/app_ps2.hpp"
#include "usart.h" // 假设你用了串口打印调试
#include <stdio.h>
PS2_Data_TypeDef ps2;

void ps2_task_loop()
{
    PS2_ReadData(&ps2);
    // 2. 简单的逻辑判断示例

    // 判断按键：如果是红灯模式 (0x73)，摇杆数据才有效
    if (ps2.mode == 0x73)
    {
        // 获取左摇杆 X 轴 (0-255)，转换为 -128 到 127
        int16_t lx_val = (int16_t)ps2.LX - 128;
        int16_t ly_val = (int16_t)ps2.LY - 128;

        // 这里可以放入你的电机控制代码
        // 例如：如果 lx_val > 50，向右转
    }
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
