#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;

layout(std140, binding = 0) uniform UniformBuffer {
    mat4 uMVP;
};

void main() {
    gl_Position = uMVP * vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y;  // Vulkan Y flip
    outWorldPos = inPosition;
    outUV = inUV;
}
