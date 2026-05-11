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

#include "Math/MathGLM.h"
#include <cstdint>

/**
 * FViewConstants - View/projection constant buffer for deferred rendering.
 * 
 * Mirrors Donut's PlanarViewConstants structure.
 * Used by both GBufferFillPass and DeferredLightingPass.
 * 
 * Layout must match the HLSL cbuffer layout:
 * struct PlanarViewConstants {
 *     float4x4 matWorldToView;
 *     float4x4 matViewToClip;
 *     float4x4 matClipToView;      // Inverse of viewToClip
 *     float4x4 matClipToWorld;    // For depth reconstruction
 *     float4 cameraDirectionOrPosition;  // xyz=dir or pos, w: >0=position, <0=direction
 *     float2 viewportOrigin;
 *     float2 viewportSize;
 *     float2 viewportSizeInv;
 * };
 */
struct FViewConstants
{
    FMat4 MatWorldToView;      // World to view transform
    FMat4 MatViewToClip;       // View to clip projection
    FMat4 MatClipToView;       // Clip to view (inverse of viewToClip)
    FMat4 MatClipToWorld;      // Clip to world (for depth reconstruction)
    
    // Camera direction (if w < 0) or position (if w > 0)
    FVec4 CameraDirectionOrPosition;
    
    // Viewport in screen pixels
    FVec2 ViewportOrigin;
    FVec2 ViewportSize;
    FVec2 ViewportSizeInv;      // 1.0f / viewportSize

    FViewConstants()
    {
        MatWorldToView = FMat4(1.0f);
        MatViewToClip = FMat4(1.0f);
        MatClipToView = FMat4(1.0f);
        MatClipToWorld = FMat4(1.0f);
        CameraDirectionOrPosition = FVec4(0.0f, 0.0f, 1.0f, 1.0f);  // Default: position at (0,0,1)
        ViewportOrigin = FVec2(0.0f, 0.0f);
        ViewportSize = FVec2(1920.0f, 1080.0f);
        ViewportSizeInv = FVec2(1.0f / 1920.0f, 1.0f / 1080.0f);
    }
};
