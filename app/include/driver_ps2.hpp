

#ifndef __AX_PS2_HPP
#define __AX_PS2_HPP

#include <stdint.h>

extern "C"
{
#if defined(STM32H723xx)
#include "stm32h7xx_hal.h"
#elif defined(STM32F407xx)
#include "stm32f4xx_hal.h"
#else
#error "Unsupported STM32 device for app_ps2.hpp"
#endif
    // 如果 main.h 里没有引 HAL 库，你可能得加上这句：
    // include the HAL header selected above.
}

/*
 * PS2 引脚定义 (根据你的代码调整)
 * 请确保在 CubeMX 中将这些引脚设置为 GPIO 输出/输入模式
 */
#define PS2_DI_PIN GPIO_PIN_12  // PA4 - 数据输入
#define PS2_CMD_PIN GPIO_PIN_13 // PA5 - 命令输出
#define PS2_CS_PIN GPIO_PIN_14  // PA8 - 片选
#define PS2_CLK_PIN GPIO_PIN_15 // PC9 - 时钟

#define PS2_DI_PORT GPIOB
#define PS2_CMD_PORT GPIOB
#define PS2_CS_PORT GPIOB
#define PS2_CLK_PORT GPIOB

/* PS2 按键位定义 (对应数据结构) */
// 注意：PS2 按下为 0，代码中会取反，所以这里 1 代表按下
typedef struct
{
    uint8_t select : 1;
    uint8_t L3 : 1;
    uint8_t R3 : 1;
    uint8_t start : 1;
    uint8_t up : 1;
    uint8_t right : 1;
    uint8_t down : 1;
    uint8_t left : 1;
} PS2_Btn_Group1;

typedef struct
{
    uint8_t L2 : 1;
    uint8_t R2 : 1;
    uint8_t L1 : 1;
    uint8_t R1 : 1;
    uint8_t triangle : 1; // 三角
    uint8_t circle : 1;   // 圆圈
    uint8_t cross : 1;    // 叉
    uint8_t square : 1;   // 方块
} PS2_Btn_Group2;

/* 完整手柄数据结构体 */
typedef struct
{
    uint8_t mode;        // 手柄模式 (0x41 数字, 0x73 模拟)
    PS2_Btn_Group1 btn1; // 方向键与功能键
    PS2_Btn_Group2 btn2; // 动作键与肩键
    uint8_t RX;          // 右摇杆 X (0-255)
    uint8_t RY;          // 右摇杆 Y (0-255)
    uint8_t LX;          // 左摇杆 X (0-255)
    uint8_t LY;          // 左摇杆 Y (0-255)
} PS2_Data_TypeDef;

/* 函数声明 */
void PS2_Init(void);
void PS2_ReadData(PS2_Data_TypeDef* ps2_data);
void PS2_Delay_us(uint16_t us); // 微秒延时

#endif