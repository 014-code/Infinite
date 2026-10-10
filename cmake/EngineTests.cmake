# 本文件由根CMakeLists include，路径仍以项目根目录为基准。
# 只收拢测试登记，不改变引擎、示例和交付资源的依赖边界。
function(configure_engine_test TARGET_NAME)
    # 大多数测试名与目标名相同。仅保留确有使用场景的覆盖项，额外源码/包含目录仍显式配置。
    cmake_parse_arguments(TEST "" "NAME;WORKING_DIRECTORY" "ARGS" ${ARGN})
    if(TEST_UNPARSED_ARGUMENTS OR TEST_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid configure_engine_test arguments for ${TARGET_NAME}")
    endif()
    if(NOT TEST_NAME)
        set(TEST_NAME "${TARGET_NAME}")
    endif()
    if(NOT TEST_WORKING_DIRECTORY)
        set(TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
    target_link_libraries(${TARGET_NAME} PRIVATE infinite_engine)
    add_test(NAME ${TEST_NAME} COMMAND ${TARGET_NAME} ${TEST_ARGS})
    set_tests_properties(${TEST_NAME} PROPERTIES
        WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MINGW_BIN_DIRECTORY}"
        TIMEOUT 30)
endfunction()

add_executable(infinite_shader_uniform_test tests/shader_uniform_test.cpp)
configure_engine_test(infinite_shader_uniform_test)
add_executable(infinite_pbr_test tests/pbr_test.cpp)
configure_engine_test(infinite_pbr_test)
add_executable(infinite_gltf_pbr_test tests/gltf_pbr_test.cpp)
configure_engine_test(infinite_gltf_pbr_test)
add_executable(infinite_animation_sampler_test tests/animation_sampler_test.cpp)
configure_engine_test(infinite_animation_sampler_test)
add_executable(infinite_animation_player_test tests/animation_player_test.cpp)
configure_engine_test(infinite_animation_player_test)
add_executable(infinite_skin_test tests/skin_test.cpp)
configure_engine_test(infinite_skin_test)
add_executable(infinite_directional_shadow_test tests/directional_shadow_test.cpp)
configure_engine_test(infinite_directional_shadow_test)
add_executable(infinite_render_preparation_test tests/render_preparation_test.cpp)
configure_engine_test(infinite_render_preparation_test)
add_executable(infinite_scene_file_test tests/scene_file_test.cpp)
configure_engine_test(infinite_scene_file_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/scene-files")

# 截图测试复用真实示例布局，避免测试与用户运行的示例逐渐偏离。
add_executable(infinite_scene_lighting_example_test tests/scene_lighting_example_test.cpp
    examples/scene_lighting/SceneLightingScene.cpp)
target_include_directories(infinite_scene_lighting_example_test PRIVATE examples)
target_include_directories(infinite_scene_lighting_example_test SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
configure_engine_test(infinite_scene_lighting_example_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/scene-lighting")

foreach(PRIMITIVE_TEST IN ITEMS render serialization)
    add_executable(infinite_primitive_${PRIMITIVE_TEST}_test tests/primitive_${PRIMITIVE_TEST}_test.cpp)
    if(PRIMITIVE_TEST STREQUAL "render")
        target_sources(infinite_primitive_render_test PRIVATE examples/primitive_shapes/PrimitiveScene.cpp)
        target_include_directories(infinite_primitive_render_test PRIVATE examples)
        target_include_directories(infinite_primitive_render_test SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
    endif()
    configure_engine_test(infinite_primitive_${PRIMITIVE_TEST}_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/primitives")
endforeach()

add_executable(infinite_showcase_render_test tests/showcase_render_test.cpp examples/material_showcase/ShowcaseScene.cpp)
target_include_directories(infinite_showcase_render_test PRIVATE examples)
target_include_directories(infinite_showcase_render_test SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
configure_engine_test(infinite_showcase_render_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/showcase")
add_executable(infinite_gltf_render_test tests/gltf_render_test.cpp)
configure_engine_test(infinite_gltf_render_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/gltf")
foreach(MODULE IN ITEMS model_instance gltf_texture gltf_loader)
    add_executable(infinite_${MODULE}_test tests/${MODULE}_test.cpp)
    configure_engine_test(infinite_${MODULE}_test)
endforeach()

# 实际启动全部示例及空白模板，隐藏窗口绘制三帧并检查像素。
# 使用已有示例目标，不调用测试helper重新配置示例；运行目录特意不同于资源源码目录。
foreach(EXAMPLE_NAME IN LISTS INFINITE_ENGINE_EXAMPLES)
    add_test(NAME infinite_example_${EXAMPLE_NAME}_smoke
        COMMAND $<TARGET_FILE:infinite_example_${EXAMPLE_NAME}> --smoke-test)
    set_tests_properties(infinite_example_${EXAMPLE_NAME}_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MINGW_BIN_DIRECTORY}"
        TIMEOUT 30)
endforeach()

add_executable(infinite_mesh_material_test tests/mesh_material_test.cpp)
configure_engine_test(infinite_mesh_material_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/resources")
add_executable(infinite_resource_manager_test tests/resource_manager_test.cpp)
configure_engine_test(infinite_resource_manager_test)

foreach(MODULE_TEST IN ITEMS scene_lighting_render_test primitive_scene_test primitive_mesh_test lighting_test
    player_controller_test transform_test scene_test scene_render_test scene_update_test input_state_test
    camera_test camera_controller_test input_action_test window_test frame_timing_test application_test
    application_state_test ui_test scene_manager_test area_event_test particle_system_test save_game_test)
    add_executable(infinite_${MODULE_TEST} tests/${MODULE_TEST}.cpp)
    # 平地移动属于示例，只有对应示例和测试编译它，引擎库不反向依赖examples。
    if(MODULE_TEST STREQUAL "player_controller_test")
        target_sources(infinite_${MODULE_TEST} PRIVATE examples/player_controller/FlatGroundMovement.cpp)
    endif()
    target_include_directories(infinite_${MODULE_TEST} PRIVATE examples)
    configure_engine_test(infinite_${MODULE_TEST})
endforeach()

# 音频核心支持Null Backend，因此可以在没有声卡或音频设备的环境中纯CPU测试。
add_executable(infinite_audio_test tests/audio_test.cpp)
configure_engine_test(infinite_audio_test)
add_executable(infinite_ui_font_test tests/ui_font_test.cpp)
configure_engine_test(infinite_ui_font_test)

add_executable(infinite_mesh_loader_test tests/mesh_loader_test.cpp)
configure_engine_test(infinite_mesh_loader_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/meshes")
add_executable(infinite_material_loader_test tests/material_loader_test.cpp)
configure_engine_test(infinite_material_loader_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/materials")
add_executable(infinite_scene_serialization_test tests/scene_serialization_test.cpp)
configure_engine_test(infinite_scene_serialization_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/scenes")
# 场景文件资源引用需要隐藏GL上下文，与纯CPU的序列化测试分别登记。
add_executable(infinite_scene_resource_test tests/scene_resource_test.cpp)
configure_engine_test(infinite_scene_resource_test ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/scenes")
add_executable(infinite_log_test tests/log_test.cpp)
configure_engine_test(infinite_log_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/logging")

# 物理基础：形状/射线/AABB数学全部为无窗口纯CPU测试，不创建OpenGL上下文。
foreach(PHYSICS_TEST IN ITEMS physics_shapes physics_math physics_raycast)
    add_executable(infinite_${PHYSICS_TEST}_test tests/${PHYSICS_TEST}_test.cpp)
    configure_engine_test(infinite_${PHYSICS_TEST}_test)
endforeach()
# 碰撞世界与narrowphase同样不依赖窗口；场景侧组件集成也在无窗口下验证。
foreach(PHYSICS_WORLD_TEST IN ITEMS physics_collision physics_world physics_body_component)
    add_executable(infinite_${PHYSICS_WORLD_TEST}_test tests/${PHYSICS_WORLD_TEST}_test.cpp)
    configure_engine_test(infinite_${PHYSICS_WORLD_TEST}_test)
endforeach()
# 角色控制器与刚体动力学：纯CPU模拟，不创建窗口。
foreach(PHYSICS_SIMULATION_TEST IN ITEMS physics_character physics_rigidbody)
    add_executable(infinite_${PHYSICS_SIMULATION_TEST}_test tests/${PHYSICS_SIMULATION_TEST}_test.cpp)
    configure_engine_test(infinite_${PHYSICS_SIMULATION_TEST}_test)
endforeach()

# 示例层的摄像机相对输入：纯方向数学，不需要窗口；防止出现左右颠倒之类的回归。
add_executable(infinite_camera_relative_input_test tests/camera_relative_input_test.cpp)
target_include_directories(infinite_camera_relative_input_test PRIVATE examples)
configure_engine_test(infinite_camera_relative_input_test)

# 同时覆盖启用/禁用诊断宏；显式checkErrors在两种编译方式中都可用。
foreach(DIAGNOSTIC_MODE IN ITEMS debug release)
    set(DIAGNOSTIC_TARGET "infinite_opengl_${DIAGNOSTIC_MODE}_test")
    add_executable(${DIAGNOSTIC_TARGET} tests/opengl_debug_test.cpp)
    if(DIAGNOSTIC_MODE STREQUAL "release")
        target_compile_definitions(${DIAGNOSTIC_TARGET} PRIVATE NDEBUG)
    else()
        target_compile_options(${DIAGNOSTIC_TARGET} PRIVATE -UNDEBUG)
    endif()
    configure_engine_test(${DIAGNOSTIC_TARGET} ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/logging/${DIAGNOSTIC_MODE}.log")
endforeach()

foreach(MODULE IN ITEMS alpha_blending culling_smoke)
    add_executable(infinite_${MODULE}_test tests/${MODULE}_test.cpp)
    target_include_directories(infinite_${MODULE}_test PRIVATE examples)
    configure_engine_test(infinite_${MODULE}_test)
endforeach()
# 保留历史CTest名称，避免已有CI或本机筛选命令失效。
add_executable(infinite_smoke_test tests/smoke_test.cpp)
configure_engine_test(infinite_smoke_test NAME infinite_open_gl_smoke_test)
add_executable(infinite_depth_smoke_test tests/depth_smoke_test.cpp)
configure_engine_test(infinite_depth_smoke_test)

# 图片编码仅生成测试输入；即使只筛选上传测试，CTest也会先执行图片夹具。
add_executable(infinite_image_loader_test tests/image_loader_test.cpp)
target_include_directories(infinite_image_loader_test SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
configure_engine_test(infinite_image_loader_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/images")
set_tests_properties(infinite_image_loader_test PROPERTIES FIXTURES_SETUP image_fixtures)
add_executable(infinite_texture_upload_test tests/texture_upload_test.cpp)
configure_engine_test(infinite_texture_upload_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ARGS "${CMAKE_CURRENT_BINARY_DIR}/tests/images/rgba.png")
set_tests_properties(infinite_texture_upload_test PROPERTIES FIXTURES_REQUIRED image_fixtures)
add_executable(infinite_diffuse_lighting_test tests/diffuse_lighting_test.cpp)
configure_engine_test(infinite_diffuse_lighting_test)

# 缺少图形上下文的环境按cpu标签筛选；完整CTest仍运行所有测试。
get_property(ENGINE_TESTS DIRECTORY PROPERTY TESTS)
set_tests_properties(${ENGINE_TESTS} PROPERTIES LABELS "graphics")
set_tests_properties(
    infinite_primitive_mesh_test infinite_scene_file_test infinite_lighting_test infinite_player_controller_test
    infinite_gltf_loader_test infinite_material_loader_test infinite_transform_test infinite_scene_test
    infinite_scene_update_test infinite_input_state_test infinite_camera_test infinite_camera_controller_test
    infinite_input_action_test infinite_mesh_loader_test infinite_log_test infinite_image_loader_test
    infinite_frame_timing_test infinite_scene_serialization_test infinite_animation_sampler_test
    infinite_physics_shapes_test infinite_physics_math_test infinite_physics_raycast_test
    infinite_physics_collision_test infinite_physics_world_test infinite_physics_body_component_test
    infinite_physics_character_test infinite_physics_rigidbody_test
    infinite_camera_relative_input_test
    infinite_audio_test infinite_ui_test infinite_scene_manager_test infinite_area_event_test infinite_particle_system_test infinite_save_game_test
    infinite_ui_font_test
    PROPERTIES LABELS "cpu")
