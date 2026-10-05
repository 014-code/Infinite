#version 330 core

// 故意保留语法错误，验证编译失败时异常包含驱动日志。
void main()
{
    gl_Position = this_is_not_valid_shader_code;
}
