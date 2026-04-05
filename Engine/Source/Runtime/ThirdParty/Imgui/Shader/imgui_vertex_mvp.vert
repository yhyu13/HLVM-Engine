#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform PushConstants {
    float mvp[4][4];
} pc;

layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec2 OutUV;

void main() {
    mat4 M;
    M[0] = vec4(pc.mvp[0][0], pc.mvp[0][1], pc.mvp[0][2], pc.mvp[0][3]);
    M[1] = vec4(pc.mvp[1][0], pc.mvp[1][1], pc.mvp[1][2], pc.mvp[1][3]);
    M[2] = vec4(pc.mvp[2][0], pc.mvp[2][1], pc.mvp[2][2], pc.mvp[2][3]);
    M[3] = vec4(pc.mvp[3][0], pc.mvp[3][1], pc.mvp[3][2], pc.mvp[3][3]);
    gl_Position = M * vec4(aPos, 0.0, 1.0);
    OutColor = aColor;
    OutUV = aUV;
}