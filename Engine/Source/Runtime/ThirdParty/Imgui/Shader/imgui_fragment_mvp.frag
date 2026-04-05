#version 450

layout(location = 0) in vec4 InColor;
layout(location = 1) in vec2 InUV;

layout(location = 0) out vec4 fColor;

// Combined image sampler - sampler needs to be at binding 128 for NVRHI's VulkanBindingOffsets
layout(binding = 0) uniform texture2D sTexture;
layout(binding = 128) uniform sampler samp;

void main() {
    fColor = InColor * texture(sampler2D(sTexture, samp), InUV.st);
}
