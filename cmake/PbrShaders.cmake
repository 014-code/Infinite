# PBR着色器由独立GLSL文件组成，构建时嵌入，不依赖运行目录中的示例资源。
set(PBR_SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PBR_SHADER_DIRECTORY}/pbr.vert" "${PBR_SHADER_DIRECTORY}/pbr.frag"
    "${PBR_SHADER_DIRECTORY}/pbr_brdf.glsl" "${PBR_SHADER_DIRECTORY}/pbr_normal.glsl"
    "${PBR_SHADER_DIRECTORY}/output.vert" "${PBR_SHADER_DIRECTORY}/output.frag"
    "${PBR_SHADER_DIRECTORY}/skinning.glsl" "${PBR_SHADER_DIRECTORY}/shadow.vert"
    "${PBR_SHADER_DIRECTORY}/shadow.frag" "${PBR_SHADER_DIRECTORY}/directional_shadow.glsl")
file(READ "${PBR_SHADER_DIRECTORY}/pbr.vert" PBR_VERTEX_SOURCE)
file(READ "${PBR_SHADER_DIRECTORY}/skinning.glsl" SKINNING_SOURCE)
string(REPLACE "// @skinning@" "${SKINNING_SOURCE}" PBR_VERTEX_SOURCE "${PBR_VERTEX_SOURCE}")
file(READ "${PBR_SHADER_DIRECTORY}/pbr.frag" PBR_FRAGMENT_SOURCE)
foreach(MODULE IN ITEMS scene_lighting pbr_brdf pbr_normal directional_shadow)
    file(READ "${PBR_SHADER_DIRECTORY}/${MODULE}.glsl" MODULE_SOURCE)
    string(REPLACE "// @${MODULE}@" "${MODULE_SOURCE}" PBR_FRAGMENT_SOURCE "${PBR_FRAGMENT_SOURCE}")
endforeach()
file(READ "${PBR_SHADER_DIRECTORY}/output.vert" OUTPUT_VERTEX_SOURCE)
file(READ "${PBR_SHADER_DIRECTORY}/output.frag" OUTPUT_FRAGMENT_SOURCE)
file(READ "${PBR_SHADER_DIRECTORY}/shadow.vert" SHADOW_VERTEX_SOURCE)
string(REPLACE "// @skinning@" "${SKINNING_SOURCE}" SHADOW_VERTEX_SOURCE "${SHADOW_VERTEX_SOURCE}")
file(READ "${PBR_SHADER_DIRECTORY}/shadow.frag" SHADOW_FRAGMENT_SOURCE)
configure_file("${PBR_SHADER_DIRECTORY}/PbrShaderSources.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PbrShaderSources.h" @ONLY)
