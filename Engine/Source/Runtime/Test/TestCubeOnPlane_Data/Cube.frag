#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(std140, binding = 1) uniform LightBuffer {
    vec3 uLightPosition;
    vec3 uLightColor;
};

layout(std140, binding = 2) uniform ObjectBuffer {
    vec3 uObjectColor;
};

void main() {
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * uLightColor;
    
    // Diffuse
    vec3 lightDir = normalize(uLightPosition - inWorldPos);
    float diff = max(dot(inNormal, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(-inWorldPos);
    vec3 reflectDir = reflect(-lightDir, inNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * uLightColor;
    
    // Final color
    vec3 result = (ambient + diffuse + specular) * uObjectColor;
    outColor = vec4(result, 1.0);
}
