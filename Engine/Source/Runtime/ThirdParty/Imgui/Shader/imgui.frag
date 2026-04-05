#version 450

layout(location = 0) out vec4 fColor;

layout(location = 0) in vec4 InColor;
layout(location = 1) in vec2 InUV;

layout(binding = 0) uniform texture2D sTexture;
layout(binding = 1) uniform sampler samp;

void main() {
    fColor = InColor * texture(sampler2D(sTexture, samp), InUV.st);
}
