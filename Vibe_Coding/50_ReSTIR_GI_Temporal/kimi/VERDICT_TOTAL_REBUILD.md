# VERDICT: TOTAL REBUILD REQUIRED

**Date**: 2026-06-05
**Scope**: 50_ReSTIR_GI_Temporal — Complete architecture and implementation audit
**Reviewer**: Kimi Code CLI (post-mortem analysis)

---

## Executive Summary

**The ReSTIR GI implementation is completely broken.** The self-reviewed "8.5/10" rating was based on theoretical code review, not verified visual output. Frame dumps reveal total corruption: magenta screens, pure black frames, and extreme noise. The architecture is fundamentally incompatible with how ReSTIR actually works.

**Recommendation: Tear down and rebuild from scratch.**

---

## Visual Evidence of Total Failure

Frame dumps from `TestFewBounceGI_Data/dumps/`:

| Frame | Result | Description |
|-------|--------|-------------|
| `2026-05-30_17-28-02_557_frame0004.png` | **Bright magenta + green** | Complete memory corruption or NaN/Inf propagation |
| `2026-05-30_09-48-54_245_frame0002.png` | **Pure black** | Zeroed history buffer or complete pipeline failure |
| `2026-05-30_11-09-54_536_frame0001.png` | **Extreme noise/grain** | Uninitialized texture memory being read |
| `2026-05-30_18-00-35_313_frame0004.png` | **Lime green + black** | More corruption patterns |
| `2026-05-30_18-46-34_086_frame0003.png` | **Dark noise + artifacts** | NaN propagation through temporal history |

**None of these images contain anything resembling GI, Sponza, or even coherent geometry.**

---

## Category 1: Fatal Architectural Flaws (Cannot Be Patched)

### 1.1 ReSTIR Is a Post-Process on Already-Computed GI

**The Problem**: The pipeline is:
```
GI Ray Trace → Bilateral Denoise → [ReSTIR Generation → Temporal → Spatial] → ReBLUR
```

ReSTIR runs **after** the GI pass has already computed radiance with fixed cosine-weighted sampling. ReSTIR then reads this finished image and builds reservoirs from it. **This is mathematically meaningless.**

**Why ReSTIR Cannot Work This Way**:
- ReSTIR's purpose is to **importance-sample light directions/points** during ray tracing
- It must control WHICH rays are traced, not observe the output AFTER tracing
- The reservoir stores a **sample** (direction + its radiance contribution)
- Temporal/spatial reuse **shares good samples** between pixels to reduce variance
- In this implementation, the "sample" is just the pixel's own surface position — there is no resampling happening

**What the Code Actually Does**:
```hlsl
// Generation: "Sample" = pixel's own world position
// p_hat = luminance of ALREADY COMPUTED GI output
// pdf = 1.0 (uniform, but there's nothing to sample!)
Reservoir.y = worldPos;  // <-- This is not a light sample!
Reservoir.W = luminance(denoisedHDR[pixel]);
```

### 1.2 Spatial Pass Is a Complete No-Op

**File**: `ReSTIR_Spatial_cs.hlsl`, line 205:
```hlsl
float3 centerRadiance = gRadiance.Load(int3(pixel, 0)).rgb;
gOutRadiance[pixel] = float4(centerRadiance, spatial.W);
```

The spatial pass outputs **exactly the same RGB** as the input denoised HDR. The entire ReSTIR chain (generation → temporal → spatial) does not modify the radiance values at all. It only changes the alpha channel (which contains `spatial.W`, a weight that ReBLUR misinterprets as hit distance).

**Result**: Enabling or disabling ReSTIR produces the same RGB output. ReSTIR is invisible.

### 1.3 Reservoirs Store Surface Positions, Not Light Samples

**In real ReSTIR GI**:
- Reservoir stores a sampled incoming direction `ω` or sampled point light `y`
- The sample is evaluated at the shading point to compute `L_i(x, ω)`
- Temporal/spatial reuse propagates good directions across pixels and frames

**In this implementation**:
- Reservoir stores `y = GBufferWorldPos[pixel]` — the primary surface position
- Every pixel's "sample" is itself
- There is no concept of "evaluating a sample at a neighbor pixel"
- The reservoir weight `W` is just `luminance(denoisedHDR)` — a scalar that describes the already-computed image brightness

**This is not a simplified ReSTIR. This is not ReSTIR at all.**

---

## Category 2: Critical Implementation Bugs

### 2.1 ReBLUR Reads History at Wrong Coordinates (No Temporal Reprojection)

**File**: `ReBLUR_cs.hlsl`, line 126:
```hlsl
float4 historyPacked = gHistory.Load(int3(dispatchThreadID.xy, 0));
```

The shader defines `ReconstructViewPos()` and has `PrevViewProj` in constants, but **never reprojects the pixel to previous-frame UV space**. It reads history at the **same pixel coordinate** as the current frame.

**Impact**: If the camera moves (or even slightly jitters), the history is read from the wrong location. The "temporal accumulation" just lerps current and history at the same screen pixel, which is spatial averaging, not temporal.

### 2.2 ReBLUR Output Texture Never Cleared (Garbage on Frame 1)

**In `TestFewBounceGI.cpp`, lines 731-738**:
```cpp
// Clear ReBLUR history texture to zero
ClearCmd->clearTextureFloat(ReBLURHistoryTexture, ...);
```

Only `ReBLURHistoryTexture` is cleared. `ReBLUROutputTexture` is never cleared and contains uninitialized GPU memory.

**Pipeline trace**:
```
Frame 1:
  ReBLUR reads History(cleared=0), writes Output(garbage)
  Swap: History=Output(garbage), Output=History(0)

Frame 2:
  ReBLUR reads History(garbage), writes Output(0)
  Swap: History=Output(0), Output=History(garbage)
```

The garbage texture enters the ping-pong cycle and corrupts all subsequent frames.

### 2.3 Spatial Shader Reads W from Wrong Texture Channel

**In `ReSTIR_Spatial_cs.hlsl`, `LoadReservoir()`**:
```hlsl
r.W = data1.z;  // data1.z = pdf = 1.0 (always!)
```

But in generation and temporal shaders:
```hlsl
// Generation: gReservoir1 = float4(w_sum, M, pdf, hitDist)
// Temporal:   gOutReservoir1 = float4(r.w_sum, r.M, r.pdf, r.hitDist)
```

The actual `W` is stored in `Reservoir0.w` (`data0.w`), not `Reservoir1.z`. The spatial shader thinks `W = 1.0` for every pixel, which breaks all weight calculations.

### 2.4 Temporal Reprojection Uses Current-Frame Depth for Validation

**In `ReSTIR_Temporal_cs.hlsl`, lines 210-212**:
```hlsl
float histDepth = gDepth.Load(int3(histPixel, 0)).r;
float depthDiff = abs(depth - histDepth);
if (depthDiff > 0.05 || hist.M <= 0.0) { historyValid = false; }
```

`gDepth` is the **current frame's** depth texture. `histPixel` is the reprojected UV from the **previous frame** projected into current frame space. The code reads current-frame depth at `histPixel` and compares it to current-frame depth at the current pixel.

**This compares two arbitrary pixels in the current frame.** It does NOT validate whether the history pixel corresponds to the same surface. There is no previous-frame depth buffer at all.

### 2.5 Frame Dump Captures Wrong Texture

**In `TestFewBounceGI.cpp`, lines 1407-1425**:
```cpp
else if (bReBLURInitialized) {
    DumpTexture = ReBLURHistoryTexture;  // <-- This is INPUT to ReBLUR, not output!
}
```

The frame dump reads `ReBLURHistoryTexture`, which after the swap is the **previous frame's input** (or garbage on early frames), not the actual rendered output.

---

## Category 3: Additional Bugs and Design Issues

### 3.1 `PrevViewProj` Updated Twice Per Frame

In `Render()`:
- Updated after ReSTIR temporal pass (line 1336)
- Updated again after ReBLUR pass (line 1398)

Both assignments use `currViewProj` from the SAME frame. This is redundant and confusing.

### 3.2 No Previous-Frame GBuffer or Depth

For correct temporal reprojection, you need:
- Previous frame's depth buffer (for depth validation)
- Previous frame's normal buffer (for normal validation)
- Previous frame's world position (for position validation)

The code has none of these. It tries to validate history using only current-frame data.

### 3.3 ReBLUR Samplers Created But Never Used

The ReBLUR C++ pass creates `PointSampler` and `LinearSampler`, binds them to the pipeline, but the shader uses `.Load()` for ALL texture reads. The samplers are dead weight.

### 3.4 Test Only Checks for Crashes, Not Visual Output

```cpp
RECORD_BOOL(test_FewBounceGI) {
    // Runs for 5 seconds
    // If no exception thrown, PASS
}
```

The test passes even though the output is pure corruption. There is no image comparison, no reference PNG, no pixel value validation.

### 3.5 Self-Review Was Theoretical, Not Empirical

The `self_review_final.md` gives an "8.5/10" rating based on code structure and theoretical correctness. **It never examined frame dumps.** All of its "✅ Works" claims are unsubstantiated.

---

## Why Output Is "Not GI" and "Doesn't Render Without GI"

### "Not GI Result"

The pipeline outputs corrupted garbage (magenta, green, noise) because:
1. ReBLUR output texture starts with garbage (uninitialized)
2. ReBLUR reads history at wrong coordinates, so temporal accumulation is random
3. NaN/Inf from uninitialized memory propagates through the ping-pong buffers
4. The bilateral denoise shader uses `t_Input[pixelCoord]` with `[uint2]` coordinates on a `Texture2D` — this uses the sampler, not `.Load()`, which may sample garbage at edges

Even if you remove ReSTIR entirely, ReBLUR alone is so broken that the output is corrupted.

### "Does Not Render Without GI"

The test `TestFewBounceGI` has **no fallback rendering path**. The pipeline is:
```
GBuffer → GI Ray Trace → Denoise → [Optional ReSTIR] → ReBLUR → Blit
```

If you disable the GI ray trace pass, `HDRTexture` is never written (or contains previous frame's garbage), and the rest of the pipeline processes empty/invalid data. There is no "direct lighting only" or "albedo blit" fallback.

---

## What Would Need to Change for a Real ReSTIR GI Implementation

### Approach A: Modify Ray Generation (Correct but Invasive)

1. **Change `FewBounceGI.hlsl` RayGen** to trace multiple candidate samples per pixel (e.g., 4-8 rays)
2. **Store raw samples** in a structured buffer: `{direction, radiance, pdf, hitDist}`
3. **Run ReSTIR generation** that selects the best sample using RIS (resampled importance sampling)
4. **Run temporal reuse** that reprojects the chosen sample and merges reservoirs
5. **Run spatial reuse** that evaluates neighbor samples at the center pixel's position
6. **Final shading pass** that uses the reservoir's chosen direction to compute lighting

This requires:
- Double-buffered history for depth, normals, and sample directions
- Previous-frame TLAS for ray tracing against historical geometry
- Proper motion vectors or camera reprojection
- The reservoir must store `direction` + `radiance`, not `worldPos`

### Approach B: Decoupled ReSTIR Path (Simpler)

1. Keep the existing GI path as a baseline (fix ReBLUR or remove it)
2. Add a separate compute shader that traces short rays and builds reservoirs independently
3. Use reservoir samples to modulate the existing GI output
4. This is approximate but architecturally cleaner

### Minimum Fixes for the Existing Pipeline (Even Without ReSTIR)

If you just want the base GI test to render correctly:
1. Clear `ReBLUROutputTexture` at initialization
2. Fix ReBLUR shader to reproject history UV using `PrevViewProj`
3. Fix bilateral denoise to use `.Load()` instead of sampler indexing
4. Add a "no GI" mode that blits GBuffer diffuse directly

---

## File-by-File Damage Assessment

| File | Verdict | Notes |
|------|---------|-------|
| `TestFewBounceGI.cpp` | **REWRITE** | Pipeline order wrong, texture state chaos, PrevViewProj logic broken |
| `FReSTIRPass.cpp/h` | **DELETE** | The entire abstraction is built on wrong foundations |
| `ReSTIR_Generate_cs.hlsl` | **DELETE** | Computes meaningless reservoirs from finished image |
| `ReSTIR_Temporal_cs.hlsl` | **DELETE** | Reprojection wrong, depth validation wrong |
| `ReSTIR_Spatial_cs.hlsl` | **DELETE** | No-op output, wrong W channel read |
| `ReBLUR_cs.hlsl` | **REWRITE** | No temporal reprojection, reads history at wrong coord |
| `FReBLURPass.cpp/h` | **REWRITE** | Samplers unused, output texture not cleared |
| `BilateralDenoise_cs.hlsl` | **FIX** | Uses sampler instead of `.Load()`; minor fix |
| `FewBounceGI.hlsl` | **REWRITE** | Needs to output raw samples for ReSTIR; currently single-sample only |
| `self_review_final.md` | **DISREGARD** | Theoretical review, no visual verification |

---

## Recommended Path Forward

### Phase 1: Salvage the Baseline (1-2 days)
1. Remove all ReSTIR code from `TestFewBounceGI.cpp`
2. Fix bilateral denoise `.Load()` bug
3. Fix or remove ReBLUR (it doesn't do anything useful in current form)
4. Verify clean GI output without any post-processing

### Phase 2: Proper ReSTIR Design (1 week)
1. Study NVIDIA's ReSTIR GI paper and reference implementation
2. Design the reservoir format: must store `direction`, `radiance`, `pdf`, `hitDistance`
3. Modify ray generation to emit multiple candidate samples
4. Implement generation + temporal + spatial as a unified compute pipeline
5. Add proper history buffers (depth, normal, motion vectors)
6. Integrate with a working denoiser (either fixed ReBLUR or NRD)

### Phase 3: Verify with Frame Dumps (Ongoing)
1. Every implementation phase must include visual validation
2. Compare against reference images or known-good renders
3. Use debug visualization modes extensively

---

## Conclusion

**Do not attempt to patch this implementation.** The architectural foundation is wrong — ReSTIR cannot be a post-process on already-computed GI. The code contains too many interacting bugs across C++ and HLSL. The visual output proves total failure.

**Start over with a correct understanding of ReSTIR as a sampling algorithm, not a filter.**

---

*Generated by Kimi Code CLI — post-mortem analysis of 50_ReSTIR_GI_Temporal*
