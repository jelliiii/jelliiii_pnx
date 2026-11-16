
#include "bsp_can.hpp"
#include "shoot_motor.hpp"
#include "remoter_ps2.hpp"

static TX_THREAD shoot_thread;
static uint8_t shoot_thread_stack[4096];
extern void shoot_thread_entry(ULONG thread_input);

static TX_THREAD ps2_thread;
static uint8_t ps2_thread_stack[4096];
extern void ps2_thread_entry(ULONG thread_input);

void shoot_task_start()
{
    tx_thread_create(&shoot_thread, "shoot_task", shoot_thread_entry, 0, shoot_thread_stack,
        sizeof(shoot_thread_stack), 5, 5, TX_NO_TIME_SLICE, TX_AUTO_START);
}

void ps2_task_start()
{
    tx_thread_create(&ps2_thread, "ps2_task", ps2_thread_entry, 0, ps2_thread_stack,
        sizeof(ps2_thread_stack), 3, 3, TX_NO_TIME_SLICE, TX_AUTO_START);
}

extern "C" void app_start()
{
    ps2_task_start();
    shoot_task_start();
}
