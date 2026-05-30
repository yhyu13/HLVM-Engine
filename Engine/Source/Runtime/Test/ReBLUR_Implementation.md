# ReBLUR Implementation Documentation

## Overview

This document describes the ReBLUR-style temporal denoiser implementation integrated into the HLVM Engine's few-bounce GI pipeline.

## Reference

Based on NRD's ReBLUR algorithm as documented in `reblurer_implementation_guide.md`.

## Architecture

### Pipeline Flow

```
GI Ray Trace → Bilateral Denoise → ReBLUR (Temporal) → Blit to screen
```

The ReBLUR pass replaces the previous TAA pass while retaining bilateral denoising as a pre-denoiser.

### Components

1. **FReBLURPass** (`Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h`)
   - Header-only interface for the ReBLUR pass
   - Uses SH (Spherical Harmonics) encoding for temporal accumulation
   - Implements anti-lag feedback for stability

2. **FReBLURPass.cpp** (`Engine/Source/Runtime/Private/Renderer/PostProcess/FReBLURPass.cpp`)
   - NVRHI compute shader pipeline management
   - 8x8 thread group dispatch
   - Ping-pong history buffering

3. **ReBLUR_cs.hlsl** (`Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReBLUR_cs.hlsl`)
   - Compute shader implementing the ReBLUR algorithm
   - 8-sample Poisson disk spatial blur
   - Hit distance normalization
   - YCoCg color space encoding

## Key Features

### Hit Distance Normalization

Before temporal accumulation, hit distances are normalized using view-Z and roughness:

```cpp
// Parameters (matching NRD defaults)
HitDistParams[0] = 3.0f;   // A - constant offset
HitDistParams[1] = 0.1f;   // B - viewZ linear scale
HitDistParams[2] = 20.0f;  // C - roughness scale
HitDistParams[3] = -25.0f; // D - roughness falloff

float normHitDist = hitDist / ((A + |viewZ| * B) * exp2(D * roughness^2));
```

### Spherical Harmonics Encoding

Radiance is encoded into SH coefficients using YCoCg color space:

```
SH0 = Y (luminance DC)
SH1 = Co,Cg * Y (chroma scaled by luminance)
A = normalized hit distance (confidence)
```

This compact representation (4 floats vs 3 floats for RGB) enables efficient temporal blending.

### Temporal Accumulation

- History fade-in over first 6 frames (configurable)
- Anti-lag feedback detects unstable regions and accelerates convergence
- Ping-pong textures for history buffering

### Spatial Blur

- 8-sample Poisson disk pattern with rotation based on UV position
- Adaptive radius based on depth
- Normal, plane distance, and roughness rejection weights

## Integration

### Files Modified

- `TestFewBounceGI.cpp` - Integrated ReBLUR pass, replaced TAA
- `CMakeLists.txt` - Added FReBLURPass.cpp to build

### Files Created

- `FReBLURPass.h` - Public header
- `FReBLURPass.cpp` - Implementation
- `ReBLUR_cs.hlsl` - Compute shader
- Updated `ShaderMake.cfg` to include ReBLUR_cs.hlsl

## Configuration

### Blur Parameters (FPooledBlurParams)

```cpp
BlurRadius = 12.0f;        // Spatial blur radius in pixels
NormalWeight = 0.1f;      // Normal rejection weight
PlaneWeight = 100.0f;     // Plane distance sensitivity
RoughnessWeight = 0.3f;   // Roughness rejection weight
AntiLagIntensity = 0.5f;   // Anti-lag feedback intensity (0-1)
DarknessSensitivity = 0.01f; // Darkness sensitivity
```

### Constants (FReBLURConstants)

```cpp
InverseCurrViewProj[16];   // Current frame's inverse ViewProj
PrevViewProj[16];          // Previous frame's ViewProj
ViewMatrix[16];           // Current view matrix
ProjMatrix[16];           // Current projection matrix
OutputSize[2];            // Output dimensions
RcpOutputSize[2];          // 1/Output dimensions
HitDistParams[4];         // A,B,C,D for hit distance normalization
FrameIndex;               // Frame counter
HistoryFadeIn = 6.0f;     // Frames to fade in history
ConfidenceScale = 1.0f;    // Confidence scale factor
```

## Test Results

- **TestFewBounceGI**: PASSED (~9.2-9.5 seconds per run)
- No performance regression compared to previous bilateral+TAA pipeline

## Future Improvements

1. **Motion vectors**: Currently not fully implemented - using simplified history validation
2. **Roughness from GBuffer**: Currently using normals texture - should add dedicated roughness buffer
3. **Adaptive blur radius**: Based on detected noise level
4. **Per-material denoising**: Different params for different surfaces

## Related Work

### ReSTIR GI

See `ReSTIR_Implementation.md` for the reservoir generation pass.

**Current integrated pipeline**:
```
GI Ray Trace → Bilateral Denoise → ReSTIR Generation → ReBLUR → Blit
```

ReBLUR provides temporal denoising after ReSTIR performs reservoir-based sample reuse.

## Changelog

### Phase 6 - ReBLUR Integration (2026-05-29)

- Created FReBLURPass class with SH encoding
- Implemented temporal accumulation with ping-pong history
- Added anti-lag feedback for stability
- Integrated 8-sample Poisson disk spatial blur
- Hit distance normalization with view-Z and roughness
- Replaced TAA pass with ReBLUR in TestFewBounceGI pipeline