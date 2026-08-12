# Technical Bug Breakdown — Shader-by-Shader, Line-by-Line

**Date**: 2026-06-05
**Scope**: Every bug found in the ReSTIR GI implementation

---

## Table of Contents

1. [ReSTIR_Generate_cs.hlsl](#1-restir_generate_cshlsl)
2. [ReSTIR_Temporal_cs.hlsl](#2-restir_temporal_cshlsl)
3. [ReSTIR_Spatial_cs.hlsl](#3-restir_spatial_cshlsl)
4. [ReBLUR_cs.hlsl](#4-reblur_cshlsl)
5. [BilateralDenoise_cs.hlsl](#5-bilateraldenoise_cshlsl)
6. [TestFewBounceGI.cpp](#6-testfewbouncegicpp)
7. [FReSTIRPass.cpp](#7-frestirpasscpp)
8. [FReBLURPass.cpp](#8-freblurrpasscpp)

---

## 1. ReSTIR_Generate_cs.hlsl

### Bug 1.1: Reservoir Stores Surface Position Instead of Light Sample

**Lines**: 115-116
```hlsl
gReservoir0[pixel] = float4(worldPos, W);
gReservoir1[pixel] = float4(w_sum, M, pdf, hitDist);
```

**Problem**: `worldPos` is the primary surface point from the GBuffer. A reservoir should store a **sampled light direction or point** that contributes incoming radiance. Storing the pixel's own position means every pixel's "sample" is itself. There is no resampling.

**Fix**: Store `direction` (the ray direction that produced the radiance) or `samplePoint` (the secondary surface hit point). The GI shader must output this data.

### Bug 1.2: p_hat Is Computed from Already-Denoised Output

**Lines**: 99-106
```hlsl
float p_hat = Luminance(radiance);  // radiance comes from denoised HDR
float w_sum = p_hat;
```

**Problem**: `p_hat` is the target PDF. It should be proportional to the **incoming radiance along a specific direction**. Here it's just the luminance of the final image. This is circular — you're importance-sampling an image you've already rendered.

**Fix**: `p_hat` must be computed per-sample during ray tracing, before denoising.

### Bug 1.3: Debug Output Written for Sky Pixels Even When DebugVis Disabled

**Lines**: 92-94
```hlsl
if (depth == 0.0 || all(radiance == float3(0.0, 0.0, 0.0)))
{
    gDebugOutput[pixel] = float4(0.0, 0.0, 0.0, 1.0);  // Always writes!
    return;
}
```

**Problem**: Even when `DebugVis = 0`, sky pixels write to `gDebugOutput`. This overwrites whatever was there.

**Fix**: Only write to `gDebugOutput` when `DebugVis > 0.5`.

---

## 2. ReSTIR_Temporal_cs.hlsl

### Bug 2.1: History Validation Uses Current-Frame Depth

**Lines**: 209-216
```hlsl
float histDepth = gDepth.Load(int3(histPixel, 0)).r;
float depthDiff = abs(depth - histDepth);
if (depthDiff > 0.05 || hist.M <= 0.0)
{
    historyValid = false;
    hist.M = 0.0;
}
```

**Problem**: `gDepth` is the **current frame's** depth texture. `histPixel` is the reprojected previous-frame UV in current-frame screen space. You're comparing:
- `depth` = current frame depth at current pixel
- `histDepth` = current frame depth at `histPixel` (a DIFFERENT pixel in the current frame)

This does NOT tell you if the history pixel corresponds to the same surface. You need the **previous frame's** depth buffer.

**Fix**: Maintain a `PrevDepthTexture` ping-pong buffer.

### Bug 2.2: Reprojection Formula Assumes Depth in [0,1] Clip Space

**Lines**: 83-91
```hlsl
float4 clipPos = float4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
```

**Problem**: `depth * 2.0 - 1.0` assumes depth is in [0,1] and needs mapping to [-1,1]. But the GBuffer uses `D32` format with `Less` comparison. The depth value stored is already the raw Z value from projection, which is NOT linear and NOT in [0,1] clip space after `glm::perspectiveLH_ZO`.

Actually, `glm::perspectiveLH_ZO` does map to [0,1] Z-range. But the issue is that `depth` is the hardware depth buffer value, which IS in [0,1]. The multiplication by 2 and subtraction by 1 is correct for clip space. However, `ReconstructViewPos` uses this same formula and the `InverseCurrViewProj` matrix to get back to view space.

Wait — actually this might be correct for a zero-to-one depth buffer. Let me reconsider. `glm::perspectiveLH_ZO` produces NDC Z in [0,1]. So `depth * 2 - 1` maps to [-1,1] which is correct for clip space when multiplied by inverse projection.

Actually no, `glm::perspectiveLH_ZO` already outputs [0,1] directly. To get clip space you need `depth * 2 - 1` if you're treating it as post-projection. But `mul(gConstants.InverseCurrViewProj, clipPos)` expects clip space coordinates (X,Y,Z,W) where Z is in [-1,1] for the inverse of a standard projection. However, `ZO` means zero-to-one, so the projection matrix maps to [0,1], not [-1,1].

So the formula `depth * 2.0 - 1.0` is WRONG for zero-to-one projection. It should just be `depth` directly if the inverse matrix accounts for [0,1], or the inverse matrix needs to be constructed differently.

This is a subtle but real bug. The reprojection is off.

### Bug 2.3: Jacobian Is Assumed to Be 1.0 Without Justification

**Line**: 144
```hlsl
float w_sum_combined = curr.w_sum + hist.w_sum;  // "Jacobian = 1.0"
```

**Problem**: When merging reservoirs from different pixels/frames, you need the Jacobian determinant of the PDF transformation. For temporal reuse, this accounts for how the PDF changes when a sample from pixel `q` is evaluated at pixel `p`. The code assumes `J = 1.0` with no derivation.

For world-position-based samples (which these aren't really, but let's pretend), the Jacobian would involve the distance ratio squared. For direction-based samples, it involves solid angle compression.

**Fix**: Actually compute the Jacobian, or use direction-based samples where the Jacobian is better behaved.

### Bug 2.4: Merged Reservoir Loses Hit Distance from Loser

**Lines**: 150-160
```hlsl
if (rand < selectHist)
{
    merged.y = hist.y;
    merged.hitDist = hist.hitDist;
}
else
{
    merged.y = curr.y;
    merged.hitDist = curr.hitDist;
}
```

**Problem**: The reservoir winner is selected, but the `w_sum` is the SUM of both. In standard RIS, when you merge two reservoirs, the winner is selected proportionally to their weights, but the `w_sum` should be `w_sum_a + w_sum_b * (p_hat_y_at_a / p_hat_y_at_b) * J`. The code just adds them.

For this simplified case it might be acceptable, but combined with Bug 2.1 (history often invalid), the merge rarely does anything useful.

---

## 3. ReSTIR_Spatial_cs.hlsl

### Bug 3.1: Spatial Pass Outputs Original Input Unchanged

**Line**: 205
```hlsl
gOutRadiance[pixel] = float4(centerRadiance, spatial.W);
```

**Problem**: This is the **fatal bug** that makes ReSTIR invisible. The RGB output is exactly `gRadiance.Load(int3(pixel, 0)).rgb` — the denoised HDR input. The spatial merge result `spatial` is completely ignored for color.

**Fix**: The spatial pass should evaluate neighbor samples at the center pixel. If neighbor `q` has a reservoir storing direction `ω_q`, you trace or evaluate `L_i(center, ω_q)` to see what radiance that sample would produce at the center pixel. Then output the weighted average of these evaluations.

### Bug 3.2: LoadReservoir Reads W from Wrong Channel

**Lines**: 73-87
```hlsl
Reservoir LoadReservoir(Texture2D<float4> r0, Texture2D<float4> r1, int2 pixel)
{
    float4 data0 = r0.Load(int3(pixel, 0));
    float4 data1 = r1.Load(int3(pixel, 0));
    r.y = data0.xyz;
    r.hitDist = data0.w;
    r.w_sum = data1.x;
    r.M = data1.y;
    r.W = data1.z;  // <-- WRONG! data1.z = pdf = 1.0
    r.seed = asuint(data1.w);
}
```

**Problem**: Generation and temporal shaders write:
```hlsl
gReservoir1[pixel] = float4(w_sum, M, pdf, hitDist);  // z = pdf = 1.0
```

So `r.W = data1.z` always reads `1.0`. The actual `W` is in `data0.w` (Reservoir0's alpha channel).

**Impact**: All spatial weight calculations use `W = 1.0`, making them meaningless. But since the output ignores the spatial result anyway (Bug 3.1), this doesn't visibly matter.

### Bug 3.3: StoreReservoir Is Never Called

**Lines**: 89-93
```hlsl
void StoreReservoir(RWTexture2D<float4> r0, RWTexture2D<float4> r1, int2 pixel, Reservoir r)
{
    r0[pixel] = float4(r.y, r.hitDist);
    r1[pixel] = float4(r.w_sum, r.M, r.W, asfloat(r.seed));
}
```

**Problem**: This function exists but is never used in the shader. The spatial pass writes directly to `gOutRadiance` and `gDebugOutput`.

### Bug 3.4: Seed Is Stored as float but Used as uint

**Lines**: 84, 92
```hlsl
r.seed = asuint(data1.w);           // Load: bit-cast float -> uint
r1[pixel] = float4(..., asfloat(r.seed));  // Store: bit-cast uint -> float
```

**Problem**: The seed is a hash value stored in a float texture. While `asuint`/`asfloat` preserves bits, this is fragile. If the texture format is ever changed to something that doesn't preserve exact float32 values (e.g., RGBA16F), the seed will be corrupted.

**Fix**: Use a separate `RWTexture2D<uint>` or `RWStructuredBuffer<uint>` for seeds.

### Bug 3.5: Normal Weight Uses acos(0) Which Is Expensive and Unstable

**Lines**: 130-132
```hlsl
float normalDot = dot(centerNormal, neighborNormal);
float normalAngle = acos(saturate(normalDot));
float nWeight = exp(-normalAngle * normalSigma * 10.0);
```

**Problem**: `acos` is expensive and numerically unstable near `dot = 1.0` (where `acos(1.0) = 0` but gradients blow up). A standard approach is:
```hlsl
float nWeight = pow(max(normalDot, 0.0), normalSigma);
```

### Bug 3.6: Plane Distance Check Is Skipped

**Lines**: 135-136
```hlsl
// Skip plane distance check - use simpler approximation
float pWeight = 1.0;
```

**Problem**: The comment says "skip". Plane distance is critical for rejecting neighbors across depth discontinuities (e.g., edge of a pillar). Without it, neighbors from background surfaces leak onto foreground edges.

---

## 4. ReBLUR_cs.hlsl

### Bug 4.1: History Read at Same Pixel Coordinate (No Reprojection)

**Line**: 126
```hlsl
float4 historyPacked = gHistory.Load(int3(dispatchThreadID.xy, 0));
```

**Problem**: This reads history at `(dispatchThreadID.xy)` — the exact same pixel as current. For temporal accumulation to work, you must:
1. Compute current pixel's world position
2. Reproject to previous frame using `PrevViewProj`
3. Convert to UV
4. Sample history at the REPROJECTED UV

The shader has all the matrices (`InverseCurrViewProj`, `PrevViewProj`) but never uses them for history sampling.

**Impact**: Stationary camera looks okay-ish (same pixel = same surface). Any camera movement causes severe ghosting, smearing, or flickering because history is read from the wrong location.

### Bug 4.2: History Validity Check Is Trivial

**Lines**: 63-66, 140
```hlsl
bool IsHistoryValid(float2 historyUv) {
    return all(historyUv > 0.0) && all(historyUv < 1.0);
}
// ...
bool historyValid = history.hitDist > 0.0 && IsHistoryValid(pixelUv);
```

**Problem**: `IsHistoryValid` receives `pixelUv` (the CURRENT pixel's UV), not a reprojected UV. It always returns `true` for non-edge pixels. And `history.hitDist > 0.0` is also trivially true for most pixels.

**Fix**: Reproject, check depth difference, check normal difference, check roughness difference.

### Bug 4.3: Anti-Lag Can Completely Disable Temporal Accumulation

**Lines**: 144-146
```hlsl
float antiLagScale = ComputeAntiLagScale(current, history);
if (antiLagScale < 0.1)
    temporal = current;
```

**Problem**: `antiLagScale` compares current and history radiance. If history is garbage (which it often is due to Bug 4.1 and uninitialized memory), the difference is huge, `antiLagScale` becomes tiny, and temporal accumulation is disabled. This means every frame is just the noisy current input.

### Bug 4.4: Hit Distance Normalization Uses View-Space Z

**Lines**: 121-122
```hlsl
float viewZ = ReconstructViewPos(pixelUv, depth).z;
float normHitDist = GetNormHitDist(hitDist, abs(viewZ), roughness);
```

**Problem**: `ReconstructViewPos` uses `InverseCurrViewProj`. But the depth buffer stores hardware depth, and the reconstruction assumes standard projection. In left-handed ZO (zero-to-one) projection used by `glm::perspectiveLH_ZO`, the reconstruction might be correct, but it's fragile.

More importantly, `hitDist` comes from the GI shader output alpha. The GI shader sets `Output[pixel] = float4(payload.radiance, 1.0)`, so `hitDist = 1.0` always. This makes `normHitDist` a constant, rendering the hit-distance-aware parts of ReBLUR meaningless.

---

## 5. BilateralDenoise_cs.hlsl

### Bug 5.1: Sampler-Based Texture Read on UAV Input

**Lines**: 66-68
```hlsl
float centerDepth = t_Depth[pixelCoord];
float3 centerNormal = normalize(t_Normal[pixelCoord].rgb * 2.0 - 1.0);
float3 centerValue = t_Input[pixelCoord];
```

**Problem**: `t_Input` is declared as `Texture2D<float3>` (SRV) but in C++ it's bound as the HDR output texture. The indexing `t_Input[pixelCoord]` uses implicit sampler behavior, not explicit `.Load()`. For compute shaders, `[]` on `Texture2D` may use the default sampler (which could be linear, causing blurring at edges) or point (causing potential offset issues).

**Fix**: Use `.Load(int3(pixelCoord, 0))` consistently.

### Bug 5.2: Normal Sigma Interpreted as Radians

**Lines**: 50-53
```hlsl
float threshold = cos(sigma);
return smoothstep(0.0, 1.0, (dotProd - threshold) / (1.0 - threshold + 0.0001));
```

**Problem**: `cos(sigma)` where `sigma` is passed as `0.1` (radians) gives `cos(0.1) ≈ 0.995`. This means normals must be within ~0.1 radians (5.7 degrees) to get any weight. This is very strict and may reject valid neighbors on curved surfaces.

**Impact**: Minor — the bilateral filter still works, just with sharper edges.

---

## 6. TestFewBounceGI.cpp

### Bug 6.1: PrevViewProj Updated Twice Per Frame with Same Value

**Lines**: 1336, 1398
```cpp
PrevViewProj = currViewProj;  // After ReSTIR temporal
// ...
PrevViewProj = currViewProj;  // After ReBLUR
```

**Problem**: Redundant and confusing. The second assignment overwrites the first with the same value, but if they were ever different (e.g., if ReBLUR modified the matrix), the logic would be wrong.

### Bug 6.2: Frame Dump Reads Input Texture, Not Output

**Lines**: 1418-1425
```cpp
else if (bReBLURInitialized) {
    DumpTexture = ReBLURHistoryTexture;  // Input to ReBLUR!
}
```

**Problem**: After the swap, `ReBLURHistoryTexture` contains the previous frame's ReBLUR output. But the dump happens on the current frame before the NEXT swap. So:
- Frame 1 dump: reads History (cleared to 0) = BLACK
- Frame 2 dump: reads History (frame 1 output) = frame 1 result
- etc.

This is inconsistent. The dump should read `ReBLUROutputTexture` (the current frame's result) or the swap should happen after dump.

### Bug 6.3: No "Without GI" Fallback Path

**Problem**: The pipeline always runs GI ray tracing. If the GI pass is skipped, `HDRTexture` contains uninitialized data. There is no mode to render direct lighting + albedo only.

### Bug 6.4: ReBLUR Output Texture Never Cleared

**Lines**: 711-728
```cpp
ReBLURHistoryTexture = NvrhiDevice->createTexture(Desc);  // Cleared below
ReBLUROutputTexture = NvrhiDevice->createTexture(Desc);   // NEVER CLEARED
```

**Problem**: Only `ReBLURHistoryTexture` is cleared to zero. `ReBLUROutputTexture` contains whatever garbage the GPU memory had. This enters the ping-pong cycle on frame 1.

### Bug 6.5: ReSTIR Textures Created as RGBA32F But Plan Says RGBA16F

**Lines**: 755-788
```cpp
Desc.format = nvrhi::Format::RGBA32_FLOAT;
```

**Problem**: The plan (`plan.md`) specifies `RGBA16F` for reservoirs to save memory. The implementation uses `RGBA32F`, wasting ~35 MB of VRAM.

### Bug 6.6: Texture State Transitions Missing Barriers

**Problem**: Multiple `setTextureState` calls are issued back-to-back without `commitBarriers()` or state tracking verification. While NVRHI may handle this automatically, rapid state transitions on the same texture (e.g., `Reservoir0Texture` transitioned to SRV then immediately back to UAV) can cause synchronization issues.

---

## 7. FReSTIRPass.cpp

### Bug 7.1: Constant Buffer Layout Mismatch Between Shaders

**Problem**: The C++ code uses a single 256-byte constant buffer for all three shaders (generation, temporal, spatial). But each shader has a different constant structure:
- Generation: 32 bytes (OutputSize, RcpOutputSize, FrameIndex, DebugVis, Pad)
- Temporal: 148 bytes (2x float4x4 + OutputSize + ...)
- Spatial: 48 bytes (OutputSize + RcpOutputSize + 6 floats + Pad)

The C++ code packs different data into the same buffer for each dispatch. This works because each dispatch is independent, but it's fragile. If two dispatches were batched, they'd corrupt each other.

### Bug 7.2: No Validation of Texture Dimensions Match

**Problem**: `Dispatch()`, `DispatchTemporal()`, and `DispatchSpatial()` accept textures but don't verify they all have the same dimensions. Passing mismatched textures causes out-of-bounds access or partial coverage.

### Bug 7.3: BindingSet Created Every Frame

**Lines**: 319-330 (example from Dispatch)
```cpp
nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, GenerationLayout);
```

**Problem**: A new binding set is allocated every frame for every dispatch. This causes GPU descriptor heap churn. For a temporal technique that runs every frame, binding sets should be cached and only recreated when textures change (e.g., resize).

---

## 8. FReBLURPass.cpp

### Bug 8.1: Samplers Created But Shader Uses .Load()

**Lines**: 74-95
```cpp
PointSampler = Device->createSampler(...);
LinearSampler = Device->createSampler(...);
```

**Problem**: The shader (`ReBLUR_cs.hlsl`) uses `.Load()` for ALL texture reads. The samplers are bound but never used. This wastes binding slots and descriptor heap space.

### Bug 8.2: Constant Buffer Size Mismatch

**Lines**: 142-152
```cpp
BufferDesc.byteSize = 512;  // C++ allocates 512 bytes
```

**Shader expects**: `FReBLURConstants` is 336 bytes (asserted in header).

**Problem**: While 512 > 336 is safe (extra padding), the C++ packing code writes 128 floats = 512 bytes. The shader reads 84 floats = 336 bytes. The extra data is ignored, but if the layout ever changes, this mismatch could cause silent corruption.

### Bug 8.3: No Output Texture Clearing

**Problem**: Same as Bug 6.4 — the C++ pass doesn't clear its output texture, assuming the caller does. But `TestFewBounceGI.cpp` only clears the history texture, not the output texture.

---

## Summary: Bug Count by Severity

| Severity | Count | Description |
|----------|-------|-------------|
| **Fatal (Architecture)** | 3 | ReSTIR as post-process, reservoirs store surface pos, spatial no-op |
| **Critical** | 5 | ReBLUR no reprojection, uninitialized output texture, temporal depth validation wrong, W channel wrong, frame dump reads wrong texture |
| **Major** | 6 | Reprojection formula wrong, no prev-frame depth, sampler unused, constant buffer mismatch, anti-lag disables accumulation, bilateral sampler read |
| **Minor** | 4 | Debug output always writes, seed stored as float, acos in normal weight, plane check skipped |
| **Nit** | 2 | PrevViewProj updated twice, RGBA32F instead of RGBA16F |

**Total: 20 distinct bugs across 8 files.**

---

*This document should be read alongside `VERDICT_TOTAL_REBUILD.md` for the full picture.*
