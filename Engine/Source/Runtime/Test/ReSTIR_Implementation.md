# ReSTIR GI Implementation Documentation

## Overview

This document describes the ReSTIR GI (Reservoir-based Spatio-Temporal Importance Resampling) implementation in the HLVM Engine.

**Current Phase**: 9 — Advanced Features (Environment Sampling + Poisson Disk)  
**Status**: ✅ Implemented and tested

## Reference

- Bitterli et al. "Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting" (ReSTIR DI)
- Ouyang et al. "ReSTIR GI: Path Resampling for Real-Time Path Tracing" (ReSTIR GI)

## Architecture

### Pipeline Flow

```
GBuffer Pass
    ↓
GI Ray Trace (3 bounces + procedural sky)
    ↓
Bilateral Denoise
    ↓
ReSTIR Generation (Phase 7a)
    ↓
ReSTIR Temporal Reuse (Phase 7b)
    ↓
ReSTIR Spatial Reuse (Phase 8 — Poisson disk sampling)
    ↓
ReBLUR Temporal Denoiser
    ↓
Blit to screen
```

### Reservoir Representation

Stored in 2x RGBA16F textures per frame:

| Texture | R | G | B | A |
|---------|---|---|---|---|
| `Reservoir0` | worldPos.x | worldPos.y | worldPos.z | W (unbiased weight) |
| `Reservoir1` | w_sum | M (sample count) | pdf | hitDist |

### Ping-Pong Buffer Management

- **Current**: `Reservoir0Texture`, `Reservoir1Texture` — generation output
- **History**: `Reservoir0HistoryTexture`, `Reservoir1HistoryTexture` — merged result from previous frame
- **Merged**: `Reservoir0MergedTexture`, `Reservoir1MergedTexture` — temporal merge output

**Swap pattern per frame**:
1. Generation writes to `Current`
2. Temporal reads `Current` + `History`, writes to `Merged`
3. Swap `Merged` <-> `History`

### Spatial Merge Algorithm (Phase 8-9)

For each pixel center:
1. Read center merged reservoir
2. For each neighbor in **12-point Poisson disk** (rotated per-pixel):
   a. Compute geometric weights:
      - **Normal weight**: `exp(-acos(dot(centerNormal, neighborNormal)) * normalSigma * 10.0)`
      - **Plane weight**: `exp(-planeDist * planeSigma * 1000.0)`
      - **Depth weight**: `exp(-depthDiff * depthSigma * 100.0)`
   b. Reject if combined weight < 0.01
   c. Merge neighbor reservoir into spatial reservoir
3. Output: `RGB = centerRadiance`, `A = W_merged`

**Poisson Disk Pattern** (12 points, rotated per pixel):
```hlsl
static const float2 PoissonDisk[12] = {
    float2(-0.326212, -0.405810),
    float2(-0.840144, -0.073580),
    float2(-0.695914,  0.457137),
    float2(-0.203345,  0.620716),
    float2( 0.962340, -0.194983),
    float2( 0.473434, -0.480026),
    float2( 0.519456,  0.767022),
    float2( 0.185461, -0.893124),
    float2( 0.507431,  0.064425),
    float2( 0.896420,  0.412458),
    float2(-0.321940, -0.932615),
    float2(-0.791559, -0.597710)
};
```

### Environment Sampling (Phase 9)

**Procedural Sky Model**:
```hlsl
float3 SampleSky(float3 direction) {
    // Horizon gradient: blue zenith → warm horizon → dark ground
    float3 skyColor = lerp(groundColor, horizonColor, zenithColor, direction.y);
    
    // Sun disk + glow (aligned with directional light)
    float3 sun = sunColor * (pow(sunDot, 512) + pow(sunDot, 6) * 0.15);
    
    // Intensity scale: 0.3 (30% of direct light intensity)
    return (skyColor + sun) * 0.3;
}
```

The sky is sampled in the miss shader and attenuated by `payload.throughput`, giving physically plausible multi-bounce sky lighting.

## Files Created

### C++ Pass

- `Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h`
- `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp`

### Compute Shaders

- `ReSTIR_Generate_cs.hlsl` — Phase 7a: per-pixel reservoir generation
- `ReSTIR_Temporal_cs.hlsl` — Phase 7b: temporal reservoir merge with reprojection
- `ReSTIR_Spatial_cs.hlsl` — Phase 8-9: spatial reuse with Poisson disk sampling

### Ray Tracing Shader

- `FewBounceGI.hlsl` — Modified miss shader with procedural sky sampling

## Files Modified

- `TestFewBounceGI.cpp` — Full pipeline integration
- `ShaderMake.cfg` — All ReSTIR + GI shaders
- `CMakeLists.txt` — `FReSTIRPass.cpp`

## Configuration

### Debug Visualization

Set `g_ReSTIRDebugVis = true` in `TestFewBounceGI.cpp`:

```cpp
static bool g_ReSTIRDebugVis = true;
```

Modes:
- **Generation**: `DebugVis = 1.0` — log-scaled W as grayscale
- **Temporal**: `DebugVis = 1.0` — M as grayscale (linear, normalized to MaxM)
- **Spatial**: `DebugVis = 1.0` — M as grayscale
- **Spatial**: `DebugVis = 2.0` — rejection mask (green = accepted, red = rejected)

### Spatial Constants (FReSTIRSpatialConstants)

```cpp
struct FReSTIRSpatialConstants {
    float OutputSize[2];
    float RcpOutputSize[2];
    float NormalSigma;    // Normal rejection (default 0.1)
    float PlaneSigma;     // Plane distance (default 100.0)
    float DepthSigma;     // Depth difference (default 0.01)
    float MaxM;           // Cap on M (default 30)
    float SpatialRadius;  // Poisson disk radius in pixels (default 3.0)
    float DebugVis;
};
```

### Sky Parameters

Hardcoded in `FewBounceGI.hlsl`:
- `skyIntensity = 0.3` — 30% of direct light intensity
- `zenithColor = (0.4, 0.6, 1.0)` — Blue sky
- `horizonColor = (0.7, 0.8, 0.9)` — Warm horizon
- `groundColor = (0.15, 0.12, 0.1)` — Dark ground
- Sun aligned with `g_GI.LightDir`

## Register Maps

### Generation Shader

| Register | Name | Type | Purpose |
|----------|------|------|---------|
| b0 | `gConstants` | cbuffer | Pass constants |
| t0 | `gRadiance` | Texture2D | Denoised GI radiance |
| t1 | `gWorldPos` | Texture2D | GBuffer world position |
| t2 | `gNormals` | Texture2D | GBuffer normals |
| t3 | `gDepth` | Texture2D | Linear depth |
| u0 | `gReservoir0` | RWTexture2D | Reservoir data 0 |
| u1 | `gReservoir1` | RWTexture2D | Reservoir data 1 |
| u2 | `gDebugOutput` | RWTexture2D | Debug visualization |

### Temporal Shader

| Register | Name | Type | Purpose |
|----------|------|------|---------|
| b0 | `gConstants` | cbuffer | Temporal constants |
| t0 | `gCurrReservoir0` | Texture2D | Current reservoir 0 |
| t1 | `gCurrReservoir1` | Texture2D | Current reservoir 1 |
| t2 | `gHistReservoir0` | Texture2D | History reservoir 0 |
| t3 | `gHistReservoir1` | Texture2D | History reservoir 1 |
| t4 | `gDepth` | Texture2D | Linear depth |
| u0 | `gOutReservoir0` | RWTexture2D | Merged reservoir 0 |
| u1 | `gOutReservoir1` | RWTexture2D | Merged reservoir 1 |
| u2 | `gDebugOutput` | RWTexture2D | Debug visualization |

### Spatial Shader

| Register | Name | Type | Purpose |
|----------|------|------|---------|
| b0 | `gConstants` | cbuffer | Spatial constants |
| t0 | `gMergedReservoir0` | Texture2D | Merged reservoir 0 |
| t1 | `gMergedReservoir1` | Texture2D | Merged reservoir 1 |
| t2 | `gNormals` | Texture2D | GBuffer normals |
| t3 | `gDepth` | Texture2D | Linear depth |
| t4 | `gRadiance` | Texture2D | Denoised HDR radiance |
| u0 | `gOutRadiance` | RWTexture2D | Spatially stable radiance |
| u1 | `gDebugOutput` | RWTexture2D | Debug visualization |

**NVRHI Binding Offsets**: `constantBufferOffset = 0`

## Testing Strategy

1. **Compile**: `./Build.sh --Config=Debug --Target=TestFewBounceGI`
2. **Run**: `./Build.sh --Config=Debug --Target=TestFewBounceGI --Test`
3. **Frame dump**: `HLVM_DUMP_GI=1 ./Build.sh ...` — verify sky is visible (blue/white) in Sponza open ceiling
4. **Debug vis M**: Static pixels should show M increasing over frames
5. **Debug vis rejection mask**: Geometry edges should show red (rejected), flat surfaces green (accepted)

## Test Results

- **TestFewBounceGI**: ✅ PASSED (~9.0-9.2 seconds)
- Shader compilation: 8 tasks
- No performance regression across all phases

## Self-Critic Review Summary

### Phase 7a

1. **Unused sampler binding**: Removed `SamplerState` from HLSL and `PointSampler` from C++
2. **Unnecessary UAV writes**: Removed else-branch zero-writes to debug output

### Phase 7b

1. **RAW hazard in initial plan**: Fixed by using separate `Merged` textures with ping-pong swap
2. **Random hash quality**: Improved bit-mixing constants

### Phase 8

1. **Shading approach under-defined**: Revised to output center radiance RGB + merged confidence A
2. **HLSL type mismatch**: Fixed `float2` construction from `float[2]`

### Phase 9

1. **Sky intensity**: Hardcoded at 0.3 — documented for future tuning
2. **Poisson disk verified**: Standard 12-point pattern from graphics literature

## Known Limitations

1. **No motion vectors**: Reprojection uses depth + camera matrices only
2. **No visibility rays**: Occlusion detected via depth difference only
3. **Uniform target PDF**: `p(y) = 1.0` is a simplification
4. **Debug vis is static bool**: Cannot toggle at runtime in test executable
5. **ReBLUR alpha mismatch**: Spatial output alpha is confidence, not hit distance (non-regression: original GI shader already output `alpha = 1.0`)
6. **Simplified view reconstruction**: `ReconstructViewPos` uses UV-depth space for plane distance

## Changelog

### Phase 7a — Reservoir Generation (2026-05-29)

- Created `FReSTIRPass` class
- Implemented `ReSTIR_Generate_cs.hlsl`
- Reservoir format: worldPos + unbiased weight

### Phase 7b — Temporal Reuse (2026-05-29)

- Added `DispatchTemporal()`
- Implemented `ReSTIR_Temporal_cs.hlsl`
- Ping-pong reservoir buffers
- Reprojection with depth validation

### Phase 8 — Spatial Reuse (2026-05-29)

- Added `DispatchSpatial()`
- Implemented `ReSTIR_Spatial_cs.hlsl`
- 3x3 neighbor kernel with geometric rejection
- Center radiance output + confidence alpha

### Phase 9 — Advanced Features (2026-05-29)

- **Procedural sky sampling** in `FewBounceGI.hlsl` miss shader
  - Horizon gradient, sun disk, sun glow
  - Sky intensity = 0.3 * direct light
  - Multi-bounce attenuation via throughput
- **12-point Poisson disk spatial kernel**
  - Replaced 3x3 grid with well-distributed sampling
  - Per-pixel rotation for decorrelation
  - `SpatialRadius` parameter (default 3.0 pixels)
