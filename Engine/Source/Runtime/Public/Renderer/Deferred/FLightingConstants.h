// Copyright 2026 HLVM Engine
// 
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "FViewConstants.h"
#include <cstdint>

// Light type enumeration (matches Donut's LightType)
enum class ELightType : uint32_t
{
    Directional = 0,
    Point = 1,
    Spot = 2
};

/**
 * FLightConstants - Single light constant buffer entry.
 * 
 * Mirrors Donut's LightConstants structure.
 * Used in the deferred lighting pass to compute per-pixel lighting.
 */
struct FLightConstants
{
    ELightType LightType;             // Type of light
    float AngularSizeOrInvRange;      // For spot/point: angular size or inverse range
    FVec3 Direction;                  // Light direction (normalized)
    float Intensity;                  // Light intensity
    FVec3 Position;                  // Light position (for point/spot)
    float Radius;                     // Light radius (for point/spot)
    FVec3 Color;                     // Light color (RGB)
    float Padding;                    // Alignment padding
    
    // Shadow-related fields
    int32_t ShadowChannel;           // Shadow map channel (-1 = no shadow)
    int32_t ShadowCascades[4];       // Shadow cascade indices (-1 = not used)
    float PerObjectShadows[4];       // Per-object shadow indices (-1 = not used)
    float OutOfBoundsShadow;         // Shadow value for out-of-bounds
    
    FLightConstants()
    {
        LightType = ELightType::Directional;
        AngularSizeOrInvRange = 0.0f;
        Direction = FVec3(0.0f, -1.0f, 0.0f);
        Intensity = 1.0f;
        Position = FVec3(0.0f);
        Radius = 0.0f;
        Color = FVec3(1.0f);
        Padding = 0.0f;
        ShadowChannel = -1;
        for (int i = 0; i < 4; ++i)
        {
            ShadowCascades[i] = -1;
            PerObjectShadows[i] = -1.0f;
        }
        OutOfBoundsShadow = 0.0f;
    }
};

/**
 * FLightingConstants - Full deferred lighting constant buffer.
 * 
 * Mirrors Donut's DeferredLightingConstants structure.
 * Contains view info, light array, and ambient lighting.
 */
struct FLightingConstants
{
    FViewConstants View;                       // View matrices and info
    
    FVec2 ViewportOrigin;                     // Viewport offset in pixels
    FVec2 ViewportSize;                      // Viewport dimensions
    FVec2 ViewportSizeInv;                   // 1.0f / viewportSize
    FVec2 PixelPositionOffset;                // Pixel position offset for sampling
    
    uint32_t NumLights = 0;                   // Number of active lights
    uint32_t NumLightProbes = 0;             // Number of active light probes
    float IndirectDiffuseScale = 0.0f;        // Scale for indirect diffuse
    float IndirectSpecularScale = 0.0f;       // Scale for indirect specular
    float EnableAmbientOcclusion = 0.0f;      // Enable AO (1.0 or 0.0)
    float Padding1 = 0.0f;                   // Alignment
    
    FVec3 AmbientColorTop = FVec3(0.0f);     // Ambient color from above
    float Padding2 = 0.0f;
    FVec3 AmbientColorBottom = FVec3(0.0f);  // Ambient color from below
    float Padding3 = 0.0f;
    
    // Light array (max 64 lights)
    FLightConstants Lights[64];
    
    // Light probe array (max 8 probes) - simplified, not fully implemented
    // For now, we don't support light probes in the basic version
    
    // Noise pattern for stochastic lighting
    float NoisePattern[4][4];
    
    FVec2 RandomOffset = FVec2(0.0f);        // Random offset for sampling
    float ShadowDistance = 1000.0f;           // Max shadow distance
    FVec3 Padding4 = FVec3(0.0f);
    
    FLightingConstants()
    {
        ViewportOrigin = FVec2(0.0f);
        ViewportSize = FVec2(1920.0f, 1080.0f);
        ViewportSizeInv = FVec2(1.0f / 1920.0f, 1.0f / 1080.0f);
        PixelPositionOffset = FVec2(0.0f);
        AmbientColorTop = FVec3(0.2f);
        AmbientColorBottom = FVec3(0.06f);
        
        // Initialize noise pattern
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                NoisePattern[i][j] = 0.0f;
    }
};
