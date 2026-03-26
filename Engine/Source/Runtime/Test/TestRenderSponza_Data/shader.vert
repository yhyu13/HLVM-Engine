#version 450
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    // Transform normal with inverse-transpose of model matrix
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    outNormal = normalMatrix * inNormal;
    outUV = inUV;
    gl_Position = ubo.proj * ubo.view * worldPos;
}