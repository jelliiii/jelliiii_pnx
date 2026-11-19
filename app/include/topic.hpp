#include "msg.hpp"

struct ChassisCommand
{
    float vx;
    float vy;
    float wz;
};

inline msg::topic<ChassisCommand> chassis_cmd_topic{};
