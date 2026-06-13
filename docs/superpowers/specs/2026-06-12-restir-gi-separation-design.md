# ReSTIR/GI Architectural Separation

**Status**: Draft (revised)
**Date**: 2026-06-12 (initial) → 2026-06-13 (revised after user review)
**Author**: Claude + user
**Scope**: 1-2 weeks (Week 1: GI extraction; Week 2: ReSTIR rewrite + cleanup)
**Bugs addressed**: bug-046 (ReSTIR GI retrospective), bug-053 (ReBLUR temporal alpha), bug-054 (ReBLUR frame stat validation), and the "checkerboard" / "snow-flower flicker" complaints from the current TestFewBounceGI

---

## Context

`TestFewBounceGI.cpp` has grown to **1,764 lines** and conflates two distinct algorithms:

1. **Multi-bounce path tracing** (Global Illumination) — accumulates radiance from primary rays, bounces, and Russian Roulette.
2. **ReSTIR** (Reservoir-based Spatiotemporal Importance Resampling) — a Direct Illumination technique that selects good light samples from a candidate list.

The current pipeline applies ReSTIR as a **post-process** on the denoised GI output. This is architecturally meaningless: ReSTIR's job is to pick light samples for the first bounce, not to re-weight already-accumulated indirect radiance. The 15-turn ReSTIR GI retrospective (bug-046) spent most of its time chasing flicker caused by feeding wrong inputs to a DI algorithm.

### Symptoms in current output
- "Checkerboard" / dark pixels (analysis: 30% of pixels are dark; stable across frames; not actually a checkerboard pattern — it's Monte Carlo noise that looks structured thanks to deterministic hashing)
- Snow-flower flicker (variance 12.30 in 4-frame analysis before pairwise MIS fix)
- Bimodal radiance distribution (mean ~33/255, 63% black + 35% saturated)

### Why now
- `FBindingLayoutBuilder` and `FRayTracingPipeline` already exist in `Runtime/Renderer/Common/` (committed). The infrastructure to extract features cleanly is in place.
- `FReSTIRPass` already exists in `Runtime/Renderer/PostProcess/` but in the wrong shape (Generate/Temporal/Spatial as separate compute dispatches, operating on denoised GI output). It needs a rewrite, not just a move.
- `FReBLURPass` already exists and works correctly. Both new thin tests will reuse it.
- Project policy (cerebrum.md, 2026-06-07): "no feature lives in a test for more than one iteration" — every feature must be extracted to `Runtime/Renderer/<Feature>/`.

---

## Goals

1. **Two clean reference tests** that demonstrate each algorithm in isolation:
   - `TestPathTraceGI` — multi-bounce GI without ReSTIR
   - `TestReSTIR_DI` — ReSTIR light sampling for direct illumination, no GI
2. **Both features extracted to Runtime/Renderer/**, owned by reusable pass classes with documented public APIs.
3. **No architectural confusion**: ReSTIR operates on light samples, not on denoised GI. The two passes are independent and composable.
4. **Bug-046 regression tests**: pairwise MIS, position-hash stability, W-weighting all locked in by automated tests.
5. **`TestFewBounceGI.cpp` deleted** by end of Week 1 (per "no feature lives in a test for more than one iteration"). The kitchen-sink test outlived its purpose.
6. **Bindless texture support preserved**: `FGIPass` continues to use `FDescriptorTableManager` + bindless `Texture_SRV` for material albedo sampling, matching the current `TestFewBounceGI` implementation (commit `d03a99a`). This is required for Sponza's ~70 unique materials to fit in a single descriptor table.
7. **BilateralDenoise float4 fix verified**: Per the user's prior analysis, the current dark-pixel artifact in dumped frames may be a `Texture2D<float3>` vs `RGBA32_FLOAT` mismatch in `BilateralDenoise_cs.hlsl` (`.bak` file suggests the fix is mid-progress). This is a hard prerequisite for quality gates (Pre-Week 1 Day 0).

## Non-Goals

1. **Not a production renderer.** This is a renderer-tech-demo; no scene file format, no asset import, no editor.
2. **No ReSTIR GI algorithm** (the paper's full version that combines ReSTIR for both first bounce and bounces). That's a follow-up. For now: `TestReSTIR_DI` does DI only; `TestPathTraceGI` does multi-bounce GI without ReSTIR.
3. **No TAA radiance blending in the temporal ReSTIR pass.** Bug-046 noted that "dead bindings" (CurrentRadiance/HistoryRadiance/OutRadiance) mislead readers. If the temporal pass doesn't blend radiance, the bindings must be removed. Decision: temporal pass is **reservoir-only**. Radiance is computed in the spatial pass from the final reservoir.
4. **No new light type beyond StructuredBuffer<FLight>.** ReSTIR samples from a structured list of lights (point/spot/directional/area). No envmap-only path; no implicit lights.
5. **No backward compatibility with old FReSTIRPass API.** The Generate/Temporal/Spatial method names are gone. There is no `FReSTIRPass_Legacy`.

---

## Architecture

```
Engine/Source/Runtime/
├── Renderer/
│   ├── GI/                                     [NEW — Week 1]
│   │   ├── FGIPass.h                           # public API
│   │   ├── FGIPass.cpp                         # orchestration
│   │   └── Shader/                             # moved from TestFewBounceGI_Data/
│   │       ├── GIRayGen.hlsl
│   │       ├── GIClosestHit.hlsl
│   │       ├── GIMiss.hlsl
│   │       └── GIPathTracing.hlsl              # common helper functions
│   ├── ReSTIR/                                 [REWRITTEN — Week 2]
│   │   ├── FReSTIRPass.h                       # in-shader DI; was PostProcess/
│   │   ├── FReSTIRPass.cpp
│   │   └── Shader/
│   │       ├── ReSTIR_DI_RayGen.hlsl
│   │       ├── ReSTIR_DI_ClosestHit.hlsl       # reservoir update in hit shader
│   │       ├── ReSTIR_DI_Miss.hlsl
│   │       └── ReSTIR_Reservoir.hlsl           # shared RIS math
│   ├── PostProcess/
│   │   └── FReBLURPass.{h,cpp}                 # unchanged, reused by both tests
│   ├── Common/
│   │   ├── FBindingLayoutBuilder.h             # unchanged
│   │   └── FRayTracingPipeline.h               # unchanged
│   └── Deferred/
│       └── FDeferredFrameRenderer.{h,cpp}       # register FGIPass + FReSTIRPass
└── Test/
    ├── TestPathTraceGI.cpp                     [NEW — ~200 lines, Week 1]
    ├── TestReSTIR_DI.cpp                       [NEW — ~200 lines, Week 2]
    ├── TestPathTraceGI_Data/                   [NEW — scene + ShaderMake.cfg]
    ├── TestReSTIR_DI_Data/                     [NEW]
    └── TestFewBounceGI.cpp                     [DELETED — end of Week 1]
```

**Key principles:**
- Each pass in Runtime owns its own shaders and binding layouts. Tests are pure composition.
- The `FReSTIRPass` rename (PostProcess/ → ReSTIR/) reflects its new role: RT pipeline with reservoir state, not a screen-space effect.
- `FReBLURPass` stays where it is. It's a post-process that reads a texture and writes a denoised one; the description still fits.

---

## Pass API Contracts

### `FGIPass` (Week 1)

```cpp
// Runtime/Renderer/GI/FGIPass.h
class FGIPass : public nvrhi::RefCounted
{
public:
    bool Initialize(nvrhi::IDevice* device, const FScene* scene,
                    const FGIPassDesc& desc = {});
    void Shutdown();

    // Model A: pass owns scene reference; reads TLAS from scene each frame.
    // DispatchRays takes only the per-frame data, not the TLAS.
    void DispatchRays(nvrhi::ICommandList* cmd,
                      const FGIDispatchParams& params);

    nvrhi::TextureHandle GetOutputTexture() const { return mRadianceTex; }
    const FGIPassStats& GetLastFrameStats() const { return mLastStats; }

    // CVars
    // r_GI.MaxBounces         int    default 4
    // r_GI.SamplesPerPixel    int    default 8
    // r_GI.MinRayLength       float  default 0.001
    // r_GI.EnableRR           bool   default true
    // r_GI.RussianRoulette    float  default 0.95
    // r_GI.DebugBounceStats   bool   default false # gates per-bounce debug UAV
    // Note: any CVar that affects GI output must be read at DispatchRays() time,
    //       not Initialize() time, so runtime changes take effect.

private:
    FRayTracingPipeline mPipeline;
    nvrhi::TextureHandle mRadianceTex;       // RGBA32_FLOAT
    const FScene* mScene = nullptr;          // not owned; reads TLAS from here
    FGIPassStats mLastStats;                 // avg bounces, RR survival rate
};

struct FGIPassDesc
{
    uint32_t maxBounces      = 4;
    uint32_t samplesPerPixel = 8;
    bool enableRR            = true;
    bool useMIS              = true;
};

struct FGIPassStats
{
    float avgBounces;          // mean bounce count across all completed paths
    float rrSurvivalRate;      // mean survival rate across ALL bounces (not at maxBounces)
    uint32_t primaryHits;      // count of primary rays that hit geometry
    uint32_t totalSamples;     // total samples dispatched
    double avgRadiance;        // mean output radiance (for log spam, not for validation)
};

struct FRTDispatchParamsBase
{
    nvrhi::TextureHandle gbufferAlbedo;     // t0
    nvrhi::TextureHandle gbufferNormal;     // t1
    nvrhi::TextureHandle gbufferMaterial;   // t2
    const FViewConstants& viewConstants;    // b0  (const ref, not value; struct must be brace-initialized)
    uint32_t frameIndex;                    // 0-based frame counter, for RNG seeding
};

struct FGIDispatchParams : FRTDispatchParamsBase
{
    // No additional fields. TLAS is read from mScene (set in Initialize).
};
```

**Why this shape:**
- `DispatchRays` (not `Dispatch`) because it's RT.
- GBuffer is the input — same GBuffer as the deferred renderer.
- `FScene*` is the engine's scene handle, owned elsewhere; pass only borrows.
- `mRadianceTex` is internal — exposed via getter so `FReBLURPass` can bind it as `t0`.
- CVar-driven tunables; pass-level config in `FGIPassDesc` is fixed at `Initialize()` time and cannot be changed per-frame. The CVar reads override the struct value at runtime.
- **Stateless across frames**: same scene + same GBuffer → same output. CVar changes affect output but no internal state.

### `FReSTIRPass` (Week 2 — rewrite of existing)

```cpp
// Runtime/Renderer/ReSTIR/FReSTIRPass.h
class FReSTIRPass : public nvrhi::RefCounted
{
public:
    bool Initialize(nvrhi::IDevice* device, const FScene* scene,
                    const FReSTIRPassDesc& desc = {});
    void Shutdown();

    // Model A: pass owns scene reference; reads TLAS from scene each frame.
    void DispatchRays(nvrhi::ICommandList* cmd,
                      const FReSTIRDispatchParams& params);

    // Manual history invalidation. Tests call this on scene changes
    // (camera move, mesh edit, light change).
    void ResetHistory();

    nvrhi::TextureHandle GetOutputTexture() const { return mRadianceTex; }

    // CVars (runtime tunables; init-time config is in FReSTIRPassDesc)
    // r_ReSTIR.M                  int   default 32     # candidates per pixel
    // r_ReSTIR.SpatialRadius      int   default 5      # neighborhood size
    // r_ReSTIR.TemporalAlpha      float default 0.2    # history weight
    // r_ReSTIR.ResetHistory       bool  default false  # hotkey-driven reset
    // Note: enablePairwiseMIS is NOT a CVar. It is init-time only via FReSTIRPassDesc,
    //       and defaults to true. Bug-046 proved pairwise MIS reduces variance 6x;
    //       exposing it as a runtime CVar would invite accidental re-introduction.
    // Note: r_ReSTIR.ResetHistory does NOT auto-clear. The user/test must set it
    //       back to false. The pass logs a warning when ResetHistory fires via CVar
    //       (so the user can see the state transition in console output).

private:
    FRayTracingPipeline mPipeline;
    nvrhi::TextureHandle mReservoirTex[2];   // ping-pong for temporal reuse
    nvrhi::TextureHandle mRadianceTex;       // RGBA32_FLOAT output
    const FScene* mScene = nullptr;          // not owned; reads TLAS from here
    int mHistoryIdx = 0;                     // 0 or 1
};

struct FReSTIRPassDesc
{
    uint32_t candidatesMPerPixel = 32;     // M in the paper
    uint32_t spatialRadius        = 5;       // K neighborhood
    bool enablePairwiseMIS        = true;    // bug-046 fix: default-on
    bool enableTemporal           = true;    // toggle for ablation tests
};

struct FReSTIRDispatchParams : FRTDispatchParamsBase
{
    nvrhi::TextureHandle lightBuffer;       // t4 — StructuredBuffer<FLight>
};
```

**Why this shape differs from the old FReSTIRPass:**
- Old API had `Generate()` / `Temporal()` / `Spatial()` — three calls, post-process compute, no scene access.
- New API: one `DispatchRays` call. Reservoir state lives in `mReservoirTex[2]` (ping-pong). The hit shader does RIS selection, the compute pass does temporal merge, the compute pass does spatial merge. Internally ordered, externally invisible.
- `EnablePairwiseMIS = true` is the default — bug-046 proved naive merge produces 12.3 variance, pairwise MIS produces 2.08. Default-on to prevent regression.
- `ResetHistory()` is the only manual state invalidation API. CVar `r_ReSTIR.ResetHistory` provides hotkey-driven reset for debugging.

### Light representation

`FReSTIRDispatchParams::lightBuffer` is a `StructuredBuffer<FLight>`. The `FLight` struct lives in `Runtime/Renderer/Common/FLight.h` and is shared by FGIPass and FReSTIRPass. **GPU layout matters here** — AGENTS.md notes that `FVec3` is `double`-based, so the C++ struct must use `glm::vec3`/`float3` for GPU compatibility, with explicit padding to match std430 layout:

```cpp
// Runtime/Renderer/Common/FLight.h
// IMPORTANT: This struct must match the HLSL definition exactly (std430 layout).
// Use float3/float4, NOT FVec3 (which is double-based per project conventions).
struct FLight
{
    glm::vec3 position;        // float3 in HLSL; std430 pads to 16 bytes
    uint32_t type;             // 0=point, 1=spot, 2=directional, 3=area
    glm::vec3 direction;       // float3 in HLSL; std430 pads to 16 bytes
    float intensity;           // multiplier
    glm::vec3 color;           // float3 in HLSL; std430 pads to 16 bytes
    float area;                // for area lights
    float cosOuterCone;        // for spot lights; -1 for non-spot
    float cosInnerCone;        // for spot lights; -1 for non-spot
    glm::vec2 _pad0;           // pad to vec4 alignment
};
// Total: 80 bytes (matches HLSL `struct FLight` with cbuffer-style alignment)
```

HLSL counterpart (`Runtime/Shader/Common/FLight.hlsl`):
```hlsl
struct FLight
{
    float3 position;   uint type;
    float3 direction;  float intensity;
    float3 color;      float area;
    float cosOuterCone;
    float cosInnerCone;
    float2 _pad0;
};

StructuredBuffer<FLight> gLights;
```

Sponza's analytic light list (for both thin tests) is **1 sun (directional) + 4 area lights** positioned in the arches. Stored in `TestPathTraceGI_Data/Scene.json` and `TestReSTIR_DI_Data/Scene.json` — these are **ad-hoc test data files**, not a formal engine scene format. Per non-goal #1, no general scene file format is being designed.

---

## Data Flow

### TestPathTraceGI (Week 1)

```
GBuffer Fill Pass
  ├── MRT0: Albedo (RGBA8)
  ├── MRT1: Normal (RG16 + depth in alpha)
  ├── MRT2: Material (RG8: roughness/Metal)
  └── MRT3: WorldPos + ViewDepth
            ↓
FGIPass::DispatchRays(cmd, params)
  ├── RayGen: per-pixel primary ray
  ├── ClosestHit:
  │   ├── Sample GBuffer for material at hit
  │   ├── Sample light dir (cosine-weighted)
  │   ├── Trace shadow ray → contributes
  │   ├── Bounce: sample new dir, trace again
  │   └── RR: terminate by survival probability
  ├── Miss: sky color / envmap
  └── Output: mRadianceTex (RGBA32_FLOAT)
            ↓
FReBLURPass (existing, unchanged)
            ↓
Tonemap + Present (existing)
```

### TestReSTIR_DI (Week 2)

```
GBuffer Fill Pass (same as Week 1)
            ↓
FReSTIRPass::DispatchRays(cmd, params)
  ├── RayGen: per-pixel primary ray
  ├── ClosestHit:
  │   ├── Load GBuffer material
  │   ├── RIS: sample M candidates from lightBuffer
  │   ├── For each: trace shadow, weight by p_hat
  │   └── Update reservoir
  ├── [Temporal compute pass]
  │   └── Blend current reservoir with history
  │       using alpha = max(1/(FrameIndex+1), 1/HistoryFadeIn)
  │       (NOT the bad (FrameIndex-1)/HistoryFadeIn form! — bug-053)
  ├── [Spatial compute pass]
  │   ├── Pairwise MIS over K×K neighborhood
  │   └── W-clamped weighted blend
  └── Output: mRadianceTex (RGBA32_FLOAT)
            ↓
FReBLURPass
            ↓
Tonemap + Present
```

**Two key differences from TestPathTraceGI:**
1. **ClosestHit is the workhorse** — RIS sampling + shadow ray loop lives in the hit shader, not a compute post-process.
2. **Reservoir ping-pong** — `mReservoirTex[0]` and `mReservoirTex[1]` alternate each frame. Temporal pass reads one, writes the other. Spatial pass is single-pass (no history needed).

**Bug-046 hardening baked in:**
- `FrameIndex` in RNG, **not** in position hash (`hash22(pixel + float2(i*7, i*13))`).
- Pairwise MIS default-on in `FReSTIRPassDesc`.
- Reservoir W is applied (clamped [0, 5]), not discarded.
- TAA-style radiance blend: **not present in temporal pass** (it is reservoir-only); spatial pass writes the final radiance.

### Combined pipeline (composition, not a test)

The two thin tests can be composed in any future test:
```
GBuffer → FGIPass (multi-bounce) → FReSTIRPass (DI for first bounce) → FReBLURPass → tonemap
```

This is **not** a third test. It's the natural composition if/when someone needs a "first-bounce ReSTIR + multi-bounce GI" renderer. No need to build it speculatively.

---

## Error Handling

Per project policy: no exceptions. All errors via macros.

**In C++ (FGIPass.cpp / FReSTIRPass.cpp):**
```cpp
HLVM_ASSERT(scene->TLAS != nullptr);
HLVM_ASSERT(mRadianceTex != nullptr);
HLVM_ASSERT_F(desc.maxBounces > 0 && desc.maxBounces <= 16,
              TXT("FGIPass maxBounces out of range: {}"), desc.maxBounces);

HLVM_ENSURE(gbufferAlbedo->getDesc().format == nvrhi::Format::RGBA8_UNORM);
HLVM_ENSURE(lightBuffer != nullptr);

if (!mPipeline.Initialize(...)) {
    HLVM_LOG(LogGI, error, TXT("FGIPass shader compile failed: {}"), err);
    return false;
}
```

**In HLSL:** No try/catch equivalent. Use `if (any(isnan(input))) return float3(0,0,0);` early-outs for robustness. NaN propagation has been a recurring source of flicker (bug-046 was 90% of the ReSTIR fix work, but GI also has NaN risks from bouncing).

**In tests:**
```cpp
RECORD(test_path_trace_gi_sponza_renders) {
    FGIPass pass;
    HLVM_ENSURE(pass.Initialize(...));
    pass.DispatchRays(cmd, params);
    CheckCondition(pass.GetOutputTexture() != nullptr);
}
```

---

## Testing Strategy — 3 Layers

**Layer 1: Pass unit tests (no scene, minimal geometry)**
- Synthetic single-triangle test: validate primary ray, single bounce, RR, MIS.
- "Cube on plane" reference scene (existing at `TestCubeOnPlane.cpp`).
- Frame dump to a known-baseline pixel range, assert within tolerance.

**Layer 2: Scene integration tests (Sponza)**
- `TestPathTraceGI` and `TestReSTIR_DI` use Sponza.
- Frame dump at known camera angles (3 angles: front, side, top).
- Compare against baseline using `mmx vision describe`.

**Layer 3: Quality regression (frame statistics)**

Per bug-054: `RECORD(exit_0)` is not a quality test. Each thin test must validate stats:
```cpp
RECORD(test_path_trace_gi_quality) {
    // Run for 4 frames, dump each
    // Validate:
    //   - 4-frame stdev (radiance): < 10
    //   - Black% (pixel == 0): < 15%
    //   - Saturated% (pixel == 255): < 5%
    //   - Per-channel mean: in expected range (Sponza: R 30-60, G 30-60, B 30-60)
    //   - Tonemap range: all values in [0, 1]
}
```

**Specific quality gates per pass:**

| Pass | Metric | Target | Why |
|------|--------|--------|-----|
| FGIPass | 4-frame stdev | < 10 (with fixed tonemap) | GI should be stable frame-to-frame on static scene |
| FGIPass | Bounce 2 contribution | ≥ 5% of bounce 1 (per-pixel mean) | GI is doing work, not just direct |
| FGIPass | RR survival rate | 30-70% **average across all bounces** | RR is firing but not too aggressively; per-bounce survival should be high near origin and decay toward the end |
| FReSTIRPass | 4-frame stdev | < 5 (with fixed tonemap) | ReSTIR should be more stable than naive |
| FReSTIRPass | Pairwise MIS on/off variance ratio | > 3.0 | Replicates bug-046 lesson (12.3 → 2.08) |
| FReSTIRPass | Temporal convergence | variance after 4 frames with temporal on **≤** variance with temporal off | First frame may be uninitialized; convergence is the steady-state behavior |
| FReSTIRPass | Pixel-diff on static scene | < 0.05 in [0,1] linear radiance (≈ 13/255 in 8-bit sRGB) | Replicates bug-046 (FrameIndex in position hash → flicker) |

**Tonemap gating:** All stdev measurements assume a fixed exposure/tonemap curve in the test. The exact curve is ACES filmic with `Exposure=1.0` (no further adjustment). If the test changes tonemap params, the stdev gates must be re-validated. Tests must set this explicitly:

```cpp
// In test setup:
r_Tonemap.Mode = 1;       // ACES
r_Tonemap.Exposure = 1.0;
r_Tonemap.Gamma = 1.0;
```

**Bounce 2 measurement:** The "Bounce 2 contribution ≥ 5%" gate is measured via an **optional per-bounce debug UAV** in `GIClosestHit.hlsl`, gated by `r_GI.DebugBounceStats` CVar (default false). When enabled, the shader writes to a `RWStructuredBuffer<FBounceStats>` with per-bounce accumulated radiance. Test reads back, asserts the ratio. When the CVar is false (production path), the UAV is unbound and there is zero overhead. This avoids forcing a separate render pass for measurement.

**Pixel-diff units clarified:** "Pixel-diff < 0.05" means the per-pixel L1 distance in [0,1] linear radiance space, between two consecutive frames on a static scene. 0.05 in linear ≈ 13/255 in 8-bit sRGB. The test must:
1. Use a fixed tonemap (above)
2. Compare in linear space (not sRGB), since 8-bit sRGB has gamma compression that hides small differences

**No debug visualizations in shipping shaders** (per bug-046 lesson). `#ifdef DEBUG_VIS` is allowed during dev but must be removed before the iteration ends.

### Bug-046 regression tests (specific)

```cpp
RECORD(test_pairwise_mis_reduces_variance) {
    // Render 4 frames with pairwise MIS off, dump stats
    // Render 4 frames with pairwise MIS on, dump stats
    // Assert: variance_off / variance_on > 3.0
}

RECORD(test_temporal_does_not_use_frameindex_in_position_hash) {
    // Render 2 consecutive frames on static scene
    // Pixel-level diff between frames should be < 0.05
}
```

---

## Migration Plan

### Pre-Week 1 — Denoiser fix (Day 0)

| Day | Task | Acceptance |
|-----|------|------------|
| 0 (Fri) | Verify BilateralDenoise float4 fix is landed in `FReBLURPass`. If not, land it. The `.bak` file at `TestFewBounceGI_Data/BilateralDenoise_cs.hlsl.bak` suggests the fix is mid-progress — check git log. | Quality gates (Layer 3) are unreliable without this fix |

Without this fix, the "4-frame stdev < 10" gate is meaningless — the denoiser is corrupting pixel data before the gate measurement.

### Week 1 — Extract `FGIPass` + thin `TestPathTraceGI`

| Day | Task | Acceptance |
|-----|------|------------|
| 1 (Mon) | Create `Runtime/Renderer/GI/Shader/` directory. Create stub `GIRayGen.hlsl`, `GIClosestHit.hlsl`, `GIMiss.hlsl`, `GIPathTracing.hlsl` (empty body, return black). Create stub `FGIPass.h` with API but no implementation. Add `Runtime/Renderer/GI/Shader/ShaderMake.cfg`. Add `GetCVarManager().LoadAllFromIni()` call to test entry points (bug-052). | Shaders compile (return black). Stub compiles. |
| 2 | Move real shader bodies from `TestFewBounceGI_Data/Shaders/` to `Runtime/Renderer/GI/Shader/`. Implement `FGIPass::Initialize()` and `FGIPass::DispatchRays()` using `FRayTracingPipeline` + `FBindingLayoutBuilder`. | Pass compiles, links, initializes, runs on `TestCubeOnPlane` |
| 3 | Implement multi-bounce path tracing in `GIClosestHit.hlsl` (extracted from current). RR, MIS, bounce accumulation. CVar-driven. Add `r_GI.DebugBounceStats` UAV for measurement. | Pixel output stable on `TestCubeOnPlane`, multi-bounce visible |
| 4 | Create `TestPathTraceGI.cpp` (~200 lines): load Sponza, fill GBuffer, call `FGIPass::DispatchRays`, call `FReBLURPass`, tonemap. Fixed tonemap (ACES, Exposure=1.0). | Sponza renders, GI contribution visible |
| 5 | Quality tests (Layer 2 + 3). **Delete `TestFewBounceGI.cpp` entirely** (do not keep with ReSTIR stripped — there's no point in a GI-only test when `TestPathTraceGI` exists). | 4-frame stdev < 10, Bounce 2 contribution ≥ 5%, build passes |

**Week 1 milestone**: `TestPathTraceGI` is the GI reference. `TestFewBounceGI.cpp` deleted.

### Week 2 — Rewrite `FReSTIRPass` + thin `TestReSTIR_DI` + final cleanup

| Day | Task | Acceptance |
|-----|------|------------|
| 6 (Mon) | Move `Runtime/Renderer/PostProcess/FReSTIRPass.{h,cpp}` to `Runtime/Renderer/ReSTIR/FReSTIRPass.{h,cpp}`. Delete old Generate/Temporal/Spatial methods. Create `Runtime/Renderer/ReSTIR/Shader/` with stub shaders. | Old methods gone, file compiles |
| 7 | Implement in-shader RIS in `ReSTIR_DI_ClosestHit.hlsl`. Single-frame output (no temporal, no spatial). Pairwise MIS default-on via init-time flag. `StructuredBuffer<FLight>` from `Runtime/Renderer/Common/FLight.h`. | Pipeline compiles, links, single-frame Sponza DI visible |
| 8 | Implement temporal compute pass. Bug-046 + bug-053 fix: alpha = `max(1/(FrameIndex+1), 1/HistoryFadeIn)`. `FrameIndex` in RNG only, **never** in position hash. | Temporal pass converges (variance after 4 frames ≤ without temporal) |
| 9 | Implement spatial compute pass with pairwise MIS over K×K neighborhood. W-clamped weighted blend. Spatial pass writes the final radiance. | Full ReSTIR DI pipeline works end-to-end |
| 10 | Create `TestReSTIR_DI.cpp` (~200 lines). Sponza analytic light list (1 sun + 4 area lights). Fixed tonemap. | Sponza renders, DI dominant, all 3 quality gates pass |
| 11 | Quality tests (Layer 2 + 3) for ReSTIR. Variance ratio, temporal convergence, position-hash stability. | All 3 quality gates pass |
| 12 | **Contingent**: if `FDeferredFrameRenderer` already exists, register `FGIPass` and `FReSTIRPass` with the pass registry. Otherwise, document the integration point in a `Runtime/Renderer/Deferred/INTEGRATION.md` file. Update `CLAUDE.md` and `.wolf/cerebrum.md` to reflect new module layout and add bug-046 regression test. | All tests pass Debug + RelWithDebInfo |

**Week 2 milestone**: Two clean reference tests, both extracted to Runtime, kitchen-sink deleted, regression test in place.

---

## Risk Register

| Risk | Mitigation |
|------|------------|
| GBuffer format mismatch between GBuffer fill and FGIPass/FReSTIRPass | HLVM_ENSURE in both passes; document the contract in FGIPassDesc |
| `FrameIndex` regression in ReSTIR position hash | Bug-046 regression test (pixel-diff < 0.05 on static scene) |
| Pairwise MIS off by default (would re-introduce bug-046) | `FReSTIRPassDesc::enablePairwiseMIS = true` hardcoded; no public setter |
| Sponza KTX loader issues (cerebrum: KTX1+ASTC, not KTX2) | Use PNG/JPG Sponza; same as existing `TestSponzaDeferred` |
| Sponza 0.008 node scale (cerebrum: tiny mesh bug) | Apply 2x ModelMatrix scale (matches existing fix) |
| Shader rebuild not detected (cerebrum: delete .sblob to force) | Document in CLAUDE.md / .wolf/cerebrum.md |
| Debug visualizations left in shipping shaders (bug-046) | `#ifdef DEBUG_VIS` only; CI grep for `DEBUG_VIS` in commit-merged branches |
| Old FReSTIRPass users in tests elsewhere (e.g. future ReBLUR integration) | Grep for `FReSTIRPass::` callers; update all to new API |
| GLight struct changes break ABI | All callers within HLVM; no external ABI. Add a single header `Runtime/Renderer/Common/FLight.h` |

---

## Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-06-12 | Approach A: Incremental extraction (TestPathTraceGI first) | Matches user's instinct; bug-046 lesson ("validate GI before adding ReSTIR"); each week ends with a verifiable milestone |
| 2026-06-12 | Rewrite FReSTIRPass in place (delete old Generate/Temporal/Spatial methods) | Old API encodes the wrong architecture. Keeping it would create two ReSTIR modules with different semantics. |
| 2026-06-12 | Delete TestFewBounceGI (originally Week 2; superseded 2026-06-13: end of Week 1) | Per project policy "no feature lives in a test for more than one iteration"; thin tests are the references; .bak is on disk for archaeology |
| 2026-06-12 | `StructuredBuffer<FLight>` for ReSTIR candidate sampling | ReSTIR's whole point is to sample from many candidates. Analytic constants don't exercise the algorithm. |
| 2026-06-12 | Manual `ResetHistory()` API + `r_ReSTIR.ResetHistory` CVar | Predictable, debuggable, no magic. Hash-based auto-detect hides failures. |
| 2026-06-12 | CVar prefixes: `r_GI.*` and `r_ReSTIR.*` | Two new CVar groups, INI sections `[GI]` and `[ReSTIR]`. Clear separation, easy to toggle a whole algorithm. |
| 2026-06-12 | Module layout: `Runtime/Renderer/{GI,ReSTIR,PostProcess,Common,Deferred}` | Two new top-level modules. Clean separation by algorithm. |
| 2026-06-12 | Temporal pass is reservoir-only; spatial pass writes radiance | Removes "dead bindings" (bug-046). Single source of truth for radiance: the final reservoir. |
| 2026-06-13 | TLAS ownership: Model A (pass owns `FScene*` ref, reads TLAS from scene each frame) | Pass class is the natural unit of scene-resource ownership. `DispatchRays` signature stays simple. Per-frame TLAS changes are common in dynamic scenes. |
| 2026-06-13 | Shared `FRTDispatchParamsBase` for GI and ReSTIR params | Both passes bind identical GBuffer + view constants + frame index. The only difference is `lightBuffer` (ReSTIR-only). Inheritance is cleaner than duplication. |
| 2026-06-13 | `FViewConstants` by `const&` in params struct | Avoids ~64-byte value copy. Struct must be brace-initialized. C++20 designated initializers work. |
| 2026-06-13 | Bindless texture arrays preserved in `FGIPass` | Sponza has ~70 unique materials; bindless is required for a single descriptor table. Established in commit `d03a99a`. |
| 2026-06-13 | BilateralDenoise float4 fix is a hard prerequisite (Pre-Week 1 Day 0) | Without it, the stdev quality gates are unreliable. The `.bak` file indicates the fix is mid-progress. |
| 2026-06-13 | `FReBLURPass` must consume external `nvrhi::TextureHandle` (not always allocate its own) | Both new tests compose the pass with an upstream GI/DI output. The existing pass API must already support this; if not, the writing-plans phase should add a `Dispatch(cmd, inputTex, outputTex)` overload. |
| 2026-06-13 | `r_ReSTIR.ResetHistory` does NOT auto-clear | Auto-clearing hides state transitions and confuses users debugging. Pass logs a warning when the CVar triggers reset, so users can see the transition in console. |
| 2026-06-13 | `FGIPass` / `FReSTIRPass` take `const FScene*` (not `FSceneResourceManager&`) | Spec uses raw `FScene*` for consistency with the existing `FReSTIRPass` API. If `FSceneResourceManager` is the project's preferred scene-lifetime abstraction, the writing-plans phase should switch both passes to take a reference to it instead. This is a low-priority cleanup, not a blocker. |
| 2026-06-13 | CVar INI loading must be explicit in test entry points (bug-052) | `GetCVarManager().LoadAllFromIni()` is not called automatically. Tests that rely on `Engine.ini` overrides must call this in their entry point. Added to Week 1 Day 1. |
| 2026-06-13 | RR survival rate measured as **average across all bounces**, not at `maxBounces` | Per-bounce survival should be high near the origin and decay toward the end. Averaging captures the algorithm's behavior, not the worst-case tail. |
| 2026-06-13 | Temporal convergence is a steady-state property, not a frame-1-to-4 slope | First frame may be uninitialized. Quality gate compares variance after 4 frames with temporal on vs off, not a 50% drop in 4 frames. |
| 2026-06-13 | Stdev gates assume a fixed tonemap (ACES, Exposure=1.0) | Tonemap parameters affect stdev measurements. Tests must set the tonemap explicitly to make stdev gates reproducible. |
| 2026-06-13 | Bounce 2 contribution measured via `r_GI.DebugBounceStats`-gated UAV | Avoids forcing a separate render pass. The UAV is unbound in the production path; zero overhead. |
| 2026-06-13 | `TestFewBounceGI.cpp` deleted at end of Week 1, not Week 2 | Once ReSTIR is stripped, the file is a redundant GI test. `TestPathTraceGI` is the GI reference. No point in keeping both. |

---

## Definition of Done

**For `FGIPass`:**
- [ ] Lives in `Runtime/Renderer/GI/`
- [ ] Has unit tests (Layer 1) — passes
- [ ] `TestPathTraceGI` shows Sponza with visible GI
- [ ] Quality stats: stdev < 10, bounce 2 contribution ≥ 5%
- [ ] CVars: `r_GI.MaxBounces`, `r_GI.SamplesPerPixel`, `r_GI.EnableRR`
- [ ] No debug visualizations in shipping code
- [ ] Code reviewed via `code-reviewer` skill

**For `FReSTIRPass`:**
- [ ] Lives in `Runtime/Renderer/ReSTIR/`
- [ ] Old Generate/Temporal/Spatial methods deleted
- [ ] `TestReSTIR_DI` shows Sponza with dominant DI
- [ ] Quality stats: stdev < 5, variance ratio > 3.0 vs naive, temporal convergence (variance after 4 frames with temporal on ≤ variance with temporal off)
- [ ] Bug-046 regression test: pixel-diff < 0.05 on static scene
- [ ] CVars: `r_ReSTIR.M`, `r_ReSTIR.SpatialRadius`, `r_ReSTIR.TemporalAlpha`, `r_ReSTIR.ResetHistory`
- [ ] Code reviewed via `code-reviewer` skill

**For `TestFewBounceGI`:**
- [ ] File deleted
- [ ] Build passes without it
- [ ] Reference still available as `TestFewBounceGI.cpp.bak` and `TestFewBounceGI_Data/` git history

---

## References

- Bug-046: ReSTIR GI retrospective (cerebrum.md, 2026-06-06) — 15 turns debugging flicker caused by treating ReSTIR as a GI post-process
- Bug-054: ReBLUR frame stat validation (cerebrum.md, 2026-06-08) — `RECORD(exit_0)` is not a quality test
- Project policy (cerebrum.md, 2026-06-07): "no feature lives in a test for more than one iteration"
- Renderer MVP boundary (cerebrum.md, 2026-06-07): ReSTIR+ReBLUR+FSR2 in scope; scene file loading out of scope
- Next Session Priority (cerebrum.md, 2026-06-08): CVars for GI tunables, refactor TestFewBounceGI to use new APIs, then pass registry
- `FBindingLayoutBuilder` (Public/Renderer/Common/) — eliminates manual binding math
- `FRayTracingPipeline` (Public/Renderer/RayTracing/) — encapsulates RT pipeline boilerplate
- `FReBLURPass` (Public/Renderer/PostProcess/) — denoise, reused by both new tests
