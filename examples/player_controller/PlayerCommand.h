#pragma once

#include <glm/vec2.hpp>

namespace PlayerExample
{
    // 输入适配与移动演示之间的数据约定，不携带按键、窗口或GameObject指针。
    // 当前示例采用世界坐标：x向右(+X)、y向后(+Z)，向前为(0,-1)。
    // 以后若演示摄像机相对移动，应在提交指令前转换方向，不让移动规则读取摄像机。
    struct PlayerCommand
    {
        glm::vec2 movement{0.0f};
        bool jumpPressed = false; // 本次更新请求起跳；不是持续按住状态。
        bool sprint = false;
    };
}
