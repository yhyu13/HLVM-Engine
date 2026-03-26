#version 450
layout(binding = 4) uniform LightUniform {
    vec3 lightPos;
} light;
layout(binding = 5) uniform CameraUniform {
    vec3 cameraPos;
} camera;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Use constant white albedo (texture fallback)
    vec3 albedo = vec3(1.0);
    float metallic = 0.0;
    float roughness = 1.0;

    vec3 N = normalize(inNormal);
    vec3 V = normalize(camera.cameraPos - inWorldPos);
    vec3 L = normalize(light.lightPos - inWorldPos);
    vec3 H = normalize(V + L);

    // Diffuse (Lambert)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo * (1.0 - metallic);

    // Specular (Blinn-Phong)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * F0;

    // Ambient term
    vec3 ambient = 0.1 * albedo;

    outColor = vec4(ambient + diffuse + specular, 1.0);
}
