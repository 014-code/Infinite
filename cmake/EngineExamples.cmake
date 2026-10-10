# 所有可交付示例的唯一清单。
#
# CMake用它登记示例Smoke Test，Release验收脚本也读取同一份清单。
# 新增示例时必须同时在根CMakeLists.txt创建目标并把名称加入这里，
# 这样“能编译”与“被交付验收”不会再次出现遗漏。
set(INFINITE_ENGINE_EXAMPLES
    color_triangle
    textured_quad
    textured_cube
    diffuse_lighting
    player_controller
    alpha_blending
    scene_objects
    asset_scene
    gltf_model
    application_template
    ui_demo
    menu_demo
    game_flow
    material_showcase
    primitive_shapes
    scene_lighting
    pbr_materials
    gltf_pbr
    node_animation
    skeletal_animation
    directional_shadow
    physics_demo
    mini_game
)
