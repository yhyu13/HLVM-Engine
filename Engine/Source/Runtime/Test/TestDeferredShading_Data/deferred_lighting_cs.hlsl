// Deferred Lighting Compute Shader - HLVM Native
// Reads GBuffer textures, computes lighting, writes to UAV

// Lighting constants - binding b0 with bRegShift 256 → SPIR-V binding 256
cbuffer LightingConstants : register(b0)
{
    float3 LightDir;       // Directional light direction (normalized)
    float LightIntensity;
    float3 AmbientColor;   // Ambient light color
    float _pad;
};

// GBuffer textures - with tRegShift 0, registers directly map to SPIR-V bindings
Texture2D t_GBuffer0 : register(t0);   // Albedo + Specular
Texture2D t_GBuffer1 : register(t1);   // Normal + Roughness  
Texture2D t_GBuffer2 : register(t2);   // Emissive + Depth

// Output UAV - binding u0 with uRegShift 384 → SPIR-V binding 384
RWTexture2D<float4> u_Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID, uint2 groupId : SV_GroupID)
{
    uint2 pixelCoord = dispatchThreadId;
    
    // Sample GBuffer
    float4 gbuf0 = t_GBuffer0[pixelCoord];  // Albedo.rgb, Specular.a
    float4 gbuf1 = t_GBuffer1[pixelCoord];  // Normal.xyz, Roughness.a
    float4 gbuf2 = t_GBuffer2[pixelCoord];  // Emissive.rgb, Depth.a
    
    // Unpack data
    float3 albedo = gbuf0.rgb;
    float specular = gbuf0.a;
    
    float3 normal = gbuf1.xyz * 2.0 - 1.0;  // Unpack from [0,1] to [-1,1]
    normal = normalize(normal);
    
    float emissive = gbuf2.r;
    float depth = gbuf2.a;
    
    // Skip background pixels (depth = 1.0 means far plane)
    if (depth >= 0.999)
    {
        u_Output[pixelCoord] = float4(0.1, 0.1, 0.1, 1.0);  // Background color
        return;
    }
    
    // Simple directional lighting with ambient
    float NdotL = saturate(dot(normal, -LightDir));
    float3 diffuse = albedo * AmbientColor + albedo * LightDir * NdotL * LightIntensity;
    
    // Add emissive contribution
    float3 finalColor = diffuse + emissive;
    
    // Write result
    u_Output[pixelCoord] = float4(finalColor, 1.0);
}