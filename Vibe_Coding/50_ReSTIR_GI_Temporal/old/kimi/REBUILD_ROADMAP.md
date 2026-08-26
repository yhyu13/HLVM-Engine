# ReSTIR GI Rebuild Roadmap

**Date**: 2026-06-05
**Status**: Post-mortem planning — starting from scratch
**Goal**: A minimal, correct, and verifiable ReSTIR GI implementation

---

## Philosophy: Build a Working Baseline First

The previous attempt failed because it tried to bolt ReSTIR onto an existing GI pipeline as a post-process. **ReSTIR is a sampling algorithm, not a filter.** It must live inside or directly control the ray generation phase.

**New rule**: Every phase must produce a visually verifiable image before moving to the next phase.

---

## Phase 0: Salvage the Baseline (Day 1)

Before any ReSTIR work, fix the existing `TestFewBounceGI` so it renders clean GI without corruption.

### 0.1 Remove All ReSTIR and ReBLUR Code

From `TestFewBounceGI.cpp`:
- Remove `ReSTIRPass`, all reservoir textures, all ReSTIR dispatches
- Remove `ReBLURPass`, history/output textures, ReBLUR dispatches
- Keep: GBuffer, GI ray trace, bilateral denoise, blit to screen

### 0.2 Fix Bilateral Denoise Sampler Bug

**File**: `BilateralDenoise_cs.hlsl`

Change:
```hlsl
float centerDepth = t_Depth[pixelCoord];
float3 centerNormal = normalize(t_Normal[pixelCoord].rgb * 2.0 - 1.0);
float3 centerValue = t_Input[pixelCoord];
```

To:
```hlsl
float centerDepth = t_Depth.Load(int3(pixelCoord, 0));
float3 centerNormal = normalize(t_Normal.Load(int3(pixelCoord, 0)).rgb * 2.0 - 1.0);
float3 centerValue = t_Input.Load(int3(pixelCoord, 0));
```

Also change all `t_Depth[uint2(neighborPixel)]` to `.Load(int3(neighborPixel, 0))`.

### 0.3 Verify Clean Output

**Command**:
```bash
./Build.sh --Config=Debug --Target=TestFewBounceGI --Test
HLVM_DUMP_GI=1 ./Engine/Source/Runtime/Binary/Debug/TestFewBounceGI
```

**Success criteria**:
- Frame dumps show coherent Sponza scene with indirect lighting
- No magenta/green/black corruption
- No extreme noise
- Sky is visible with gradient

---

## Phase 1: Simple Temporal Accumulation (Days 2-3)

Before ReSTIR, implement a working temporal accumulation pass. This teaches the history buffer infrastructure and validates reprojection.

### 1.1 Add Previous-Frame Depth Buffer

In `TestFewBounceGI.cpp`:
- Create `PrevDepthTexture` (same format as GBuffer depth)
- After GBuffer pass, copy `GBufferDepthTexture` → `PrevDepthTexture`
- After swap/resize, clear `PrevDepthTexture` to 1.0

### 1.2 Add Previous-Frame Normal Buffer

- Create `PrevNormalTexture` (RGBA16F)
- After GBuffer pass, copy `GBufferNormalsTexture` → `PrevNormalTexture`

### 1.3 Write Simple Temporal Accumulation Shader

**New file**: `SimpleTemporal_cs.hlsl`

```hlsl
// Inputs: current denoised HDR, previous frame output, current depth, previous depth, current normal, previous normal
// Output: temporally accumulated HDR

// Per pixel:
// 1. Reproject current pixel to previous frame UV using PrevViewProj
// 2. Read history at reprojected UV (use bilinear filter or nearest)
// 3. Validate: depth diff < threshold, normal dot > threshold
// 4. If valid: output = lerp(history, current, 1.0 / (frameIndex + 1))
// 5. If invalid: output = current
```

### 1.4 Implement in C++

- Create `SimpleTemporalPass` class (or inline in test)
- Double-buffer temporal output (ping-pong)
- Update `PrevViewProj` ONCE per frame, AFTER all rendering

### 1.5 Verify Temporal Stability

**Test**:
- Run test, observe frame dumps
- Frame 1 should be noisy (no history)
- Frame 2+ should be progressively smoother
- Camera is static, so history should be 100% valid

---

## Phase 2: Understand ReSTIR Data Flow (Day 4)

### 2.1 Read the Paper

**Required reading**:
- "Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting" (Bitterli et al., 2020)
- NVIDIA's ReSTIR GI blog post and sample code

### 2.2 Key Insight for This Engine

The existing `FewBounceGI.hlsl` traces ONE ray per bounce per pixel. For ReSTIR:
- Trace `M` candidate rays per pixel (e.g., M=4) in the raygen shader
- Each candidate produces: `direction`, `radiance`, `pdf`, `hitDistance`
- Store candidates in a `RWStructuredBuffer<FCandidate>`
- Run ReSTIR generation compute to select the best candidate using RIS

**This is the ONLY architecturally correct approach.** ReSTIR must control ray generation.

### 2.3 Design the Reservoir Format

```hlsl
struct FReservoir {
    float3 Direction;    // Sampled incoming direction (normalized)
    float  W;            // Unbiased contribution weight
    float  w_sum;        // Sum of weights
    float  M;            // Sample count
    float  pdf;          // Target PDF of selected sample
    float  hitDist;      // Distance to hit point
};
```

**Note**: `Direction` replaces `worldPos`. The reservoir stores HOW light arrives, not WHERE the surface is.

---

## Phase 3: Multi-Sample Ray Generation (Days 5-7)

### 3.1 Modify FewBounceGI.hlsl

**New behavior**:
```hlsl
// RayGen now traces M=4 candidate samples
for (int i = 0; i < NUM_CANDIDATES; i++) {
    // Jitter seed per candidate
    float3 rayDir = sampleHemisphereCosine(normal, random(pixelSeed, i, 0), random(pixelSeed, i, 1));
    // Trace ray
    // Store result in CandidateBuffer[pixel * NUM_CANDIDATES + i]
}
```

**Candidate buffer format**:
```hlsl
struct FCandidate {
    float3 radiance;
    float  pdf;        // Cosine-weighted PDF = dot(normal, direction) / PI
    float3 direction;
    float  hitDist;
};
```

### 3.2 Add Candidate Buffer Output

In `TestFewBounceGI.cpp`:
- Create `CandidateBuffer` as `RWStructuredBuffer` (or `RWBuffer` if structured buffers are problematic)
- Size = `width * height * NUM_CANDIDATES * sizeof(FCandidate)`

### 3.3 Verify Candidates Are Sane

Write a debug visualization shader that reads the candidate buffer and outputs:
- Candidate 0 radiance as RGB
- Or candidate hit distance as grayscale

**Success criteria**: Image shows noisy but coherent GI. More candidates = less noise.

---

## Phase 4: ReSTIR Generation Pass (Days 8-9)

### 4.1 Write ReSTIR_Generate_cs.hlsl (Correct Version)

```hlsl
// Inputs: CandidateBuffer (structured buffer), GBuffer normals, GBuffer depth
// Outputs: Reservoir0Texture (direction + W), Reservoir1Texture (w_sum + M + pdf + hitDist)

// Per pixel:
// 1. Read all NUM_CANDIDATES candidates for this pixel
// 2. Run streaming RIS:
//    selected = candidate[0]
//    w_sum = p_hat(candidate[0]) / candidate[0].pdf
//    for i = 1 to NUM_CANDIDATES-1:
//        p_i = p_hat(candidate[i]) / candidate[i].pdf
//        w_sum += p_i
//        if (random() < p_i / w_sum):
//            selected = candidate[i]
// 3. W = w_sum / (M * selected.pdf)
// 4. Store reservoir
```

Where `p_hat(candidate) = Luminance(candidate.radiance)`.

### 4.2 Verify Generation

Debug visualization modes:
- `W` as grayscale: bright = high weight
- `selected direction` as RGB: map direction to color cube
- `M` as grayscale: should be constant = NUM_CANDIDATES

---

## Phase 5: ReSTIR Temporal Reuse (Days 10-12)

### 5.1 Requirements

- Previous frame's reservoir textures (ping-pong)
- Previous frame's depth + normal (for validation)
- `PrevViewProj` matrix (for reprojection)

### 5.2 Write ReSTIR_Temporal_cs.hlsl (Correct Version)

```hlsl
// Per pixel:
// 1. Load current reservoir R_c
// 2. Reproject pixel to previous frame UV
// 3. Load history reservoir R_h at reprojected UV (nearest neighbor)
// 4. Validate history:
//    - Read prevDepth at UV, compare to current depth at pixel
//    - Read prevNormal at UV, compare to current normal (dot > 0.9)
//    - If invalid: R_h = empty
// 5. Merge reservoirs:
//    - Evaluate R_h's sample at CURRENT pixel:
//      p_hat_h_at_c = p_hat(R_h.direction, currentPixel)
//    - This requires tracing a short ray or using cached radiance
//    - For simplicity: assume p_hat is similar if geometry is similar
//    - M_combined = min(R_c.M + R_h.M, MaxM)
//    - w_sum_combined = R_c.w_sum + R_h.w_sum * (p_hat_h_at_c / R_h.p_hat) * J
//    - Select winner proportionally
//    - W = w_sum_combined / (M_combined * winner.pdf)
// 6. Store merged reservoir
```

### 5.3 The Evaluation Problem

**Hard problem**: To merge a neighbor's reservoir, you need to know what radiance THEIR sample direction would produce at YOUR pixel. This requires either:
- Retracing a ray (expensive)
- Storing radiance in the reservoir (memory cost)
- Approximating (what we do for a basic impl)

**Simplification for first version**:
```hlsl
// Store radiance in reservoir
struct FReservoir {
    float3 Direction;
    float3 Radiance;  // NEW: radiance at the pixel that created this sample
    float  W, w_sum, M, pdf, hitDist;
};

// When merging, use stored radiance as approximation
// This is biased but acceptable for a first implementation
```

### 5.4 Verify Temporal

- Static camera: noise should decrease over frames (M increases)
- Debug vis: `M` should grow from NUM_CANDIDATES toward MaxM
- If camera moves: M should reset to NUM_CANDIDATES at disocclusions

---

## Phase 6: ReSTIR Spatial Reuse (Days 13-15)

### 6.1 Write ReSTIR_Spatial_cs.hlsl (Correct Version)

```hlsl
// Per pixel:
// 1. Load center reservoir R_c
// 2. For each neighbor in 3x3 (or larger) radius:
//    a. Load neighbor reservoir R_n
//    b. Geometric rejection:
//       - Normal dot > threshold
//       - Depth difference < threshold
//       - Plane distance < threshold
//    c. If accepted:
//       - Evaluate R_n at center pixel (use stored radiance)
//       - p_hat_n_at_c = Luminance(R_n.Radiance)  // simplified
//       - Merge into spatial reservoir (same math as temporal)
// 3. Output: selected direction's radiance (from spatial reservoir)
```

### 6.2 Key Difference from Previous Attempt

**Previous**: Spatial pass output `centerRadiance` (original input unchanged).
**Correct**: Spatial pass outputs the radiance of the WINNING sample from the spatial merge. If a neighbor had a better sample, its radiance is used.

### 6.3 Verify Spatial

- Image should be smoother than generation-only
- Edges should be preserved (geometric rejection working)
- Debug vis: show which pixels accepted neighbors vs rejected

---

## Phase 7: Integration and Denoising (Days 16-18)

### 7.1 Final Shading Pass

After spatial reuse, run a compute pass that:
```hlsl
// Inputs: final reservoir texture, GBuffer data
// Output: shaded HDR
// Per pixel:
// 1. Load reservoir
// 2. Output = reservoir.Radiance * BRDF(normal, direction, viewDir)
// 3. Or simply output reservoir.Radiance if already multiplied
```

### 7.2 Add Working Denoiser

**Option A**: Fix ReBLUR
- Add proper reprojection to `ReBLUR_cs.hlsl`
- Clear BOTH history and output textures
- Use previous-frame depth/normal for validation

**Option B**: Use bilateral denoise only
- The bilateral filter already works (after Phase 0 fix)
- Run it after ReSTIR shading
- Simpler, no temporal (but ReSTIR temporal already provides some stability)

**Option C**: Use NRD (if available in the project)
- Check if NVIDIA Real-Time Denoisers is integrated
- If yes, use NRD's ReLAX or ReBLUR diffuser

### 7.3 Verify End-to-End

Pipeline:
```
GBuffer → GI Multi-Sample Ray Trace → ReSTIR Generation → Temporal → Spatial → Shading → Denoise → Blit
```

**Success criteria**:
- Clean, stable image
- Indirect lighting visible (color bleeding, soft shadows)
- Temporal stability: no flicker
- Spatial quality: no neighbor bleeding at edges

---

## Phase 8: Polish and Test (Days 19-20)

### 8.1 Add Debug Visualization Modes

Hotkey-toggleable modes:
- `0`: Final output
- `1`: Raw GI (no ReSTIR)
- `2`: ReSTIR generation only
- `3`: After temporal
- `4`: After spatial
- `5`: M visualization (sample count)
- `6`: W visualization (weight)
- `7`: Rejection mask (red = rejected, green = accepted)

### 8.2 Performance Profiling

- Add `FGPUProfiler` markers to every pass
- Measure: ray trace time, ReSTIR generation, temporal, spatial, denoise
- Target: < 33ms total for 800x600 at 60fps

### 8.3 Automated Testing

**Minimum test**:
```cpp
RECORD_BOOL(test_ReSTIR_GI) {
    // Run for N frames
    // Dump frame N to PNG
    // Load reference PNG
    // Compute PSNR or SSIM
    // PASS if PSNR > 30 dB (loose threshold for noise)
}
```

**Even better**: Human-verified reference images. Check them into git and compare.

### 8.4 Memory Audit

| Texture | Format | Size (800x600) |
|---------|--------|---------------|
| GBuffer (x5) | RGBA16F + RGBA8 + D32 | ~15 MB |
| HDR Output | RGBA32F | ~7.5 MB |
| Candidate Buffer | 32 bytes × 4 × 800 × 600 | ~61 MB |
| Reservoir0 | RGBA16F | ~4 MB |
| Reservoir1 | RGBA16F | ~4 MB |
| History (ping-pong) | RGBA16F × 2 | ~8 MB |
| Prev Depth/Normal | D32 + RGBA16F | ~5 MB |
| Denoised Output | RGBA32F | ~7.5 MB |
| **Total** | | **~112 MB** |

This is acceptable for a test. For production, reduce candidates to 2 or use half precision.

---

## Common Pitfalls to Avoid

### Pitfall 1: ReSTIR as Post-Process

**WRONG**:
```
GI Ray Trace → Denoise → [ReSTIR wrap] → Output
```

**RIGHT**:
```
GI Ray Trace [with multiple candidates] → ReSTIR [selects best candidate] → Shading → Denoise
```

### Pitfall 2: Reservoir Stores World Position

**WRONG**: `Reservoir.y = GBufferWorldPos[pixel]`

**RIGHT**: `Reservoir.direction = sampledRayDirection` or `Reservoir.samplePoint = secondaryHitPoint`

### Pitfall 3: Spatial Pass Outputs Center Pixel's Color

**WRONG**: `output[pixel] = input[pixel]`

**RIGHT**: `output[pixel] = winnerRadiance` where winner may be from neighbor

### Pitfall 4: Temporal Without Previous-Frame Data

**WRONG**: Validate history using current-frame depth at history UV.

**RIGHT**: Maintain `PrevDepthTexture` and `PrevNormalTexture`. Validate using those.

### Pitfall 5: Reprojection Depth in Wrong Space

**WRONG**: `depth * 2.0 - 1.0` for zero-to-one projection.

**RIGHT**: Use `glm::perspectiveLH_ZO` inverse correctly, or use `glm::perspectiveLH_NO` with [-1,1].

### Pitfall 6: Frame Dump Reads Input Instead of Output

**WRONG**: Dump history texture before swap.

**RIGHT**: Dump output texture after dispatch, before swap.

---

## Recommended File Structure

```
TestFewBounceGI_Data/
├── FewBounceGI.hlsl              # Modified: multi-sample raygen
├── GBufferSponzaVS.hlsl          # Unchanged
├── GBufferSponzaPS.hlsl          # Unchanged
├── ReSTIR_Generate_cs.hlsl       # NEW: streaming RIS
├── ReSTIR_Temporal_cs.hlsl       # NEW: temporal merge with validation
├── ReSTIR_Spatial_cs.hlsl        # NEW: spatial merge with geo rejection
├── ReSTIR_Shade_cs.hlsl          # NEW: final shading from reservoir
├── BilateralDenoise_cs.hlsl      # FIXED: .Load() instead of sampler
├── SimpleTemporal_cs.hlsl        # NEW: Phase 1 temporal (optional keep)
└── ShaderMake.cfg                # Updated with new shaders

Runtime/Private/Renderer/PostProcess/
├── FReSTIRPass.cpp/h             # REWRITE: generation + temporal + spatial
├── FSimpleTemporalPass.cpp/h     # NEW: Phase 1 (can be removed later)
└── FBilateralDenoisePass.cpp/h   # Keep (fix shader)

Runtime/Test/
└── TestFewBounceGI.cpp           # REWRITE: clean pipeline
```

---

## Milestones and Go/No-Go Gates

| Phase | Gate | Criteria |
|-------|------|----------|
| 0 | Baseline clean | Frame dumps show coherent Sponza GI, no corruption |
| 1 | Temporal works | Static camera noise reduces over frames |
| 3 | Multi-sample works | Candidate debug vis shows N candidates per pixel |
| 4 | Generation works | W debug vis shows plausible weights, no NaN |
| 5 | Temporal works | M increases over frames, resets on disocclusion |
| 6 | Spatial works | Output smoother than generation, edges preserved |
| 7 | End-to-end | Final image looks like GI, passes PSNR test |
| 8 | Polish | Debug modes, profiling, memory audit complete |

**If any gate fails, STOP and fix before proceeding.**

---

## Final Advice

1. **Start small**: Get single-sample GI working cleanly before adding ReSTIR.
2. **Verify visually EVERY day**: Frame dumps are non-negotiable.
3. **Use debug visualization**: If you can't see what the algorithm is doing, you don't understand it.
4. **Read the paper**: ReSTIR has subtle math. Don't guess.
5. **Don't trust self-reviews**: Have the code render an image and LOOK AT IT.

---

*This roadmap replaces all previous plans. Do not refer to plan.md, plan_7b.md, plan_8.md, or plan_9.md — they describe an architecture that cannot work.*
