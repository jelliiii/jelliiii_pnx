
#include "bsp_can.hpp"
#include "shoot_motor.hpp"

extern "C" void app_start()
{

    shoot_task_start();
}
