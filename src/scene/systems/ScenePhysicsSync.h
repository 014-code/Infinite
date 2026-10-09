#pragma once

class PhysicsWorld;
class Scene;

// ScenePhysicsSync集中维护Scene与PhysicsWorld之间的同步方向。
//
// 这些函数只借用Scene和PhysicsWorld，不拥有其中任何对象。静态物体由
// GameObject::Transform驱动物理世界，动态物体和角色则由物理结果回写Transform。
// 把规则放在单独模块中，可以避免Scene对象管理代码夹杂固定步同步细节。
class ScenePhysicsSync final
{
public:
    // 在每个固定子步开始前，把Scene中属于指定世界的静态刚体同步到物理世界。
    static void syncStatic(const Scene &scene, PhysicsWorld &world);
    // 固定子步全部完成后，用插值位姿更新动态刚体，供本帧渲染使用。
    static void syncDynamic(const Scene &scene, PhysicsWorld &world,
        float interpolationAlpha);
    // 将角色控制器的当前位置写回对应GameObject；角色朝向仍由应用层管理。
    static void syncCharacters(const Scene &scene);
    // 更新Area的重叠集合并派发进入/离开事件。
    static void updateAreas(const Scene &scene);
};
