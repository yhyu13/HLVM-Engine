#version 450

layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec2 OutUV;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform PushConstants {
    vec2 uScale;
    vec2 uTranslate;
} pc;

void main() {
    OutColor = aColor;
    OutUV = aUV;
    vec2 pos = aPos * pc.uScale + pc.uTranslate;
    gl_Position = vec4(pos, 0.0, 1.0);
}
