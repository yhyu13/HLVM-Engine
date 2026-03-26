#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Procedural grid pattern
    vec2 uv = inUV * 10.0;  // 10x10 grid
    vec2 grid = fract(uv) - 0.5;
    
    // Line detection
    float isLine = step(0.95, abs(grid.x)) + step(0.95, abs(grid.y));
    
    // Colors
    vec3 baseColor = vec3(0.5, 0.5, 0.5);      // Gray
    vec3 lineColor = vec3(0.3, 0.3, 0.3);      // Dark gray
    
    // Mix base and line colors
    vec3 finalColor = mix(baseColor, lineColor, isLine);
    
    outColor = vec4(finalColor, 1.0);
}
