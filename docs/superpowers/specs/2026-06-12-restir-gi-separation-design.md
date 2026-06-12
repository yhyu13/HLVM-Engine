# ReSTIR/GI Architectural Separation

**Status**: Draft
**Date**: 2026-06-12
**Author**: Claude + user
**Scope**: 1-2 weeks (Week 1: GI extraction; Week 2: ReSTIR rewrite + cleanup)
**Bugs addressed**: bug-046 (ReSTIR GI retrospective), bug-054 (ReBLUR frame stat validation), and the "checkerboard" / "snow-flower flicker" complaints from the current TestFewBounceGI

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
5. **`TestFewBounceGI.cpp` deleted** by end of Week 2. The kitchen-sink test outlived its purpose.

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
    └── TestFewBounceGI.cpp                     [DELETED — end of Week 2]
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

private:
    FRayTracingPipeline mPipeline;
    nvrhi::TextureHandle mRadianceTex;       // RGBA32_FLOAT
    FGIPassStats mLastStats;                 // avg bounces, RR survival rate
};

struct FGIPassDesc
{
    uint32_t maxBounces      = 4;
    uint32_t samplesPerPixel = 8;
    bool enableRR            = true;
    bool useMIS              = true;
};

struct FGIDispatchParams
{
    nvrhi::TextureHandle gbufferAlbedo;     // t0
    nvrhi::TextureHandle gbufferNormal;     // t1
    nvrhi::TextureHandle gbufferMaterial;   // t2
    FViewConstants viewConstants;           // b0
    uint32_t frameIndex;                    // for RNG seeding
    FRayTracingAccelStruct tlas;            // t3
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

    void DispatchRays(nvrhi::ICommandList* cmd,
                      const FReSTIRDispatchParams& params);

    void ResetHistory();  // Call on scene change (camera move, mesh edit, light change)

    nvrhi::TextureHandle GetOutputTexture() const { return mRadianceTex; }

    // CVars (runtime tunables; init-time config is in FReSTIRPassDesc)
    // r_ReSTIR.M                  int   default 32     # candidates per pixel
    // r_ReSTIR.SpatialRadius      int   default 5      # neighborhood size
    // r_ReSTIR.TemporalAlpha      float default 0.2    # history weight
    // r_ReSTIR.ResetHistory       bool  default false  # hotkey-driven reset
    // Note: enablePairwiseMIS is NOT a CVar. It is init-time only via FReSTIRPassDesc,
    //       and defaults to true. Bug-046 proved pairwise MIS reduces variance 6x;
    //       exposing it as a runtime CVar would invite accidental re-introduction.

private:
    FRayTracingPipeline mPipeline;
    nvrhi::TextureHandle mReservoirTex[2];   // ping-pong for temporal reuse
    nvrhi::TextureHandle mRadianceTex;       // RGBA32_FLOAT output
    int mHistoryIdx = 0;                     // 0 or 1
};

struct FReSTIRPassDesc
{
    uint32_t candidatesMPerPixel = 32;     // M in the paper
    uint32_t spatialRadius        = 5;       // K neighborhood
    bool enablePairwiseMIS        = true;    // bug-046 fix: default-on
    bool enableTemporal           = true;    // toggle for ablation tests
};

struct FReSTIRDispatchParams
{
    nvrhi::TextureHandle gbufferAlbedo;     // t0
    nvrhi::TextureHandle gbufferNormal;     // t1
    nvrhi::TextureHandle gbufferMaterial;   // t2
    FViewConstants viewConstants;           // b0
    uint32_t frameIndex;                    // for RNG seeding (NEVER in position hash — bug-046)
    FRayTracingAccelStruct tlas;            // t3
    nvrhi::TextureHandle lightBuffer;       // t4 — StructuredBuffer<FLight>
};
```

**Why this shape differs from the old FReSTIRPass:**
- Old API had `Generate()` / `Temporal()` / `Spatial()` — three calls, post-process compute, no scene access.
- New API: one `DispatchRays` call. Reservoir state lives in `mReservoirTex[2]` (ping-pong). The hit shader does RIS selection, the compute pass does temporal merge, the compute pass does spatial merge. Internally ordered, externally invisible.
- `EnablePairwiseMIS = true` is the default — bug-046 proved naive merge produces 12.3 variance, pairwise MIS produces 2.08. Default-on to prevent regression.
- `ResetHistory()` is the only manual state invalidation API. CVar `r_ReSTIR.ResetHistory` provides hotkey-driven reset for debugging.

### Light representation

`FReSTIRDispatchParams::lightBuffer` is a `StructuredBuffer<FLight>`. The `FLight` struct lives in `Runtime/Renderer/Common/FLight.h` and is shared by FGIPass and FReSTIRPass. At minimum:

```cpp
struct FLight
{
    FVec3 position;       // for point/spot
    FVec3 direction;      // for directional
    FVec3 color;          // linear RGB intensity
    float intensity;      // multiplier
    float cosOuterCone;   // for spot lights; -1 for non-spot
    float cosInnerCone;   // for spot lights; -1 for non-spot
    uint32_t type;        // 0=point, 1=spot, 2=directional, 3=area
    float area;           // for area lights
};
```

Sponza's analytic light list (for both thin tests) is **1 sun (directional) + 4 area lights** positioned in the arches. Defined in `TestPathTraceGI_Data/Scene.json` and `TestReSTIR_DI_Data/Scene.json`.

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
| FGIPass | 4-frame stdev | < 10 | GI should be stable frame-to-frame on static scene |
| FGIPass | Bounce 2 contribution | ≥ 5% of bounce 1 | GI is doing work, not just direct |
| FGIPass | RR survival rate | 30-70% at maxBounces | RR is firing but not too aggressively |
| FReSTIRPass | 4-frame stdev | < 5 | ReSTIR should be more stable than naive |
| FReSTIRPass | Pairwise MIS on/off variance ratio | > 3.0 | Replicates bug-046 lesson (12.3 → 2.08) |
| FReSTIRPass | Temporal convergence | variance ↓ 50% in 4 frames | TAA-style stability test |
| FReSTIRPass | Pixel-diff on static scene | < 0.05 | Replicates bug-046 (FrameIndex in position hash → flicker) |

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

### Week 1 — Extract `FGIPass` + thin `TestPathTraceGI`

| Day | Task | Acceptance |
|-----|------|------------|
| 1 (Mon) | Move `TestFewBounceGI_Data/Shaders/*GI*` to `Runtime/Shader/GI/`. Add `Runtime/Shader/GI/ShaderMake.cfg`. | Shaders compile, .sblob files generated |
| 2 | Create `FGIPass.h` with API. Implement `Initialize()` and `DispatchRays()` using `FRayTracingPipeline` + `FBindingLayoutBuilder`. | Pass compiles, links, initializes |
| 3 | Implement multi-bounce path tracing in `GIClosestHit.hlsl` (extracted from current). RR, MIS, bounce accumulation. CVar-driven. | Pixel output stable on `TestCubeOnPlane` |
| 4 | Create `TestPathTraceGI.cpp` (~200 lines): load Sponza, fill GBuffer, call `FGIPass::DispatchRays`, call `FReBLURPass`, tonemap. | Sponza renders, GI contribution visible |
| 5 | Quality tests (Layer 2 + 3). Delete ReSTIR code from `TestFewBounceGI.cpp`. | 4-frame stdev < 10, Bounce 2 contribution ≥ 5% |

**Week 1 milestone**: `TestPathTraceGI` is the GI reference. `TestFewBounceGI` still exists but has ReSTIR stripped.

### Week 2 — Rewrite `FReSTIRPass` + thin `TestReSTIR_DI` + delete kitchen-sink

| Day | Task | Acceptance |
|-----|------|------------|
| 6 (Mon) | Move `ReSTIR/PostProcess/FReSTIRPass.{h,cpp}` to `ReSTIR/ReSTIR/FReSTIRPass.{h,cpp}`. Delete old Generate/Temporal/Spatial code. | Old methods gone, file compiles |
| 7 | Implement new `FReSTIRPass::DispatchRays` with single-call API. In-shader RIS in `ReSTIR_DI_ClosestHit.hlsl`. Pairwise MIS default-on. | Pipeline compiles, links |
| 8 | Implement temporal compute pass with bug-046 fix (alpha = `max(1/(F+1), 1/FadeIn)`). Implement spatial compute pass with pairwise MIS. | Both passes work in isolation |
| 9 | Create `TestReSTIR_DI.cpp` (~200 lines). `StructuredBuffer<FLight>` for Sponza. Sponza analytic light list (sun + 4 area lights for the arches). | Sponza renders, DI dominant |
| 10 | Quality tests (Layer 2 + 3) for ReSTIR. Variance ratio, temporal convergence, position-hash stability. | All 3 quality gates pass |
| 11 | Delete `TestFewBounceGI.cpp` and `TestFewBounceGI_Data/`. Update `Build.sh` / CMakeLists. | Build passes without TestFewBounceGI |
| 12 | Update `FDeferredFrameRenderer` to register `FGIPass` and `FReSTIRPass`. Write design doc updates. Bug-046 regression test. | All tests pass Debug + RelWithDebInfo |

**Week 2 milestone**: Two clean reference tests, both extracted to Runtime, kitchen-sink deleted.

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
| 2026-06-12 | Delete TestFewBounceGI in Week 2 | Per project policy "no feature lives in a test for more than one iteration"; thin tests are the references; .bak is on disk for archaeology |
| 2026-06-12 | `StructuredBuffer<FLight>` for ReSTIR candidate sampling | ReSTIR's whole point is to sample from many candidates. Analytic constants don't exercise the algorithm. |
| 2026-06-12 | Manual `ResetHistory()` API + `r_ReSTIR.ResetHistory` CVar | Predictable, debuggable, no magic. Hash-based auto-detect hides failures. |
| 2026-06-12 | CVar prefixes: `r_GI.*` and `r_ReSTIR.*` | Two new CVar groups, INI sections `[GI]` and `[ReSTIR]`. Clear separation, easy to toggle a whole algorithm. |
| 2026-06-12 | Module layout: `Runtime/Renderer/{GI,ReSTIR,PostProcess,Common,Deferred}` | Two new top-level modules. Clean separation by algorithm. |
| 2026-06-12 | Temporal pass is reservoir-only; spatial pass writes radiance | Removes "dead bindings" (bug-046). Single source of truth for radiance: the final reservoir. |

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
- [ ] Quality stats: stdev < 5, variance ratio > 3.0 vs naive, temporal convergence ↓ 50%
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
