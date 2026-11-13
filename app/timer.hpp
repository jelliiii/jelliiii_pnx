#include "stm32h7xx_hal.h"

extern uint32_t SystemCoreClock;
uint32_t cycles_per_us = 260;

void timer_init()
{
    // 启用内核调试单元
    SET_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);

    // STM32H7 特有步骤：解锁 DWT 寄存器写访问
    DWT->LAR = 0xC5ACCE55;

    // 启动周期计数器
    SET_BIT(DWT->CTRL, DWT_CTRL_CYCCNTENA_Msk);

    DWT->CYCCNT = 0;
}

uint32_t get_system_time_us()
{
    return DWT->CYCCNT / cycles_per_us;
}