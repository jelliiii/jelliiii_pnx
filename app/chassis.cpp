#include "chassis.hpp"
#include "bsp_can.hpp"
#include "bsp_dwt.hpp"
#include "topic.hpp"

// TODO:电机 id，底盘参数，电机配置，底盘前进平移旋转最大速度，pid 参数
Chassis::chassis_config _chassis_config{}; // 设置物理参数
Chassis chassis(_chassis_config);
motor::dji::config_t motor_config{36.0f, 1000, 1000.0f, 100.0f, 8192}; // 初始化配置可随时修改

static pnxtypes::status can_init;
static pnxtypes::status topic_init;

static msg::subscriber<ChassisCommand> chassis_sub;

void chassis_init(bsp::can::handle bus)
{
    // 1.注册电机按照顺序： 右前，左前，左后，右后
    chassis.chassis_motor_manager.register_motor(chassis.motors[0], bus, id);
    chassis.chassis_motor_manager.register_motor(chassis.motors[1], bus, id);
    chassis.chassis_motor_manager.register_motor(chassis.motors[2], bus, id);
    chassis.chassis_motor_manager.register_motor(chassis.motors[3], bus, id);
    // 2.设置 pid 参数
    chassis.motors[0].speed_pid.tune(50.0f, 0.0f, 0.1f);
    chassis.motors[1].speed_pid.tune(50.0f, 0.0f, 0.1f);
    chassis.motors[2].speed_pid.tune(50.0f, 0.0f, 0.1f);
    chassis.motors[3].speed_pid.tune(50.0f, 0.0f, 0.1f);
    // 3.修改电机默认配置
    for (uint8_t i = 0; i < 4; ++i)
    {
        chassis.motors[i].set_config(motor_config);
    }
    // 4.恢复安全relax 模式，速度为 0
    for (uint8_t i = 0; i < 4; ++i)
    {
        chassis.motors[i].motor_mode = motor::mode::relax;
        chassis.motors[i].cmd.velocity = 0.0f;
    }
    // 初始化时钟放在 appstart 里面了，防止 ps2 反复初始化
    // 遥控器新创建了一个线程，不用放在底盘里面
    // 5.配置 can 并初始化
    const bsp::can::config can_config{bus, bsp::can::id_type::standard};
    can_init = bsp::can::init(can_config);
    // 6.初始化 topic 以便线程之间通信+注册线程到这个 topic 上
    topic_init = chassis_cmd_topic.init();
    chassis_sub = chassis_cmd_topic.subscribe();
}

void chassis_thread_entry(ULONG thread_input)
{
    chassis_init(bsp::can::handle::can1);
    while (1)
    {
        ChassisCommand cmd;
        if (chassis_sub.available() && chassis_sub.read(cmd) == pnxtypes::status::ok)
        {
            chassis.set_chassis_vel(cmd.vx, cmd.vy, cmd.wz);
        }
        // 检查遥控器指令，设置底盘目标速度
        chassis.update(bsp::dwt::timeline_us());
        tx_thread_sleep(1); // 1ms 控制周期
    }
}
