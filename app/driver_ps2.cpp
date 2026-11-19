#include "include/driver_ps2.hpp"
#include "bsp_dwt.hpp"
// 内部接收缓冲区
static uint8_t PS2_Rx_Buffer[9];

// 发送命令数组 (请求数据)
// 0x01: 起始位, 0x42: 读取数据命令, 后面补0
static const uint8_t PS2_Tx_Buffer[9] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief 微秒级延时 (必须精准，否则通信失败)
 * @param us: 延时微秒数
 */

void PS2_Init(void)
{
    HAL_GPIO_WritePin(PS2_CS_PORT, PS2_CS_PIN, GPIO_PIN_SET);   // CS 拉高，待机
    HAL_GPIO_WritePin(PS2_CLK_PORT, PS2_CLK_PIN, GPIO_PIN_SET); // CLK 拉高，待机
    HAL_GPIO_WritePin(PS2_CMD_PORT, PS2_CMD_PIN, GPIO_PIN_SET); // CMD 一般也拉高

    // 5. (可选) 发送复位或配置命令，将手柄唤醒至模拟模式
    // ...
}

/**
 * @brief 发送一个字节并同时接收一个字节 (SPI 模拟核心)
 * @param tx_data: 要发送的数据
 * @return: 接收到的数据
 */
static uint8_t PS2_Transfer_Byte(uint8_t tx_data)
{
    uint8_t rx_data = 0;
    uint8_t i;

    // PS2 协议是 LSB First (低位先发)
    for (i = 0; i < 8; i++)
    {
        // 1. 准备发送位
        if (tx_data & (1 << i))
        {
            HAL_GPIO_WritePin(PS2_CMD_PORT, PS2_CMD_PIN, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(PS2_CMD_PORT, PS2_CMD_PIN, GPIO_PIN_RESET);
        }

        // 2. 拉低时钟 (开始传输)
        HAL_GPIO_WritePin(PS2_CLK_PORT, PS2_CLK_PIN, GPIO_PIN_RESET);
        bsp::dwt::delay_us(5); // 延时约 5us

        // 3. 读取返回位
        if (HAL_GPIO_ReadPin(PS2_DI_PORT, PS2_DI_PIN) == GPIO_PIN_SET)
        {
            rx_data |= (1 << i);
        }

        // 4. 拉高时钟 (结束传输)
        HAL_GPIO_WritePin(PS2_CLK_PORT, PS2_CLK_PIN, GPIO_PIN_SET);
        bsp::dwt::delay_us(5); // 延时约 5us
    }
    return rx_data;
}

/**
 * @brief 读取 PS2 完整数据包
 * @param ps2_data: 指向存储结果的结构体指针
 */
void PS2_ReadData(PS2_Data_TypeDef* ps2_data)
{
    uint8_t i;

    // 1. 开始通信：拉低 CS
    HAL_GPIO_WritePin(PS2_CS_PORT, PS2_CS_PIN, GPIO_PIN_RESET);

    // 2. 发送 9 个字节命令，同时接收 9 个字节数据
    for (i = 0; i < 9; i++)
    {
        PS2_Rx_Buffer[i] = PS2_Transfer_Byte(PS2_Tx_Buffer[i]);
    }

    // 3. 结束通信：拉高 CS
    HAL_GPIO_WritePin(PS2_CS_PORT, PS2_CS_PIN, GPIO_PIN_SET);

    // 4. 解析数据
    // 检查模式 (Byte 1)
    ps2_data->mode = PS2_Rx_Buffer[1];

    // 解析按键组 1 (Byte 3) - 取反因为按下是 0
    ps2_data->btn1.select = !(PS2_Rx_Buffer[3] & 0x01);
    ps2_data->btn1.L3 = !(PS2_Rx_Buffer[3] & 0x02);
    ps2_data->btn1.R3 = !(PS2_Rx_Buffer[3] & 0x04);
    ps2_data->btn1.start = !(PS2_Rx_Buffer[3] & 0x08);
    ps2_data->btn1.up = !(PS2_Rx_Buffer[3] & 0x10);
    ps2_data->btn1.right = !(PS2_Rx_Buffer[3] & 0x20);
    ps2_data->btn1.down = !(PS2_Rx_Buffer[3] & 0x40);
    ps2_data->btn1.left = !(PS2_Rx_Buffer[3] & 0x80);

    // 解析按键组 2 (Byte 4)
    ps2_data->btn2.L2 = !(PS2_Rx_Buffer[4] & 0x01);
    ps2_data->btn2.R2 = !(PS2_Rx_Buffer[4] & 0x02);
    ps2_data->btn2.L1 = !(PS2_Rx_Buffer[4] & 0x04);
    ps2_data->btn2.R1 = !(PS2_Rx_Buffer[4] & 0x08);
    ps2_data->btn2.triangle = !(PS2_Rx_Buffer[4] & 0x10);
    ps2_data->btn2.circle = !(PS2_Rx_Buffer[4] & 0x20);
    ps2_data->btn2.cross = !(PS2_Rx_Buffer[4] & 0x40);
    ps2_data->btn2.square = !(PS2_Rx_Buffer[4] & 0x80);

    // 解析摇杆 (Byte 5-8)
    ps2_data->RX = PS2_Rx_Buffer[5];
    ps2_data->RY = PS2_Rx_Buffer[6];
    ps2_data->LX = PS2_Rx_Buffer[7];
    ps2_data->LY = PS2_Rx_Buffer[8];
}