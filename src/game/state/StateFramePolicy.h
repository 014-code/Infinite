#pragma once

// 当前应用状态对Application主循环各阶段的控制策略。
// 状态本身仍然可以接收事件和更新回调；这些开关只控制共享的Scene、物理和渲染服务。
struct StateFramePolicy
{
    // false时不调用Scene::update，适合暂停菜单仍显示原场景的情况。
    bool updateScene = true;
    // false时跳过PhysicsWorld::step，并清掉未完成的固定步累加时间。
    bool simulatePhysics = true;
    // false时只清屏，不提交Scene的RenderItem；UI层仍可在自己的阶段绘制。
    bool renderScene = true;
    // false时不推进AudioSystem的Voice回收和流式状态；是否暂停声音由状态生命周期决定。
    bool updateAudio = true;
};
