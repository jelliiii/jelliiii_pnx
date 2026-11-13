#include "dji_motor.hpp"
#include "m2006.hpp"
#include "dji_manager.hpp"
#include "timer.hpp"
#include "remoter_dr16.hpp"

motor::dji::config_t fric_config{1.0f, 1000, 1000.0f, 0.0f, 8192};
motor::dji::config_t trigger_config{36.0f, 1000, 1000.0f, 100.0f, 8192}; // 初始化配置可随时修改
motor::dji::m2006 fric_motor1;
motor::dji::m2006 fric_motor2;
motor::dji::m2006 fric_motor3;
motor::dji::m2006 trigger_motor;

motor::dji::manager motor_manager;
motor::tx_schedule schedule;

// 底层 CAN 配置 (注意这里用的是 config，不是 fd_config)
// bsp::can::fd_config h723_fric_motor = {bsp::can::handle::fdcan1, bsp::can::id_type::standard};

static pnxtypes::status initialization;
extern pnxtypes::status usart_init;

void motor_init(bsp::can::handle bus)
{
    // 修改默认配置
    fric_motor1.set_config(fric_config);
    fric_motor2.set_config(fric_config);
    fric_motor3.set_config(fric_config);
    // 2. 把电机注册到总线上（必须在 init 硬件之前！）
    motor_manager.register_motor(fric_motor1, bus, 0x201);
    motor_manager.register_motor(fric_motor2, bus, 0x202);
    motor_manager.register_motor(fric_motor3, bus, 0x203);
    motor_manager.register_motor(trigger_motor, bus, 0x204); // 设置模式
    // 3. 设置安全初始模式
    fric_motor1.motor_mode = motor::mode::relax;
    fric_motor2.motor_mode = motor::mode::relax;
    fric_motor3.motor_mode = motor::mode::relax;
    trigger_motor.motor_mode = motor::mode::relax;

    fric_motor1.cmd.velocity = 0.0f;
    fric_motor2.cmd.velocity = 0.0f;
    fric_motor3.cmd.velocity = 0.0f;
    trigger_motor.cmd.velocity = 0.0f;
    // 初始化 pid 参数
    fric_motor1.speed_pid.tune(50.0f, 0.0f, 0.1f);
    fric_motor2.speed_pid.tune(50.0f, 0.0f, 0.1f);
    fric_motor3.speed_pid.tune(50.0f, 0.0f, 0.1f);
    trigger_motor.speed_pid.tune(50.0f, 0.0f, 0.0f);
    trigger_motor.position_pid.tune(2.5f, 0.0f, 0.1f);
    // 5. 初始化 H7 时钟（DWT等）
    timer_init();

    // 6. 遥控器初始化
    dr16_remoter_init();

    // DJI 电机使用经典 CAN，即使底层 FDCAN 控制器支持 FD。
    const bsp::can::config can_config{bus, bsp::can::id_type::standard};
    initialization = bsp::can::init(can_config);
}

void remoter_to_shoot()
{
    // 拨杆在下面（下电/停止档），全部放松
    if (remote_data.right_sw == remotes::sw_state::low)
    {
        fric_motor1.motor_mode = motor::mode::relax;
        fric_motor2.motor_mode = motor::mode::relax;
        fric_motor3.motor_mode = motor::mode::relax;
        trigger_motor.motor_mode = motor::mode::relax;
        return;
    }
    // 必须电机都上线才允许发力（安全保护）
    if (fric_motor1.motor_state == motor::state::online &&
        trigger_motor.motor_state == motor::state::online)
    {
        fric_motor1.motor_mode = motor::mode::vel;
        fric_motor2.motor_mode = motor::mode::vel;
        fric_motor3.motor_mode = motor::mode::vel;
        trigger_motor.motor_mode = motor::mode::vel;

        // 测试用的低速
        fric_motor1.cmd.velocity = 10.0f;
        fric_motor2.cmd.velocity = 10.0f;
        fric_motor3.cmd.velocity = 10.0f;
        trigger_motor.cmd.velocity = 10.0f;
    }
}

void shoot_task_loop()
{
    const uint32_t now_us = get_system_time_us();

    // 1. 从 manager 信箱读取电机状态
    motor_manager.update_feedback(fric_motor1);
    motor_manager.update_feedback(fric_motor2);
    motor_manager.update_feedback(fric_motor3);
    motor_manager.update_feedback(trigger_motor);

    // 2. 读遥控器（判断模式：下电，拨盘装弹，发射）-> 控制速度
    remoter_to_shoot();

    // 3. 使用 update 统一更新 output
    motor_manager.update();

    // （未完成）4. 拨弹状态机逻辑（是否需要拨弹，是否卡弹）

    // 5. 调用 transmit 统一发送目标电流
    motor_manager.transmit(now_us);
}

static TX_THREAD shoot_thread;
// 建议：如果你之前发现断点停不下来，大概率是栈溢出了，这里建议改成 4096
static uint8_t shoot_thread_stack[4096];

void shoot_thread_entry(ULONG thread_input)
{
    motor_init(bsp::can::handle::fdcan1);
    while (1)
    {
        shoot_task_loop();
        tx_thread_sleep(1); // 1ms 控制周期
    }
}

void shoot_task_start()
{
    tx_thread_create(&shoot_thread, "shoot_task", shoot_thread_entry, 0, shoot_thread_stack,
        sizeof(shoot_thread_stack), 5, 5, TX_NO_TIME_SLICE, TX_AUTO_START);
}