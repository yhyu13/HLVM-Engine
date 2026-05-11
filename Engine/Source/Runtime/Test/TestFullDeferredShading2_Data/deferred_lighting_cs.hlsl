// Deferred Lighting Compute Shader - TestFullDeferredShading2
// Reads GBuffer textures, computes lighting, writes to UAV

// Lighting constants - b0 with bRegShift 256 → SPIR-V binding 256
cbuffer LightingConstants : register(b0)
{
    float3 LightDir;       // Directional light direction (normalized)
    float LightIntensity;
    float3 CameraPos;      // Camera position for view direction
    float ShadowHardness;
    float3 AmbientColor;   // Ambient light color
    float Pad;
};

// GBuffer textures - tRegShift 0, registers directly map to SPIR-V bindings
// t0 = Position (world position)
Texture2D<float4> t_Position : register(t0);
// t1 = Albedo (RGB) + Specular (A)
Texture2D<float4> t_Albedo : register(t1);
// t2 = Normal (packed XYZ) + Roughness (A)
Texture2D<float4> t_Normal : register(t2);
// t3 = Emissive (RGB) + Depth (A) - depth packed in alpha
Texture2D<float4> t_DepthEmissive : register(t3);

// Output UAV - binding u0 with uRegShift 384 → SPIR-V binding 384
RWTexture2D<float4> u_HDROutput : register(u0);

// Checkerboard pattern to verify shader is running
bool checkerboard(uint2 coord)
{
    return ((coord.x / 32) + (coord.y / 32)) % 2 == 0;
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    // Sample GBuffer - Albedo (RGB) + Specular (A)
    float4 albedoData = t_Albedo[pixelCoord];
    float3 albedo = albedoData.rgb;
    float specular = albedoData.a;

    // Sample normal from GBuffer - packed to [0,1], unpack to [-1,1]
    float4 normalData = t_Normal[pixelCoord];
    float3 normal = normalData.xyz * 2.0 - 1.0;

    // Background detection: if albedo is black (no geometry), show background
    if (length(albedo) < 0.001)
    {
        // Show gradient background to verify display works
        float gradient = float(pixelCoord.x) / 800.0;  // Gradient from left to right
        u_HDROutput[pixelCoord] = float4(gradient, 0.0, 1.0 - gradient, 1.0);  // Cyan to magenta gradient
        //u_HDROutput[pixelCoord] = float4(0.02, 0.02, 0.05, 1.0);  // Dark background
        return;
    }

    // Output ALBEDO directly - if cube is rendered, we should see red cube
    // This bypasses lighting to test if GBuffer and cube are working
    //u_HDROutput[pixelCoord] = float4(albedo, 1.0)

    // Simple lighting: Ambient + Diffuse
    float3 lightDir = normalize(LightDir);
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Ambient + diffuse lighting
    float3 ambient = AmbientColor * albedo;
    float3 diffuse = LightIntensity * albedo * NdotL;

    float3 finalColor = ambient + diffuse;

    u_HDROutput[pixelCoord] = float4(finalColor, 1.0);
}