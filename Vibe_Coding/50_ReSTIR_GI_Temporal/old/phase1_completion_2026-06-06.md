# ReSTIR GI — Phase 1 Completion Report

**Date**: 2026-06-06  
**Scope**: `TestFewBounceGI` — Sponza scene with few-bounce GI + ReSTIR temporal/spatial reuse  
**Status**: ✅ Phase 1 DONE — Stable, recognisable output. Ready for Phase 2.

---

## Phase 1 Goal

Make ReSTIR GI produce a **stable, recognisable image of Sponza** without obvious artifacts (flicker, progressive dimming, black screen, snow-flower noise).

**Exit criteria achieved**:
- Mean RGB ~128 across 4 consecutive frames
- Black pixels: 7.1% (architectural shadows, not noise)
- White pixels: 0% (no HDR clipping)
- Frame-to-frame variation: ~0.4/255 (imperceptible)
- Image is recognisably Sponza with colour variation

---

## What Was Broken (Root Causes)

### 1. GI Ray Tracing Shader (`FewBounceGI.hlsl`)

| Bug | Effect | Fix |
|-----|--------|-----|
| `SV_HitT` (scalar float) used as `float3` normal | `normalize(SV_HitT)` produced garbage normals → NdotL random → 90% black output | Changed to `SV_IntersectionAttributes` with barycentric coords; added RTVertices/RTIndices/RTInstanceInfo buffers |
| Hardcoded `albedo = 0.7` | Entire scene was flat gray, no material differentiation | Added `FInstanceInfo.AlbedoColor` populated from `PBRMaterial::GetAlbedoColor()` |
| No primary direct lighting | Base image was pure black except where GI randomly hit light | Added direct + ambient in `RayGen` using `GBufferDiffuse` |
| `SPP = 1` | Severe salt-and-pepper Monte Carlo noise | Added `SPP = 8` loop in `RayGen` |
| Light intensity = 3.0 with no tonemap | ~12.6% of pixels clipped to white (255) | Reduced intensity to 1.0 + added ACES filmic curve + `saturate()` |
| Double vertex offset (`VertexOffset` in C++ AND shader) | Out-of-bounds reads → zero normals → black | Removed extra `+info.VertexOffset` in shader; C++ already offset indices |

### 2. ReSTIR Temporal Pass (`ReSTIR_Temporal_cs.hlsl`)

| Bug | Effect | Fix |
|-----|--------|-----|
| TAA-style `lerp(curr, hist, 0.85)` inside reservoir merge | Progressive dimming (~33 → 19 → 14 → 12 mean) | Removed lerp; pure reservoir merge (accumulate `w_sum`/`M`, probabilistic selection) |
| `FrameIndex` in candidate-position hash | Static scene flickered every frame due to candidate jitter | Removed `FrameIndex` from `hash22`; deterministic for static scenes |

### 3. ReSTIR Spatial Pass (`ReSTIR_Spatial_cs.hlsl`)

| Bug | Effect | Fix |
|-----|--------|-----|
| Reading from uninitialised `TemporalRadianceTexture` | Black output from spatial pass | Changed to read from `DenoisedHDRTexture` |
| Passthrough copy shader (34 lines) instead of 3×3 merge | ReSTIR spatial reuse was completely disabled | Restored 3×3 neighbour merge with geometric rejection |

### 4. Pipeline / Data Flow

| Bug | Effect | Fix |
|-----|--------|-----|
| Vulkan y-flip not accounted for in reprojection | History reprojection was vertically mirrored | Added `y = 1.0 - y` in temporal reprojection |
| Matrix order `mul(vector, matrix)` | Wrong transform → reprojection off by ~200 pixels | Changed to `mul(matrix, vector)` to match GLM column-major |
| `PrevDepthTexture` type mismatch for `copyTexture` | Validation error / potential crash | Used `D32` typeless format for copy source |

---

## Current Pipeline Architecture (Post-Fix)

```
GBuffer Fill Pass
    ↓
RayTrace GI (FewBounceGI.hlsl)
    - MAX_BOUNCES = 3, SPP = 8
    - Primary direct + ambient in RayGen
    - Per-instance albedo from FInstanceInfo
    - Barycentric normal interpolation in ClosestHit
    - Output: HDRTexture (raw radiance)
    ↓
Bilateral Denoise (SpatialSigma = 4.0)
    - Input: HDRTexture
    - Output: DenoisedHDRTexture
    ↓
ReSTIR Generate
    - 8 candidates in 2-pixel radius
    - Deterministic hash (no FrameIndex)
    - p_hat = Luminance(radiance)
    - Output: Reservoir0 (y, w_sum, M), Reservoir1 (W, p_hat)
    ↓
ReSTIR Temporal
    - Reprojects history using PrevViewProj
    - Validates with depth diff (0.05) + normal dot (0.5)
    - Merges curr + hist reservoirs
    - Clamps M to MaxM
    - Output: TemporalReservoir0/1
    ↓
ReSTIR Spatial
    - 3×3 neighbour merge
    - Geometric rejection (normal dot, depth diff)
    - Accumulates w_sum/M, selects winner
    - Reads radiance at selected y from DenoisedHDRTexture
    - Output: ReSTIRColorTexture
    ↓
Blit to swapchain (with optional tonemap if not done in RayGen)
```

---

## Technical Lessons (ReSTIR-Specific)

### Reservoir Data Layout

We use a split-buffer approach:
- **Reservoir0** = `float4(y.xy, w_sum, M)` — pixel position + accumulated weight + sample count
- **Reservoir1** = `float4(W, p_hat, 0, 0)` — unbiased weight + target PDF of selected sample

This fits in two RGBA32F textures and avoids structured buffer complexity.

### Temporal Merge Math

The correct temporal merge is NOT a TAA lerp. It is:

```hlsl
combinedWSum = currWSum + histWSum;
combinedM    = currM    + histM;

// Probabilistically select winner proportional to weight
if (r * totalWeight < histWSum) {
    y = histY;
    p_hat = histPhat;
}

// Clamp M to prevent stale history dominance
if (combinedM > maxM) {
    combinedWSum *= maxM / combinedM;
    combinedM = maxM;
}

// Unbiased W
W = (p_hat > 0.0) ? (combinedWSum / (combinedM * p_hat)) : 0.0;
```

### Spatial Merge Tradeoff

The current spatial pass outputs `radiance[y]` directly (the radiance at the selected sample position). This is **intentionally biased** but stable.

The unbiased alternative would be:
```hlsl
outRadiance = W * radiance[y];  // W from reservoir
```
But with naive spatial merge (no pairwise MIS), this introduces high variance because neighbours may have very different `p_hat` distributions. Pairwise MIS is the correct fix but adds significant complexity.

**Decision**: Keep biased `radiance[y]` for Phase 1 baseline. Implement pairwise MIS in Phase 2 or 3.

### Deterministic vs Stochastic Candidates

For a **static scene + static camera**, deterministic candidate hashing (no `FrameIndex`) eliminates flicker without quality loss. The temporal pass still accumulates confidence via `M`.

For **dynamic scenes** (camera movement, object motion), reintroduce `FrameIndex` jitter. Temporal validation (depth/normal thresholds) will reject invalid history, and the stochastic candidates will explore new sample positions.

---

## Vulkan / HLSL Pitfalls Encountered

1. **`SV_HitT` is scalar float, not a vector**. Do not use it as a normal substitute.
2. **Barycentric coords come from `SV_IntersectionAttributes`** in Slang/HLSL for triangle geometry. Access as `attrib.uv` (float2).
3. **`ObjectToWorld3x4()`** returns a 3×4 matrix (rotation + translation). Extract rotation with `(float3x3)` cast. Remember `mul(matrix, vector)` for column-major (GLM).
4. **NVRHI register shifts**: `--tRegShift 0 --sRegShift 128 --bRegShift 256 --uRegShift 384`. Texture registers start at `t0`, samplers at `s128`, etc. Don't overlap.
5. **Vulkan Y-flip**: NDC y ranges [-1, 1] with +Y up, but texture coords are [0,1] with +Y down. Reprojection needs `y = 1.0 - y`.
6. **`copyTexture` format compatibility**: Source and dest must have compatible typeless formats. `D32` typeless works for depth copy targets.

---

## Metrics History (For Reference)

| Stage | Mean RGB | Black % | White % | Notes |
|-------|----------|---------|---------|-------|
| Initial (broken normals) | ~33 | 63% | 35% | Bimodal: black + saturated |
| After TAA lerp removed | ~33 stable | — | — | Dimming stopped, still bimodal |
| After normal fix | ~45 | ~40% | ~15% | Less black, still clipping |
| After albedo + direct light | ~85 | ~15% | ~12% | Recognisable, but clipping |
| After ACES + SPP=8 | ~128 | 7.1% | 0% | ✅ Target achieved |
| After deterministic hash | ~128 | 7.1% | 0% | Stable across frames |

---

## Known Limitations (Not Bugs — Missing Features)

These are **intentionally deferred** to Phase 2+. They are documented here so future debugging doesn't mistake them for regressions.

### 1. No Shadow Rays
Back-facing surfaces and surfaces behind geometry still receive `AmbientColor * 0.5` fill. True occlusion requires a shadow ray cast toward the light in `ClosestHit`.

**Visual symptom**: Some areas that should be in shadow are slightly bright. This is expected.

### 2. Spatial W Discarded (Biased)
The spatial pass outputs `radiance[y]` without multiplying by reservoir `W`. This is stable but slightly biased toward bright samples.

**Visual symptom**: Slight brightness bias in spatially merged regions. Variance is lower than unbiased W would be with naive merge.

### 3. Secondary Albedo = Base Color Only
Bounce surfaces use `info.AlbedoColor` (the material's base RGB tint). True textured materials require UV coordinates + texture sampling in the hit shader.

**Visual symptom**: Secondary bounces are tinted by base color but lack detail (no normal maps, no roughness/metallic influence on bounce direction).

### 4. Static Scene Only
Deterministic candidate hashing means moving the camera will cause discontinuous sample changes. Temporal validation catches some of this, but the candidate distribution doesn't adapt to motion.

**Visual symptom**: Camera motion will show sliding sample positions rather than smooth temporal accumulation.

### 5. No MIS for Direct Lighting
Primary direct light is `diffuse * NdotL * lightIntensity`. No multiple importance sampling between light sampling and BRDF sampling.

**Visual symptom**: Not a visible issue for this scene, but noisy for small bright lights.

---

## Phase 2 Proposal

### Option A: Shadow Ray Occlusion (Recommended)
Add a visibility ray in `ClosestHit` toward the directional light. If occluded, remove the direct `NdotL` term (keep ambient fill or reduce it).

**Why first**: It's the biggest visual improvement for least complexity. Sponza has many arches and columns that should cast shadows.

**Implementation**:
- Add `TraceRay()` in `ClosestHit` with `RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH`
- Use a minimal `ShadowHit` / `ShadowMiss` shader group
- Mask direct light by `1.0` if unoccluded, `0.0` if occluded
- Keep ambient at reduced level (e.g., 0.1 instead of 0.5) for occluded

### Option B: Pairwise MIS Spatial Weights
Implement proper unbiased spatial reuse using pairwise MIS weights (as in ReSTIR PT / ReSTIR GI papers).

**Why second**: It reduces variance in spatially merged regions and makes ReSTIR actually better than just bilateral denoise.

**Implementation**:
- Compute `p_hat(y | x_i)` for each neighbour `x_i` at the candidate `y`
- Compute MIS weight `m_i(y)` for each neighbour
- Final radiance = `sum(m_i(y) * W_i * radiance[y])`
- Significantly more shader code and needs careful numerical stability

### Option C: Textured Secondary Bounces
Add UVs to `FRTVertex`, bind material textures (albedo, normal, roughness, metallic) as SRVs, sample in `ClosestHit`.

**Why third**: Makes secondary bounces look materially correct. Complex because it requires material system integration in hit shaders.

**Implementation**:
- Extend `FRTVertex` with `float2 UV`
- Add texture SRV array or bindless descriptors
- Sample albedo in `ClosestHit` for bounce ≥ 1
- Optionally sample normal map for bounce normal perturbation

### Option D: Dynamic Scene Support
Reintroduce `FrameIndex` jitter for candidate hashing, validate history more aggressively, add motion vectors.

**Why later**: Only needed when camera or objects move. Current test has static camera.

---

## Files Modified in Phase 1

| File | Changes |
|------|---------|
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl` | SV_HitT→barycentric, per-instance albedo, primary direct light, SPP loop, ACES tonemap |
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Temporal_cs.hlsl` | Removed TAA lerp, deterministic hash |
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Spatial_cs.hlsl` | Read from DenoisedHDRTexture, restored 3×3 merge |
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Generate_cs.hlsl` | Removed FrameIndex from hash |
| `Engine/Source/Runtime/Test/TestFewBounceGI.cpp` | FInstanceInfo with AlbedoColor, buffer bindings, y-flip fix, matrix order fix, depth format fix |

---

## Cross-References

- Debugging process retrospective: `retrospective_2026-06-06.md`
- Pre-fix critic: `critic_2026-06-06_snowflower.md`
- Post-fix critic: `critic_2026-06-06_snowflower_v2.md`
- Bug log: `.wolf/buglog.json` (bug-045, bug-046)
- Global memory: `~/.claude/projects/.../memory/restir-gi-snowflower-flicker.md`
