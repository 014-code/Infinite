cmake_minimum_required(VERSION 3.25)
project(InfiniteEngine LANGUAGES CXX)

include(CTest)
include(cmake/EngineExamples.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
find_path(STB_IMAGE_INCLUDE_DIR NAMES stb/stb_image.h REQUIRED)
find_path(STB_TRUETYPE_INCLUDE_DIR NAMES stb/stb_truetype.h REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio/miniaudio.h miniaudio.h REQUIRED)
find_library(MINIAUDIO_LIBRARY NAMES miniaudio REQUIRED)

set(ENGINE_SOURCES
    src/audio/AudioSystem.cpp
    src/audio/internal/AudioValidation.cpp
    src/audio/internal/AudioDeviceBackend.cpp
    src/audio/internal/AudioClipCache.cpp
    src/audio/internal/AudioMixerState.cpp
    src/audio/internal/AudioVoicePool.cpp
    src/audio/MusicPlayer.cpp
    src/audio/AudioEvent.cpp
    src/game/menu/MenuState.cpp
    src/game/menu/MenuStates.cpp
    src/particles/ParticleSystem.cpp
    src/save/SaveGame.cpp
    src/game/state/ApplicationStateStack.cpp
    src/physics/math/Aabb.cpp
    src/physics/math/Ray.cpp
    src/physics/math/Raycast.cpp
    src/physics/math/ClosestPoint.cpp
    src/physics/shapes/CollisionShape.cpp
    src/physics/world/ShapeCollision.cpp
    src/physics/world/PhysicsWorld.cpp
    src/physics/world/PhysicsQueries.cpp
    src/physics/world/PhysicsSolver.cpp
    src/physics/character/CharacterController.cpp
    src/graphics/shaders/SkinShaderSource.cpp
    src/graphics/lighting/DirectionalShadowSettings.cpp
    src/graphics/rendering/shadows/DirectionalShadowMap.cpp
    src/graphics/rendering/shadows/ShadowPassState.cpp
    src/animation/AnimationClip.cpp
    src/animation/AnimationSampler.cpp
    src/animation/AnimationPlayer.cpp
    src/animation/AnimationBlender.cpp
    src/animation/AnimationStateMachine.cpp
    src/animation/Skin.cpp
    src/assets/gltf/GltfSkinDecoder.cpp
    src/scene/SkinBinding.cpp
    src/assets/gltf/GltfAnimationDecoder.cpp
    src/graphics/materials/PbrParameters.cpp
    src/graphics/rendering/HdrPipeline.cpp
    src/resources/PbrResources.cpp
    src/resources/model/ModelTextureUploader.cpp
    src/assets/gltf/GltfPbrDecoder.cpp
    src/resources/PrimitiveResources.cpp
    src/scene/ScenePrimitives.cpp
    src/scene/serialization/PrimitiveSerialization.cpp
    src/scene/serialization/PendingSceneFile.cpp
    src/scene/serialization/SceneFileReader.cpp
    src/scene/serialization/SceneFileWriter.cpp
    src/scene/serialization/SceneBuilder.cpp
    src/graphics/geometry/primitives/PrimitiveMeshBuilder.cpp
    src/assets/GltfLoader.cpp
    src/assets/gltf/GltfFileReader.cpp
    src/assets/gltf/GltfMaterialDecoder.cpp
    src/assets/gltf/GltfMeshDecoder.cpp
    src/assets/gltf/GltfNodeDecoder.cpp
    src/assets/gltf/GltfValidator.cpp
    src/assets/MaterialLoader.cpp
    src/assets/MeshLoader.cpp
    src/core/Application.cpp
    src/core/ApplicationLoop.cpp
    src/core/FrameTiming.cpp
    src/core/Log.cpp
    src/core/Time.cpp
    src/core/ResourcePath.cpp
    src/input/Input.cpp
    src/input/InputState.cpp
    src/input/ActionMap.cpp
    src/math/Transform.cpp
    src/ui/UiElement.cpp
    src/ui/UiButton.cpp
    src/ui/UiCanvas.cpp
    src/ui/font/Utf8.cpp
    src/ui/font/FontFile.cpp
    src/ui/font/FontCollection.cpp
    src/ui/font/BuiltinFont.cpp
    src/ui/font/FontAtlas.cpp
    src/ui/text/TextLayout.cpp
    src/ui/UiRenderer.cpp
    src/scene/GameObject.cpp
    src/scene/components/AreaComponent.cpp
    src/scene/components/CharacterBodyComponent.cpp
    src/scene/components/AudioSourceComponent.cpp
    src/scene/components/PhysicsTransformRules.cpp
    src/scene/components/ScriptComponent.cpp
    src/scene/Scene.cpp
    src/scene/systems/SceneAudioSync.cpp
    src/scene/systems/ScenePhysicsSync.cpp
    src/scene/systems/SceneRenderCollector.cpp
    src/scene/SceneManager.cpp
    src/scene/SceneSerializer.cpp
    src/scene/ModelInstantiator.cpp
    src/scene/components/Renderable.cpp
    src/scene/components/PhysicsBodyComponent.cpp
    src/platform/Window.cpp
    src/resources/ResourceManager.cpp
    src/resources/Model.cpp
    src/graphics/camera/Camera.cpp
    src/graphics/lighting/Lighting.cpp
    src/graphics/lighting/SceneLighting.cpp
    src/graphics/lighting/LightingUniforms.cpp
    src/graphics/geometry/Mesh.cpp
    src/graphics/resources/Material.cpp
    src/graphics/rendering/OpenGLDebug.cpp
    src/graphics/resources/ImageLoader.cpp
    src/graphics/rendering/Renderer.cpp
    src/graphics/rendering/RenderItem.cpp
    src/graphics/resources/Shader.cpp
    src/graphics/resources/Texture.cpp
)

# 框架层只编译可复用的引擎模块，不包含任何具体示例的主程序和资源。
add_library(infinite_engine STATIC ${ENGINE_SOURCES})
target_include_directories(infinite_engine PUBLIC src)
# GLSL保留为可独立阅读的源文件，构建时嵌入引擎，运行时无需复制或查找默认Shader。
set(PRIMITIVE_SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PRIMITIVE_SHADER_DIRECTORY}/primitive.vert" "${PRIMITIVE_SHADER_DIRECTORY}/primitive.frag"
    "${PRIMITIVE_SHADER_DIRECTORY}/scene_lighting.glsl")
file(READ "${PRIMITIVE_SHADER_DIRECTORY}/primitive.vert" PRIMITIVE_VERTEX_SOURCE)
file(READ "${PRIMITIVE_SHADER_DIRECTORY}/primitive.frag" PRIMITIVE_FRAGMENT_SOURCE)
file(READ "${PRIMITIVE_SHADER_DIRECTORY}/scene_lighting.glsl" SCENE_LIGHTING_SOURCE)
string(REPLACE "// @scene_lighting@" "${SCENE_LIGHTING_SOURCE}"
    PRIMITIVE_FRAGMENT_SOURCE "${PRIMITIVE_FRAGMENT_SOURCE}")
configure_file("${PRIMITIVE_SHADER_DIRECTORY}/PrimitiveShaderSources.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PrimitiveShaderSources.h" @ONLY)
target_include_directories(infinite_engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(cmake/PbrShaders.cmake)
# stb只在实现文件中使用，不成为引擎公开头文件的依赖。
target_include_directories(infinite_engine SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
target_include_directories(infinite_engine SYSTEM PRIVATE ${STB_TRUETYPE_INCLUDE_DIR})
target_include_directories(infinite_engine SYSTEM PRIVATE ${MINIAUDIO_INCLUDE_DIR})
target_include_directories(infinite_engine SYSTEM PRIVATE third_party/cgltf)
install(FILES third_party/cgltf/cgltf.h third_party/cgltf/README.md DESTINATION third-party-licenses/cgltf)
target_compile_options(infinite_engine PRIVATE -Wall -Wextra)
target_link_libraries(infinite_engine PUBLIC
    glfw
    GLEW::GLEW
    glm::glm
    OpenGL::GL
    ${MINIAUDIO_LIBRARY}
)

set(RUNTIME_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime" CACHE PATH "Example runtime output root")
get_filename_component(MINGW_BIN_DIRECTORY "${CMAKE_CXX_COMPILER}" DIRECTORY)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/交付运行说明.md" DESTINATION ".")
# 附带本机MSYS2提供的第三方许可文件；这并不替代项目自身的发布授权检查。
if(MINGW)
    foreach(DEPENDENCY IN ITEMS gcc libgcc libstdc++ libwinpthread glfw glew glm stb miniaudio)
        set(LICENSE_DIRECTORY "${MINGW_BIN_DIRECTORY}/../share/licenses/${DEPENDENCY}")
        if(EXISTS "${LICENSE_DIRECTORY}")
            install(DIRECTORY "${LICENSE_DIRECTORY}" DESTINATION "third-party-licenses")
        else()
            message(WARNING "Missing distribution license directory: ${DEPENDENCY}")
        endif()
    endforeach()
endif()

# 配置一个独立示例的可执行文件、资源目录和运行库。
function(configure_engine_example TARGET_NAME EXAMPLE_NAME)
    set(EXAMPLE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/${EXAMPLE_NAME}")
    set(EXAMPLE_RUNTIME_DIRECTORY "${RUNTIME_DIRECTORY}/examples/${EXAMPLE_NAME}")

    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${EXAMPLE_RUNTIME_DIRECTORY}"
        OUTPUT_NAME "${EXAMPLE_NAME}"
    )
    # 交付目录只安装当前配置的exe与所需资源，不复制开发目录的日志或旧文件。
    install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION "examples/${EXAMPLE_NAME}")

    # 资源作为独立构建依赖：只改Shader或图片也会同步，不必修改cpp触发重新链接。
    # CONFIGURE_DEPENDS同时跟踪新增加的资源；只复制到这个示例对应的运行目录。
    file(GLOB_RECURSE EXAMPLE_RESOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
        RELATIVE "${EXAMPLE_DIRECTORY}"
        "${EXAMPLE_DIRECTORY}/shaders/*"
        "${EXAMPLE_DIRECTORY}/assets/*"
        "${EXAMPLE_DIRECTORY}/materials/*"
        "${EXAMPLE_DIRECTORY}/scenes/*"
        "${EXAMPLE_DIRECTORY}/textures/*"
    )
    set(EXAMPLE_RESOURCE_OUTPUTS)
    foreach(RESOURCE IN LISTS EXAMPLE_RESOURCES)
        get_filename_component(RESOURCE_SUBDIRECTORY "${RESOURCE}" DIRECTORY)
        install(FILES "${EXAMPLE_DIRECTORY}/${RESOURCE}"
            DESTINATION "examples/${EXAMPLE_NAME}/${RESOURCE_SUBDIRECTORY}")
        set(RESOURCE_OUTPUT "${EXAMPLE_RUNTIME_DIRECTORY}/${RESOURCE}")
        get_filename_component(RESOURCE_OUTPUT_DIRECTORY "${RESOURCE_OUTPUT}" DIRECTORY)
        add_custom_command(OUTPUT "${RESOURCE_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUTPUT_DIRECTORY}"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${EXAMPLE_DIRECTORY}/${RESOURCE}" "${RESOURCE_OUTPUT}"
            DEPENDS "${EXAMPLE_DIRECTORY}/${RESOURCE}"
            COMMENT "Sync ${EXAMPLE_NAME}/${RESOURCE}"
            VERBATIM
        )
        list(APPEND EXAMPLE_RESOURCE_OUTPUTS "${RESOURCE_OUTPUT}")
    endforeach()
    add_custom_target(${TARGET_NAME}_resources DEPENDS ${EXAMPLE_RESOURCE_OUTPUTS})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_resources)

    # 每个示例的exe旁边都放一份运行库，双击示例时不需要手动设置PATH。
    foreach(RUNTIME_LIBRARY IN ITEMS
        glfw3.dll
        glew32.dll
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
    )
        if(EXISTS "${MINGW_BIN_DIRECTORY}/${RUNTIME_LIBRARY}")
            install(FILES "${MINGW_BIN_DIRECTORY}/${RUNTIME_LIBRARY}"
                DESTINATION "examples/${EXAMPLE_NAME}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MINGW_BIN_DIRECTORY}/${RUNTIME_LIBRARY}"
                    "${EXAMPLE_RUNTIME_DIRECTORY}/${RUNTIME_LIBRARY}"
            )
        endif()
    endforeach()
endfunction()

# 示例应用层：每个示例都可以独立运行和演示一个功能。
add_executable(infinite_example_color_triangle
    examples/color_triangle/main.cpp
)
target_compile_options(infinite_example_color_triangle PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_color_triangle PRIVATE infinite_engine)
configure_engine_example(infinite_example_color_triangle color_triangle)

add_executable(infinite_example_textured_quad
    examples/textured_quad/main.cpp
)
target_compile_options(infinite_example_textured_quad PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_textured_quad PRIVATE infinite_engine)
configure_engine_example(infinite_example_textured_quad textured_quad)

add_executable(infinite_example_textured_cube
    examples/textured_cube/main.cpp
)
target_compile_options(infinite_example_textured_cube PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_textured_cube PRIVATE infinite_engine)
configure_engine_example(infinite_example_textured_cube textured_cube)

add_executable(infinite_example_diffuse_lighting
    examples/diffuse_lighting/main.cpp
)
target_include_directories(infinite_example_diffuse_lighting PRIVATE examples)
target_compile_options(infinite_example_diffuse_lighting PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_diffuse_lighting PRIVATE infinite_engine)
configure_engine_example(infinite_example_diffuse_lighting diffuse_lighting)

add_executable(infinite_example_player_controller
    examples/player_controller/main.cpp
    examples/player_controller/FlatGroundMovement.cpp
)
target_include_directories(infinite_example_player_controller PRIVATE examples)
target_compile_options(infinite_example_player_controller PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_player_controller PRIVATE infinite_engine)
configure_engine_example(infinite_example_player_controller player_controller)

add_executable(infinite_example_alpha_blending
    examples/alpha_blending/main.cpp
)
target_include_directories(infinite_example_alpha_blending PRIVATE examples)
target_compile_options(infinite_example_alpha_blending PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_alpha_blending PRIVATE infinite_engine)
configure_engine_example(infinite_example_alpha_blending alpha_blending)

add_executable(infinite_example_scene_objects examples/scene_objects/main.cpp)
target_include_directories(infinite_example_scene_objects PRIVATE examples)
target_compile_options(infinite_example_scene_objects PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_scene_objects PRIVATE infinite_engine)
configure_engine_example(infinite_example_scene_objects scene_objects)

# glTF预览示例：展示多节点、多primitive、基础颜色纹理和多个独立实例。
add_executable(infinite_example_gltf_model examples/gltf_model/main.cpp)
target_compile_options(infinite_example_gltf_model PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_gltf_model PRIVATE infinite_engine)
configure_engine_example(infinite_example_gltf_model gltf_model)

# 文件资源与场景加载的完整示例，具体几何、运动和资源全部留在应用层。
add_executable(infinite_example_asset_scene examples/asset_scene/main.cpp)
target_compile_options(infinite_example_asset_scene PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_asset_scene PRIVATE infinite_engine)
configure_engine_example(infinite_example_asset_scene asset_scene)

# 空白模板也是可执行目标，确保文档推荐的应用入口确实可以编译运行。
add_executable(infinite_example_application_template examples/application_template/main.cpp)
target_compile_options(infinite_example_application_template PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_application_template PRIVATE infinite_engine)
configure_engine_example(infinite_example_application_template application_template)

add_executable(infinite_example_ui_demo examples/ui_demo/main.cpp)
target_include_directories(infinite_example_ui_demo PRIVATE examples)
target_compile_options(infinite_example_ui_demo PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_ui_demo PRIVATE infinite_engine)
configure_engine_example(infinite_example_ui_demo ui_demo)

add_executable(infinite_example_menu_demo examples/menu_demo/main.cpp)
target_include_directories(infinite_example_menu_demo PRIVATE examples)
target_compile_options(infinite_example_menu_demo PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_menu_demo PRIVATE infinite_engine)
configure_engine_example(infinite_example_menu_demo menu_demo)

# 游戏流程示例：组合状态栈、SceneManager、Area事件和SaveGame，不把具体规则加入引擎层。
add_executable(infinite_example_game_flow
    examples/game_flow/main.cpp
    examples/game_flow/GameFlow.cpp
)
target_include_directories(infinite_example_game_flow PRIVATE examples)
target_compile_options(infinite_example_game_flow PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_game_flow PRIVATE infinite_engine)
configure_engine_example(infinite_example_game_flow game_flow)

# 材质展示示例：真实CC0 glTF模型、方向光和模型资源全部属于应用层示例。
add_executable(infinite_example_material_showcase
    examples/material_showcase/main.cpp
    examples/material_showcase/ShowcaseScene.cpp
)
target_include_directories(infinite_example_material_showcase PRIVATE examples)
target_compile_options(infinite_example_material_showcase PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_material_showcase PRIVATE infinite_engine)
configure_engine_example(infinite_example_material_showcase material_showcase)

add_executable(infinite_example_primitive_shapes
    examples/primitive_shapes/main.cpp
    examples/primitive_shapes/PrimitiveScene.cpp
)
target_compile_options(infinite_example_primitive_shapes PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_primitive_shapes PRIVATE infinite_engine)
configure_engine_example(infinite_example_primitive_shapes primitive_shapes)

add_executable(infinite_example_scene_lighting
    examples/scene_lighting/main.cpp
    examples/scene_lighting/SceneLightingScene.cpp
)
target_include_directories(infinite_example_scene_lighting PRIVATE examples)
target_compile_options(infinite_example_scene_lighting PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_scene_lighting PRIVATE infinite_engine)
configure_engine_example(infinite_example_scene_lighting scene_lighting)

add_executable(infinite_example_pbr_materials examples/pbr_materials/main.cpp examples/pbr_materials/PbrScene.cpp)
target_include_directories(infinite_example_pbr_materials PRIVATE examples)
target_compile_options(infinite_example_pbr_materials PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_pbr_materials PRIVATE infinite_engine)
configure_engine_example(infinite_example_pbr_materials pbr_materials)

add_executable(infinite_example_gltf_pbr examples/gltf_pbr/main.cpp)
target_include_directories(infinite_example_gltf_pbr PRIVATE examples)
target_compile_options(infinite_example_gltf_pbr PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_gltf_pbr PRIVATE infinite_engine)
configure_engine_example(infinite_example_gltf_pbr gltf_pbr)

add_executable(infinite_example_node_animation examples/node_animation/main.cpp)
target_include_directories(infinite_example_node_animation PRIVATE examples)
target_compile_options(infinite_example_node_animation PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_node_animation PRIVATE infinite_engine)
configure_engine_example(infinite_example_node_animation node_animation)

add_executable(infinite_example_skeletal_animation examples/skeletal_animation/main.cpp)
target_include_directories(infinite_example_skeletal_animation PRIVATE examples)
target_compile_options(infinite_example_skeletal_animation PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_skeletal_animation PRIVATE infinite_engine)
configure_engine_example(infinite_example_skeletal_animation skeletal_animation)

add_executable(infinite_example_directional_shadow examples/directional_shadow/main.cpp)
target_include_directories(infinite_example_directional_shadow PRIVATE examples)
target_compile_options(infinite_example_directional_shadow PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_directional_shadow PRIVATE infinite_engine)
configure_engine_example(infinite_example_directional_shadow directional_shadow)

# 物理示例：静态地形、角色控制器、动态刚体、射线放置与碰撞体调试可视化。
add_executable(infinite_example_physics_demo examples/physics_demo/main.cpp)
target_include_directories(infinite_example_physics_demo PRIVATE examples)
target_compile_options(infinite_example_physics_demo PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_physics_demo PRIVATE infinite_engine)
configure_engine_example(infinite_example_physics_demo physics_demo)

# 小游戏示例：使用GameObject组件、CharacterBody和静态碰撞体完成一个可运行的收集玩法。
# 模型、Shader和胜利规则全部属于应用层，不把具体游戏规则加入引擎库。
add_executable(infinite_example_mini_game
    examples/mini_game/main.cpp
    examples/mini_game/MiniGameScene.cpp
)
target_include_directories(infinite_example_mini_game PRIVATE examples)
target_compile_options(infinite_example_mini_game PRIVATE -Wall -Wextra)
target_link_libraries(infinite_example_mini_game PRIVATE infinite_engine)
configure_engine_example(infinite_example_mini_game mini_game)

if(BUILD_TESTING)
    include(cmake/EngineTests.cmake)
endif()
