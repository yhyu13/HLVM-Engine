# ReSTIR/GI Architectural Separation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract multi-bounce GI into `FGIPass` and rewrite `FReSTIRPass` as in-shader DI, creating two thin reference tests (`TestPathTraceGI`, `TestReSTIR_DI`) and deleting the kitchen-sink `TestFewBounceGI.cpp`.

**Architecture:** Two new pass classes in `Runtime/Renderer/{GI,ReSTIR}/` (extracted from `TestFewBounceGI`). Each pass owns its shaders and binding layouts. Tests are pure composition: GBuffer fill → pass → `FReBLURPass` → tonemap. Bug-046 (ReSTIR GI flicker) and bug-053 (temporal alpha) hardening baked in.

**Tech Stack:** C++20, NVRHI/Vulkan, HLSL→SPIR-V via ShaderMake, `FRayTracingPipeline` + `FBindingLayoutBuilder` (existing), Sponza scene, structured frame-dump validation.

**Spec:** [2026-06-12-restir-gi-separation-design.md](../specs/2026-06-12-restir-gi-separation-design.md)

---

## File Structure

### Files to create

| Path | Responsibility |
|------|----------------|
| `Engine/Source/Runtime/Public/Renderer/Common/FLight.h` | `FLight` struct (C++) for `StructuredBuffer<FLight>` |
| `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` | `FGIPass` public API |
| `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` | `FGIPass` orchestration |
| `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIRayGen.hlsl` | Ray generation |
| `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIClosestHit.hlsl` | Multi-bounce path tracing |
| `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIMiss.hlsl` | Sky/envmap miss |
| `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` | Common helper functions |
| `Engine/Source/Runtime/Private/Renderer/Shader/GI/ShaderMake.cfg` | Shader build config |
| `Engine/Source/Runtime/Private/Renderer/Shader/Common/FLight.hlsl` | HLSL `FLight` definition |
| `Engine/Source/Runtime/Public/Renderer/ReSTIR/FReSTIRPass.h` | ReSTIR public API |
| `Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp` | ReSTIR orchestration |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_RayGen.hlsl` | ReSTIR ray gen |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_ClosestHit.hlsl` | In-shader RIS |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_Miss.hlsl` | Miss |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Reservoir.hlsl` | Shared RIS math |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Temporal_cs.hlsl` | Temporal merge compute |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Spatial_cs.hlsl` | Spatial merge compute |
| `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ShaderMake.cfg` | Shader build config |
| `Engine/Source/Runtime/Test/TestPathTraceGI.cpp` | GI reference test |
| `Engine/Source/Runtime/Test/TestPathTraceGI_Data/Scene.json` | Ad-hoc test data (1 sun + 4 area lights) |
| `Engine/Source/Runtime/Test/TestReSTIR_DI.cpp` | DI reference test |
| `Engine/Source/Runtime/Test/TestReSTIR_DI_Data/Scene.json` | Ad-hoc test data |
| `Engine/Source/Runtime/Test/CMakeLists.txt` (modify) | Add new tests, remove TestFewBounceGI |

### Files to delete

| Path | Reason |
|------|--------|
| `Engine/Source/Runtime/Test/TestFewBounceGI.cpp` | Kitchen-sink, end of Week 1 |
| `Engine/Source/Runtime/Test/TestFewBounceGI.cpp.bak` (if still on disk) | Stale backup |
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/` (entire dir) | Source of moved shaders/data |
| `Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h` | Replaced by ReSTIR/ version |
| `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp` | Replaced by ReSTIR/ version |
| `Engine/Source/Runtime/Test/TestFewBounceGI_Data/BilateralDenoise_cs.hlsl.bak` | Cleanup after Day 0 verification |

### Files to modify

| Path | Change |
|------|--------|
| `Engine/Source/Runtime/CMakeLists.txt` | Add `GI/`, `ReSTIR/`, `Common/FLight.h`; remove `PostProcess/FReSTIRPass.{h,cpp}` |
| `Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h` | Verify external-texture overload exists |
| `.wolf/cerebrum.md` | Append new decision log entries; mark bug-046/053 regression tests |
| `CLAUDE.md` | Add `r_GI.*` and `r_ReSTIR.*` CVar list reference |

---

## Phase 0: Prerequisites (Day 0)

### Task 0.1: Verify BilateralDenoise fix is landed

**Files:**
- Read: `Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h` (to find the denoiser kernel)
- Possibly modify: `Engine/Source/Runtime/Private/Renderer/Shader/PostProcess/BilateralDenoise_cs.hlsl`

- [ ] **Step 1: Inspect the .bak to find the original bug**

Run: `git log --all --oneline -- Engine/Source/Runtime/Test/TestFewBounceGI_Data/BilateralDenoise_cs.hlsl.bak`
Expected: at least one commit showing the float4 fix work.

- [ ] **Step 2: Read current BilateralDenoise_cs.hlsl**

Run: `grep -n "Texture2D" Engine/Source/Runtime/Private/Renderer/Shader/PostProcess/BilateralDenoise_cs.hlsl`
Expected: One of these patterns:
- ✅ `Texture2D<float4>` (correct, fix landed)
- ❌ `Texture2D<float3>` (bug still present — go to Step 3)

- [ ] **Step 3: If the bug is present, apply the fix**

The fix: change `Texture2D<float3>` to `Texture2D<float4>` in the input binding. The current shader reads 3 components but the bound texture is `RGBA32_FLOAT` (4 components). Sample code:

```hlsl
// BilateralDenoise_cs.hlsl — Input section
// BEFORE (buggy):
Texture2D<float3> gInput;

// AFTER (fixed):
Texture2D<float4> gInput;
```

Update any variable declarations that consume the texture:
```hlsl
// BEFORE:
float3 radiance = gInput.Load(int3(pixel, 0)).rgb;

// AFTER:
float4 radiance = gInput.Load(int3(pixel, 0));
// Use .rgb where appropriate, or just .rgb for color
```

- [ ] **Step 4: Rebuild and run TestSponzaDeferred to verify the fix**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestSponzaDeferred --Test`
Expected: Test passes, frame dump shows fewer black pixels (mean luminance should be > 50/255, not the bimodal 33/255 we saw before).

- [ ] **Step 5: Commit (only if Step 3 modified the file)**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/PostProcess/BilateralDenoise_cs.hlsl
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "fix(postprocess): BilateralDenoise input Texture2D<float3> -> Texture2D<float4>

Matches RGBA32_FLOAT source format. Was reading 3 components of 4-component
texture, causing 1/4 of bits to be garbage and producing bimodal black
output. Source: bug-046 / TestFewBounceGI dark-pixel investigation.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 0.2: Remove the .bak file (cleanup)

**Files:**
- Delete: `Engine/Source/Runtime/Test/TestFewBounceGI_Data/BilateralDenoise_cs.hlsl.bak`

- [ ] **Step 1: Delete the .bak (keep the live file intact)**

Run: `rm Engine/Source/Runtime/Test/TestFewBounceGI_Data/BilateralDenoise_cs.hlsl.bak`

- [ ] **Step 2: Commit the cleanup**

```bash
git add -u Engine/Source/Runtime/Test/TestFewBounceGI_Data/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "chore: remove BilateralDenoise .bak after verifying fix landed"
```

### Task 0.3: Create FLight struct (shared header)

**Files:**
- Create: `Engine/Source/Runtime/Public/Renderer/Common/FLight.h`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/Common/FLight.hlsl`

- [ ] **Step 1: Create the C++ header**

```cpp
// Engine/Source/Runtime/Public/Renderer/Common/FLight.h
#pragma once

#include "Core/Types.h"
#include "glm/glm.hpp"

// IMPORTANT: This struct must match the HLSL definition in
// Runtime/Renderer/Shader/Common/FLight.hlsl exactly.
// Use glm::vec3/float3, NOT FVec3 (which is double-based per project conventions).
// std430 layout: each vec3 pads to 16 bytes.
struct FLight
{
    glm::vec3 position;        // 16 bytes (vec3 + 4 padding to next vec4)
    uint32_t type;             //  4 bytes (0=point, 1=spot, 2=directional, 3=area)
    glm::vec3 direction;       // 16 bytes
    float intensity;           //  4 bytes
    glm::vec3 color;           // 16 bytes
    float area;                //  4 bytes
    float cosOuterCone;        //  4 bytes
    float cosInnerCone;        //  4 bytes
    glm::vec2 _pad0;           //  8 bytes (pad to vec4 alignment)
};
// Total: 80 bytes

enum class ELightType : uint32_t
{
    Point       = 0,
    Spot        = 1,
    Directional = 2,
    Area        = 3,
};
```

- [ ] **Step 2: Verify it compiles**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes (the new header is not yet used by anything but must compile in isolation).

- [ ] **Step 3: Create the HLSL counterpart**

```hlsl
// Engine/Source/Runtime/Private/Renderer/Shader/Common/FLight.hlsl
#ifndef HLVM_FLIGHT_HLSL
#define HLVM_FLIGHT_HLSL

struct FLight
{
    float3 position;   uint type;
    float3 direction;  float intensity;
    float3 color;      float area;
    float cosOuterCone;
    float cosInnerCone;
    float2 _pad0;
};

#endif
```

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Public/Renderer/Common/FLight.h
git add Engine/Source/Runtime/Private/Renderer/Shader/Common/FLight.hlsl
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): add FLight struct (C++ + HLSL) for ReSTIR StructuredBuffer

std430 layout, glm::vec3/float3 (NOT FVec3 — that's double-based).
Used by FReSTIRPass for in-shader light candidate sampling.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Phase 1: GI Extraction (Week 1)

### Task 1.1: Create FGIPass.h skeleton

**Files:**
- Create: `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h`

- [ ] **Step 1: Create the public header with the full API (no implementation yet)**

```cpp
// Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h
#pragma once

#include "Core/Types.h"
#include "Core/RefCounted.h"
#include "Renderer/Common/FLight.h"
#include "nvrhi/nvrhi.h"
#include <glm/glm.hpp>

struct FViewConstants;
class FScene;
class FRayTracingPipeline;

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
    float rrSurvivalRate;      // mean survival rate across ALL bounces
    uint32_t primaryHits;      // count of primary rays that hit geometry
    uint32_t totalSamples;     // total samples dispatched
    double avgRadiance;        // mean output radiance (logging only)
};

struct FRTDispatchParamsBase
{
    nvrhi::TextureHandle gbufferAlbedo;     // t0
    nvrhi::TextureHandle gbufferNormal;     // t1
    nvrhi::TextureHandle gbufferMaterial;   // t2
    const FViewConstants& viewConstants;    // b0 (const ref; brace-initialize)
    uint32_t frameIndex;                    // 0-based frame counter for RNG
};

struct FGIDispatchParams : FRTDispatchParamsBase
{
    // No additional fields. TLAS read from FScene passed to Initialize.
};

class FGIPass : public nvrhi::RefCounted
{
public:
    bool Initialize(nvrhi::IDevice* device, const FScene* scene,
                    const FGIPassDesc& desc = {});
    void Shutdown();

    // Model A: pass owns scene reference; reads TLAS from scene each frame.
    void DispatchRays(nvrhi::ICommandList* cmd, const FGIDispatchParams& params);

    nvrhi::TextureHandle GetOutputTexture() const { return mRadianceTex; }
    const FGIPassStats& GetLastFrameStats() const { return mLastStats; }

private:
    FRayTracingPipeline* mPipeline = nullptr; // owned via Pimpl if needed
    nvrhi::TextureHandle mRadianceTex;        // RGBA32_FLOAT
    nvrhi::BufferHandle mStatsBuffer;         // readback for stats
    const FScene* mScene = nullptr;
    FGIPassDesc mDesc;
    FGIPassStats mLastStats;
};
```

- [ ] **Step 2: Commit (header-only, doesn't need to compile yet — it's referenced by the .cpp in Task 1.2)**

```bash
git add Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): FGIPass public API skeleton (no impl)

Includes FGIPassDesc, FGIPassStats, FRTDispatchParamsBase,
FGIDispatchParams. Model A TLAS ownership (pass owns FScene*).
CVars r_GI.* defined in implementation file.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.2: Create stub FGIPass.cpp

**Files:**
- Create: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`

- [ ] **Step 1: Create the implementation file with stub methods**

```cpp
// Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
#include "Renderer/GI/FGIPass.h"
#include "Core/Memory.h"
#include "Core/CVar.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_STATIC(LogGI, info)

AUTO_CVAR_INT(r_GI.MaxBounces, 4, "Max bounces per GI ray", Saved)
AUTO_CVAR_INT(r_GI.SamplesPerPixel, 8, "SPP for GI primary rays", Saved)
AUTO_CVAR_FLOAT(r_GI.MinRayLength, 0.001f, "Min ray length (epsilon)", Saved)
AUTO_CVAR_BOOL(r_GI.EnableRR, true, "Enable Russian Roulette termination", Saved)
AUTO_CVAR_FLOAT(r_GI.RussianRoulette, 0.95f, "RR survival probability threshold", Saved)
AUTO_CVAR_BOOL(r_GI.DebugBounceStats, false, "Write per-bounce debug UAV (gates r.GI.DebugBounceStats UAV binding)", Saved)

bool FGIPass::Initialize(nvrhi::IDevice* device, const FScene* scene, const FGIPassDesc& desc)
{
    HLVM_ASSERT(device != nullptr);
    HLVM_ASSERT_F(desc.maxBounces > 0 && desc.maxBounces <= 16,
                  TXT("FGIPass maxBounces out of range: {}"), desc.maxBounces);
    mScene = scene;
    mDesc = desc;
    // Pipeline + texture allocation in Task 1.4
    HLVM_LOG(LogGI, info, TXT("FGIPass::Initialize (stub)"));
    return true;
}

void FGIPass::Shutdown()
{
    mRadianceTex = nullptr;
    mStatsBuffer = nullptr;
    mScene = nullptr;
    mPipeline = nullptr;
}

void FGIPass::DispatchRays(nvrhi::ICommandList* cmd, const FGIDispatchParams& params)
{
    HLVM_ENSURE(params.gbufferAlbedo != nullptr);
    // Pipeline dispatch in Task 1.4
    mLastStats = FGIPassStats{0.0f, 0.0f, 0, 0, 0.0};
}
```

- [ ] **Step 2: Verify it compiles**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. The stub doesn't render anything (output is null); existing tests should still pass.

- [ ] **Step 3: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): FGIPass stub implementation with CVar definitions

CVars: r_GI.MaxBounces, SamplesPerPixel, MinRayLength, EnableRR,
RussianRoulette, DebugBounceStats. No pipeline yet — that comes in 1.4.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.3: Create stub GI shaders (return black)

**Files:**
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/GI/ShaderMake.cfg`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIRayGen.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIClosestHit.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIMiss.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`

- [ ] **Step 1: Create ShaderMake.cfg**

```ini
; Engine/Source/Runtime/Private/Renderer/Shader/GI/ShaderMake.cfg
[options]
entry = main
profile = lib_6_7

[make]
; Use lib_6_7 + explicit export names so we can mix ray tracing + ray gen in one blob
shader0 0 * vsMain vs vs_6_7
```

- [ ] **Step 2: Create GIRayGen.hlsl stub**

```hlsl
// Engine/Source/Runtime/Private/Renderer/Shader/GI/GIRayGen.hlsl
// STUB — replaced in Task 1.5 with real ray gen.
[shader("raygeneration")]
void main()
{
    // No-op stub. Returns black.
}
```

- [ ] **Step 3: Create GIClosestHit.hlsl stub**

```hlsl
// Engine/Source/Runtime/Private/Renderer/Shader/GI/GIClosestHit.hlsl
// STUB — replaced in Task 1.5 with multi-bounce path tracing.
[shader("closesthit")]
void main(inout float radiance : SV_RayPayload)
{
    radiance = 0.0;
}
```

- [ ] **Step 4: Create GIMiss.hlsl stub**

```hlsl
// Engine/Source/Runtime/Private/Renderer/Shader/GI/GIMiss.hlsl
// STUB — replaced in Task 1.5 with sky/envmap miss.
[shader("miss")]
void main(inout float radiance : SV_RayPayload)
{
    radiance = 0.0;
}
```

- [ ] **Step 5: Create GIPathTracing.hlsl stub**

```hlsl
// Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
// STUB — replaced in Task 1.5 with common helper functions
// (sampleCosineHemisphere, applyBRDF, RR survival, etc.)
#pragma once
// Empty for now.
```

- [ ] **Step 6: Verify the shaders compile**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. The shaders compile to SPIR-V but the pass doesn't use them yet.

- [ ] **Step 7: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/GI/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): GI shader stubs (return black)

GIRayGen, GIClosestHit, GIMiss, GIPathTracing + ShaderMake.cfg.
Real shader bodies land in Task 1.5 (moved from TestFewBounceGI).

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.4: Wire FGIPass to use the pipeline

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`

- [ ] **Step 1: Replace the Initialize() stub with real pipeline construction**

The Initialize() body should:
- Allocate `mRadianceTex` (RGBA32_FLOAT, screen size)
- Allocate `mStatsBuffer` (small readback buffer for FGIPassStats)
- Construct `mPipeline` using `FRayTracingPipeline::InitializeFromLibrary()`
- Build binding layout via `FBindingLayoutBuilder`: b0=viewConstants, t0=albedo, t1=normal, t2=material, t3=TLAS, u0=output (RGBA32_FLOAT UAV), u1=stats UAV (gated by DebugBounceStats)
- Build binding set via `FBindingSetBuilder` with the same shift math

```cpp
// In Initialize(), after the CVar reads:
mRadianceTex = device->createTexture(nvrhi::TextureDesc()
    .setWidth(width).setHeight(height)
    .setFormat(nvrhi::Format::RGBA32_FLOAT)
    .setUsage(nvrhi::TextureUsage::ShaderResource | nvrhi::TextureUsage::UnorderedAccess)
    .setIsUAV(true), "FGIPass.mRadianceTex");

mStatsBuffer = device->createBuffer(nvrhi::BufferDesc()
    .setByteSize(sizeof(FGIPassStatsGPU))
    .setUsage(nvrhi::BufferUsage::ShaderResource | nvrhi::BufferUsage::UnorderedAccess)
    .setCpuAccess(nvrhi::CpuAccessMode::Read), "FGIPass.mStatsBuffer");

// Use FRayTracingPipeline + FBindingLayoutBuilder (see existing test for pattern)
mPipeline = MakePimpl<FRayTracingPipeline>();
mPipeline->InitializeFromLibrary(device, shaderLib, "main", "main", "main");
// ... build binding layout with .AddConstantBuffer(0).AddTextureSRV(0)...
// ... add TLAS via .AddRayTracingAccelStruct(3)...
// ... add output UAV via .AddTextureUAV(0)...
mPipeline->FinalizePipeline(sizeof(float), sizeof(float2));
mPipeline->BuildShaderTable();
```

- [ ] **Step 2: Replace DispatchRays() stub with real dispatch**

```cpp
void FGIPass::DispatchRays(nvrhi::ICommandList* cmd, const FGIDispatchParams& params)
{
    HLVM_ENSURE(params.gbufferAlbedo != nullptr);
    HLVM_ENSURE(mScene != nullptr);
    HLVM_ENSURE(mScene->GetTLAS() != nullptr);

    // Update binding set with per-frame data
    auto bindingSet = mBindingSetBuilder
        .AddConstantBuffer(0, &mViewConstantsBuffer)  // updated from params.viewConstants
        .AddTextureSRV(0, params.gbufferAlbedo)
        .AddTextureSRV(1, params.gbufferNormal)
        .AddTextureSRV(2, params.gbufferMaterial)
        .AddRayTracingAccelStruct(3, mScene->GetTLAS())
        .AddTextureUAV(0, mRadianceTex)
        .Finalize(cmd);

    mPipeline->DispatchRays(cmd, bindingSet, screenWidth, screenHeight);
    // Readback mStatsBuffer to fill mLastStats (sync if needed; async if perf matters)
}
```

- [ ] **Step 3: Verify compilation**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. Pipeline constructs and dispatches, but output is black (stubs).

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): FGIPass pipeline + binding layout

Allocates mRadianceTex (RGBA32_FLOAT) and mStatsBuffer.
Uses FRayTracingPipeline + FBindingLayoutBuilder pattern.
Stubs return black; real shader bodies come in Task 1.5.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.5: Move real shader bodies

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIRayGen.hlsl` (replace stub)
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIClosestHit.hlsl` (replace stub)
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIMiss.hlsl` (replace stub)
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (replace stub)

- [ ] **Step 1: Read current TestFewBounceGI shader bodies**

Run: `ls Engine/Source/Runtime/Test/TestFewBounceGI_Data/Shaders/`
Expected: Files like `FewBounceGI_RayGen.hlsl`, `FewBounceGI_ClosestHit.hlsl`, etc.

- [ ] **Step 2: Read each file to understand its current structure**

For each: `cat Engine/Source/Runtime/Test/TestFewBounceGI_Data/Shaders/<File>.hlsl`
Read enough to understand: (a) inputs/bindings used, (b) algorithm body, (c) output format.

- [ ] **Step 3: Move shader bodies**

For each file in the GI/Shader/ stub set, replace its body with the equivalent from `TestFewBounceGI_Data/Shaders/`. The bindings must match the binding layout built in Task 1.4:
- `b0` = view constants
- `t0` = GBuffer albedo
- `t1` = GBuffer normal
- `t2` = GBuffer material
- `t3` = TLAS
- `u0` = output radiance (RGBA32_FLOAT UAV)
- `u1` = stats UAV (debug-only, gated by `r_GI.DebugBounceStats`)

If the existing FewBounceGI shader uses different bindings (which is likely — it was using the old PostProcess-style bindings), update them to match the new layout.

- [ ] **Step 4: Verify on TestCubeOnPlane**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. (TestCubeOnPlane doesn't use FGIPass yet, but the shaders must compile.)

- [ ] **Step 5: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/GI/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): move GI shader bodies from TestFewBounceGI

Replaces stubs with real multi-bounce path tracing. Bindings updated to
match FGIPass binding layout (b0, t0-t3, u0, u1).

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.6: Implement multi-bounce path tracing

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIClosestHit.hlsl`
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`

- [ ] **Step 1: Implement helper functions in GIPathTracing.hlsl**

```hlsl
// GIPathTracing.hlsl
#pragma once

// Cosine-weighted hemisphere sample
float3 SampleCosineHemisphere(float3 normal, uint rngState)
{
    float r1 = Rand(rngState);
    float r2 = Rand(rngState);
    float phi = 2.0 * 3.14159265 * r1;
    float r = sqrt(r2);
    float3 local = float3(r * cos(phi), r * sin(phi), sqrt(1.0 - r2));
    return localToWorld(local, normal);
}

// Russian Roulette survival
bool RussianRoulette(float3 throughput, uint rngState, float threshold)
{
    float p = max(throughput.r, max(throughput.g, throughput.b));
    return Rand(rngState) < min(p / threshold, 1.0);
}

// BRDF evaluation (Lambert for now; PBR later)
float3 EvalBRDF(float3 albedo, float metallic, float roughness, float3 wi, float3 wo, float3 n)
{
    return albedo / 3.14159265; // Lambert
}
```

- [ ] **Step 2: Implement ClosestHit with multi-bounce + RR**

```hlsl
// GIClosestHit.hlsl
[shader("closesthit")]
void main(inout FGIPayload payload, Attributes attrib)
{
    // 1. Load material from GBuffer (bindless)
    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 albedo = gbufferAlbedo.Load(int3(payload.pixel, 0)).rgb;
    float3 normal = normalize(gbufferNormal.Load(int3(payload.pixel, 0)).xyz * 2.0 - 1.0);
    float roughness = gbufferMaterial.Load(int3(payload.pixel, 0)).r;
    float metallic = gbufferMaterial.Load(int3(payload.pixel, 0)).g;

    // 2. Direct lighting (next-event estimation)
    if (payload.bounce == 0) {
        float3 lightDir = SampleCosineHemisphere(normal, payload.rngState);
        if (TraceShadowRay(worldPos, lightDir)) {
            payload.radiance += payload.throughput * EvalBRDF(albedo, metallic, roughness, lightDir, -WorldRayDirection(), normal) * lightIntensity;
        }
    }

    // 3. Bounce or terminate
    if (payload.bounce >= CVarMaxBounces ||
        (CVarEnableRR && !RussianRoulette(payload.throughput, payload.rngState, CVarRussianRoulette))) {
        return;
    }

    // 4. Spawn new ray for next bounce
    float3 newDir = SampleCosineHemisphere(normal, payload.rngState);
    payload.bounce++;
    payload.throughput *= EvalBRDF(albedo, metallic, roughness, newDir, -WorldRayDirection(), normal);
    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, worldPos, 0.001, newDir, 1e30, 0, payload);
}
```

- [ ] **Step 3: Implement r_GI.DebugBounceStats UAV write**

When `CVarDebugBounceStats` is true, after the loop:
```hlsl
if (CVarDebugBounceStats) {
    uint bounceIdx = min(payload.bounce, 7);
    uint prev;
    InterlockedAdd(gBounceStats[bounceIdx].radianceSum, pack_radiance(payload.radiance), prev);
    InterlockedAdd(gBounceStats[bounceIdx].count, 1, prev);
}
```

- [ ] **Step 4: Verify on TestCubeOnPlane**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. (TestCubeOnPlane still uses its own pipeline; the GI shaders must compile cleanly.)

- [ ] **Step 5: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/GI/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): multi-bounce path tracing in GIClosestHit

SampleCosineHemisphere + RussianRoulette + EvalBRDF helpers.
Bounce loop with CVarMaxBounces termination and CVarEnableRR.
r_GI.DebugBounceStats UAV write for quality-gate measurement.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.7: Add r_GI.DebugBounceStats binding in FGIPass

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/GI/ShaderMake.cfg`

- [ ] **Step 1: Add the binding layout entry for the debug UAV (gated)**

In `Initialize()`, conditionally bind `u1` (debug stats UAV) based on `CVar_r_GI_DebugBounceStats`:
```cpp
if (CVar_r_GI_DebugBounceStats) {
    builder.AddBufferUAV(1, mStatsBuffer);
}
```

- [ ] **Step 2: Update ShaderMake.cfg to define both uRegShift and a no-uav binding for production**

Production path: `u0` is output radiance, no `u1`. Debug path: `u0` + `u1`. The cleanest approach is to always bind `u1` to a null UAV in production (zero cost) and rebind to mStatsBuffer when DebugBounceStats is true. Or use `#ifdef` in the shader.

- [ ] **Step 3: Verify on TestCubeOnPlane**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes.

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
git add Engine/Source/Runtime/Private/Renderer/Shader/GI/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): r_GI.DebugBounceStats binding (gated)

Adds u1 binding layout entry, conditional on CVar_r_GI_DebugBounceStats.
Used by quality tests for the 'bounce 2 contribution >= 5%' gate.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.8: Create TestPathTraceGI.cpp

**Files:**
- Create: `Engine/Source/Runtime/Test/TestPathTraceGI.cpp`
- Create: `Engine/Source/Runtime/Test/TestPathTraceGI_Data/Scene.json` (ad-hoc, 1 sun + 4 area lights)
- Modify: `Engine/Source/Runtime/Test/CMakeLists.txt`

- [ ] **Step 1: Pre-flight: verify FReBLURPass has external-texture overload**

Before writing the test, check:
```bash
grep -n "Dispatch" Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h
```
Expected: a `Dispatch(cmd, inputTex, outputTex)` overload (decision 2026-06-13). If not present, add one before continuing.

- [ ] **Step 2: Write TestPathTraceGI.cpp skeleton**

```cpp
// Engine/Source/Runtime/Test/TestPathTraceGI.cpp
#include "Test.h"
#include "Renderer/GI/FGIPass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/SceneGraph/FScene.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Logging/LogMacros.h"
#include "Core/CVar.h"

DECLARE_LOG_CATEGORY(LogTestPathTraceGI)

RECORD(test_path_trace_gi_smoke, true) {
    HLVM_LOG(LogTestPathTraceGI, info, TXT("TestPathTraceGI smoke test"));

    // 1. Load Sponza scene
    FScene scene;
    HLVM_ENSURE(scene.LoadFromGLTF(TXT("Sponza/Sponza.gltf")));

    // 2. Set up GBuffer fill pass (reuse FDeferredFrameRenderer)
    FDeferredFrameRenderer renderer;
    HLVM_ENSURE(renderer.Initialize(...));

    // 3. Create FGIPass
    FGIPass giPass;
    HLVM_ENSURE(giPass.Initialize(nvrhiDevice, &scene));

    // 4. Create FReBLURPass
    FReBLURPass denoise;
    HLVM_ENSURE(denoise.Initialize(nvrhiDevice));

    // 5. Set fixed tonemap (per spec quality-gate requirement)
    r_Tonemap.Mode = 1;       // ACES
    r_Tonemap.Exposure = 1.0;
    r_Tonemap.Gamma = 1.0;
    GetCVarManager().LoadAllFromIni(); // bug-052

    // 6. Run for 4 frames, dump each
    for (uint32_t frame = 0; frame < 4; frame++) {
        auto cmd = device->createCommandList();
        renderer.RenderGBuffer(cmd, &scene);
        giPass.DispatchRays(cmd, params);
        denoise.Dispatch(cmd, giPass.GetOutputTexture());
        denoise.DumpToPNG(TEXT("TestPathTraceGI_frame{}.png").format(frame));
        device->executeCommandList(cmd);
    }

    CheckCondition(giPass.GetOutputTexture() != nullptr);
}
```

- [ ] **Step 3: Create Scene.json with 1 sun + 4 area lights**

```json
{
  "lights": [
    {"type": "directional", "direction": [0.3, -1.0, 0.2], "color": [1.0, 0.95, 0.85], "intensity": 3.0},
    {"type": "area", "position": [-5.0, 4.0, 0.0], "color": [1.0, 0.8, 0.6], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [5.0, 4.0, 0.0], "color": [0.6, 0.8, 1.0], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [0.0, 4.0, -5.0], "color": [0.8, 1.0, 0.7], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [0.0, 4.0, 5.0], "color": [0.9, 0.7, 1.0], "intensity": 1.5, "area": 2.0}
  ]
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

Append to the test list:
```cmake
add_hlvm_test(TestPathTraceGI SOURCES TestPathTraceGI.cpp)
```

- [ ] **Step 5: Build and run**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestPathTraceGI --Test --DumpFrames=4`
Expected: Sponza renders, GI contribution visible in dumped frames.

- [ ] **Step 6: Commit**

```bash
git add Engine/Source/Runtime/Test/TestPathTraceGI.cpp
git add Engine/Source/Runtime/Test/TestPathTraceGI_Data/Scene.json
git add Engine/Source/Runtime/Test/CMakeLists.txt
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(test): TestPathTraceGI thin reference test (~200 lines)

Loads Sponza, fills GBuffer, dispatches FGIPass, denoises, tonemaps.
Fixed ACES tonemap (r_Tonemap.Mode=1, Exposure=1.0). Dumps 4 frames.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.9: Add quality tests for FGIPass

**Files:**
- Modify: `Engine/Source/Runtime/Test/TestPathTraceGI.cpp`

- [ ] **Step 1: Add the quality RECORD blocks**

```cpp
RECORD(test_path_trace_gi_quality, true) {
    // Run for 4 frames, dump each, validate stats
    std::vector<FrameStats> stats;
    for (uint32_t frame = 0; frame < 4; frame++) {
        // ... run frame, dump, collect stats ...
        stats.push_back(ComputeFrameStats(dump));
    }

    // 4-frame stdev < 10 (with fixed ACES tonemap)
    float stdev = ComputeStdev(stats);
    CheckCondition(stdev < 10.0f);

    // Bounce 2 contribution >= 5% of bounce 1
    r_GI.DebugBounceStats.SetValue(true);
    // ... run frame, readback stats ...
    float bounce2Ratio = bounce2Radiance / bounce1Radiance;
    CheckCondition(bounce2Ratio >= 0.05f);
    r_GI.DebugBounceStats.SetValue(false);

    // RR survival rate 30-70% average across all bounces
    float avgSurvival = avg_rr_survival_across_bounces;
    CheckCondition(avgSurvival >= 0.30f && avgSurvival <= 0.70f);
}
```

- [ ] **Step 2: Build and run**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestPathTraceGI --Test --DumpFrames=4`
Expected: All RECORD tests pass.

- [ ] **Step 3: Commit**

```bash
git add Engine/Source/Runtime/Test/TestPathTraceGI.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "test(gi): quality gates for FGIPass

- 4-frame stdev < 10 (with fixed ACES tonemap)
- Bounce 2 contribution >= 5% (via r_GI.DebugBounceStats)
- RR survival rate 30-70% avg across all bounces

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 1.10: Delete TestFewBounceGI.cpp and TestFewBounceGI_Data/

**Files:**
- Delete: `Engine/Source/Runtime/Test/TestFewBounceGI.cpp`
- Delete: `Engine/Source/Runtime/Test/TestFewBounceGI.cpp.bak` (if still on disk)
- Delete: `Engine/Source/Runtime/Test/TestFewBounceGI_Data/` (entire dir)
- Modify: `Engine/Source/Runtime/Test/CMakeLists.txt`

- [ ] **Step 1: Remove from CMakeLists.txt**

Edit `CMakeLists.txt` and remove the `add_hlvm_test(TestFewBounceGI ...)` line.

- [ ] **Step 2: Delete the files**

```bash
git rm Engine/Source/Runtime/Test/TestFewBounceGI.cpp
git rm Engine/Source/Runtime/Test/TestFewBounceGI.cpp.bak
git rm -r Engine/Source/Runtime/Test/TestFewBounceGI_Data/
```

- [ ] **Step 3: Build and verify**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestPathTraceGI --Test`
Expected: Build passes. The kitchen-sink test is gone; `TestPathTraceGI` is the GI reference.

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Test/CMakeLists.txt
git commit -m "refactor(test): delete TestFewBounceGI kitchen-sink

Per project policy 'no feature lives in a test for more than one
iteration'. TestPathTraceGI is the GI reference. .bak files removed
along with the live file. Features are extracted to Runtime/Renderer/GI/.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Phase 2: ReSTIR Rewrite (Week 2)

### Task 2.1: Move FReSTIRPass to ReSTIR/ and delete old methods

**Files:**
- Create: `Engine/Source/Runtime/Public/Renderer/ReSTIR/FReSTIRPass.h`
- Create: `Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp`
- Delete: `Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h`
- Delete: `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp`
- Modify: `Engine/Source/Runtime/CMakeLists.txt`

- [ ] **Step 1: Create new FReSTIRPass.h with the new API (from spec Section 3)**

```cpp
// Engine/Source/Runtime/Public/Renderer/ReSTIR/FReSTIRPass.h
#pragma once

#include "Core/Types.h"
#include "Core/RefCounted.h"
#include "Renderer/Common/FLight.h"
#include "Renderer/GI/FGIPass.h"  // for FRTDispatchParamsBase
#include "nvrhi/nvrhi.h"

class FScene;
class FRayTracingPipeline;

struct FReSTIRPassDesc
{
    uint32_t candidatesMPerPixel = 32;
    uint32_t spatialRadius        = 5;
    bool enablePairwiseMIS        = true;  // bug-046: default-on
    bool enableTemporal           = true;
};

struct FReSTIRDispatchParams : FRTDispatchParamsBase
{
    nvrhi::BufferHandle lightBuffer;  // t4 — StructuredBuffer<FLight>
};

class FReSTIRPass : public nvrhi::RefCounted
{
public:
    bool Initialize(nvrhi::IDevice* device, const FScene* scene, const FReSTIRPassDesc& desc = {});
    void Shutdown();

    // Model A: pass owns scene reference; reads TLAS from scene each frame.
    void DispatchRays(nvrhi::ICommandList* cmd, const FReSTIRDispatchParams& params);

    void ResetHistory();

    nvrhi::TextureHandle GetOutputTexture() const { return mRadianceTex; }

private:
    FRayTracingPipeline* mPipeline = nullptr;
    nvrhi::TextureHandle mReservoirTex[2];  // ping-pong
    nvrhi::TextureHandle mRadianceTex;      // RGBA32_FLOAT output
    const FScene* mScene = nullptr;
    FReSTIRPassDesc mDesc;
    int mHistoryIdx = 0;
};
```

- [ ] **Step 2: Create stub FReSTIRPass.cpp with empty body and CVar defs**

```cpp
// Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp
#include "Renderer/ReSTIR/FReSTIRPass.h"
#include "Core/Memory.h"
#include "Core/CVar.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_STATIC(LogReSTIR, info)

AUTO_CVAR_INT(r_ReSTIR.M, 32, "ReSTIR candidates per pixel", Saved)
AUTO_CVAR_INT(r_ReSTIR.SpatialRadius, 5, "ReSTIR spatial neighborhood radius", Saved)
AUTO_CVAR_FLOAT(r_ReSTIR.TemporalAlpha, 0.2f, "ReSTIR temporal blend weight", Saved)
AUTO_CVAR_BOOL(r_ReSTIR.ResetHistory, false, "Reset ReSTIR history on next DispatchRays (does NOT auto-clear)", Saved)

bool FReSTIRPass::Initialize(nvrhi::IDevice* device, const FScene* scene, const FReSTIRPassDesc& desc)
{
    HLVM_ASSERT(device != nullptr);
    mScene = scene;
    mDesc = desc;
    HLVM_LOG(LogReSTIR, info, TXT("FReSTIRPass::Initialize (stub)"));
    return true;
}

void FReSTIRPass::Shutdown() { /* ... */ }
void FReSTIRPass::DispatchRays(nvrhi::ICommandList* cmd, const FReSTIRDispatchParams& params) { /* stub */ }
void FReSTIRPass::ResetHistory() { mHistoryIdx = 0; }
```

- [ ] **Step 3: Delete old FReSTIRPass files**

```bash
git rm Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h
git rm Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp
```

- [ ] **Step 4: Update CMakeLists.txt**

Move FReSTIRPass from PostProcess/ to ReSTIR/.

- [ ] **Step 5: Build to verify nothing else references the old FReSTIRPass**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes. (If any other code references old FReSTIRPass, the build will fail; fix those callers.)

- [ ] **Step 6: Commit**

```bash
git add Engine/Source/Runtime/Public/Renderer/ReSTIR/
git add Engine/Source/Runtime/Private/Renderer/ReSTIR/
git add -u Engine/Source/Runtime/Public/Renderer/PostProcess/
git add Engine/Source/Runtime/CMakeLists.txt
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "refactor(renderer): move FReSTIRPass to ReSTIR/, rewrite API

Old Generate/Temporal/Spatial methods deleted. New single-call
DispatchRays API. CVars: r_ReSTIR.M, SpatialRadius, TemporalAlpha,
ResetHistory. Pairwise MIS init-time only via FReSTIRPassDesc.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.2: Create stub ReSTIR shaders

**Files:**
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ShaderMake.cfg`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_RayGen.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_ClosestHit.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_Miss.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Reservoir.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Temporal_cs.hlsl`
- Create: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Spatial_cs.hlsl`

- [ ] **Step 1: Create all shader stubs**

For each shader file, write a stub that compiles and does nothing:
- RayGen: `[shader("raygeneration")] void main() {}`
- ClosestHit: `[shader("closesthit")] void main(inout float r : SV_RayPayload) { r = 0; }`
- Miss: `[shader("miss")] void main(inout float r : SV_RayPayload) { r = 0; }`
- Reservoir: `#pragma once` empty
- Temporal: `[numthreads(8,8,1)] void main() {}`
- Spatial: `[numthreads(8,8,1)] void main() {}`

- [ ] **Step 2: Create ShaderMake.cfg**

```ini
[options]
entry = main
profile = lib_6_7

[make]
shader0 0 * vsMain vs vs_6_7
```

- [ ] **Step 3: Verify compilation**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes.

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): ReSTIR shader stubs

RayGen, ClosestHit, Miss, Reservoir helpers, Temporal + Spatial compute.
Real implementations in Tasks 2.3-2.5.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.3: Implement in-shader RIS (single-frame)

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Reservoir.hlsl`
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_DI_ClosestHit.hlsl`
- Modify: `Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp`

- [ ] **Step 1: Implement reservoir helpers**

```hlsl
// ReSTIR_Reservoir.hlsl
#pragma once

struct Reservoir
{
    uint lightIdx;      // selected light index
    float wSum;         // weight sum
    float W;            // normalized weight
    float3 lightDir;    // direction to selected light
    float3 radiance;    // contribution of selected light
    uint sampleCount;   // number of candidates seen
};

void ReservoirInit(out Reservoir r)
{
    r.lightIdx = 0;
    r.wSum = 0;
    r.W = 0;
    r.lightDir = float3(0, 0, 0);
    r.radiance = float3(0, 0, 0);
    r.sampleCount = 0;
}

void ReservoirUpdate(inout Reservoir r, uint candidateIdx, float3 candidateDir,
                     float3 candidateRadiance, float targetPdf, uint rngState)
{
    float weight = luminance(candidateRadiance) / max(targetPdf, 1e-6);
    r.wSum += weight;
    r.sampleCount++;
    if (Rand(rngState) * r.wSum < weight)
    {
        r.lightIdx = candidateIdx;
        r.lightDir = candidateDir;
        r.radiance = candidateRadiance;
    }
    r.W = weight / max(r.wSum, 1e-6);
}
```

- [ ] **Step 2: Implement ClosestHit with RIS sampling**

```hlsl
// ReSTIR_DI_ClosestHit.hlsl
[shader("closesthit")]
void main(inout FReSTIRPayload payload, Attributes attrib)
{
    Reservoir r;
    ReservoirInit(r);

    uint rngState = payload.rngState;
    uint M = CVarM;
    StructuredBuffer<FLight> lights = gLights;

    for (uint i = 0; i < M; i++)
    {
        // Sample candidate from light buffer
        uint lightIdx = Rand(rngState) % lights.Length;
        FLight light = lights[lightIdx];

        // Compute direction to light
        float3 lightDir = light.type == 2
            ? -light.direction
            : normalize(light.position - payload.worldPos);

        // Trace shadow ray, compute contribution
        float3 contribution = float3(0, 0, 0);
        if (TraceShadowRay(payload.worldPos, lightDir))
        {
            contribution = EvalBRDF(albedo, metallic, roughness, lightDir, -WorldRayDirection(), normal) * light.color * light.intensity;
        }

        // Reservoir update
        float targetPdf = 1.0 / lights.Length;  // uniform
        ReservoirUpdate(r, lightIdx, lightDir, contribution, targetPdf, rngState);
    }

    payload.reservoir = r;
    payload.rngState = rngState;
}
```

- [ ] **Step 3: Wire up pipeline in FReSTIRPass.cpp**

```cpp
// In Initialize():
mReservoirTex[0] = device->createTexture(...);
mReservoirTex[1] = device->createTexture(...);
mRadianceTex = device->createTexture(...);  // RGBA32_FLOAT

mPipeline = MakePimpl<FRayTracingPipeline>();
mPipeline->InitializeFromLibrary(device, shaderLib, "main", "main", "main");
mPipeline->CreateBindingLayout()
    .AddConstantBuffer(0)         // b0 view constants
    .AddTextureSRV(0).AddTextureSRV(1).AddTextureSRV(2)  // t0-t2 GBuffer
    .AddRayTracingAccelStruct(3)  // t3 TLAS
    .AddBufferSRV(4)              // t4 StructuredBuffer<FLight>
    .AddTextureUAV(0)             // u0 output radiance
    .AddTextureUAV(1);            // u1 reservoir
mPipeline->FinalizePipeline(sizeof(FReSTIRPayload), sizeof(float2));
mPipeline->BuildShaderTable();

// In DispatchRays():
if (CVar_r_ReSTIR_ResetHistory) {
    mHistoryIdx = 0;
    HLVM_LOG(LogReSTIR, warning, TXT("ReSTIR history reset via CVar"));
}

auto bindingSet = mBindingSetBuilder
    .AddConstantBuffer(0, ...)
    .AddTextureSRV(0, params.gbufferAlbedo)
    .AddTextureSRV(1, params.gbufferNormal)
    .AddTextureSRV(2, params.gbufferMaterial)
    .AddRayTracingAccelStruct(3, mScene->GetTLAS())
    .AddBufferSRV(4, params.lightBuffer)
    .AddTextureUAV(0, mRadianceTex)
    .AddTextureUAV(1, mReservoirTex[mHistoryIdx])
    .Finalize(cmd);

mPipeline->DispatchRays(cmd, bindingSet, width, height);
mHistoryIdx = 1 - mHistoryIdx;
```

- [ ] **Step 4: Verify compilation**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes.

- [ ] **Step 5: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/
git add Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): in-shader RIS in ReSTIR_DI_ClosestHit

Samples M candidates from StructuredBuffer<FLight>, traces shadow rays,
updates reservoir. Single-frame output (no temporal/spatial yet).
Pairwise MIS via FReSTIRPassDesc::enablePairwiseMIS (default true).

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.4: Implement temporal compute pass

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Temporal_cs.hlsl`
- Modify: `Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp`

- [ ] **Step 1: Implement temporal merge with bug-053 fix**

```hlsl
// ReSTIR_Temporal_cs.hlsl
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= screenWidth || id.y >= screenHeight) return;

    Reservoir current = gReservoirCurrent[id.xy];
    Reservoir history = gReservoirHistory[id.xy];

    if (history.sampleCount == 0) {
        gReservoirOut[id.xy] = current;  // uninitialized, skip merge
        return;
    }

    // bug-053: alpha = max(1/(FrameIndex+1), 1/HistoryFadeIn)
    float alpha = max(1.0 / (FrameIndex + 1), 1.0 / HistoryFadeIn);

    // MIS: weight history by its visibility
    float currentWeight = current.W * current.sampleCount;
    float historyWeight = history.W * history.sampleCount * (1.0 - alpha);
    float totalWeight = currentWeight + historyWeight;

    Reservoir merged;
    merged.wSum = current.wSum + history.wSum;
    merged.W = totalWeight / max(merged.wSum, 1e-6);
    merged.lightIdx = (currentWeight > historyWeight) ? current.lightIdx : history.lightIdx;
    merged.lightDir = (currentWeight > historyWeight) ? current.lightDir : history.lightDir;
    merged.radiance = (current.radiance * currentWeight + history.radiance * historyWeight) / max(totalWeight, 1e-6);
    merged.sampleCount = 1;  // reset count after merge

    gReservoirOut[id.xy] = merged;
}
```

- [ ] **Step 2: Wire temporal pass into FReSTIRPass::DispatchRays()**

After the RT dispatch, dispatch the temporal compute:
```cpp
mPipeline->DispatchTemporal(cmd, mReservoirTex[mHistoryIdx], mReservoirTex[1 - mHistoryIdx]);
```

- [ ] **Step 3: Verify on TestCubeOnPlane**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes.

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/
git add Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): ReSTIR temporal compute pass with bug-053 fix

alpha = max(1/(FrameIndex+1), 1/HistoryFadeIn) (NOT (F-1)/FadeIn!).
History unreadable (sampleCount==0) skips merge.
FrameIndex in FrameIndex uniform only — NEVER in position hash.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.5: Implement spatial compute pass with pairwise MIS

**Files:**
- Modify: `Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/ReSTIR_Spatial_cs.hlsl`
- Modify: `Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp`

- [ ] **Step 1: Implement pairwise MIS over K×K neighborhood**

```hlsl
// ReSTIR_Spatial_cs.hlsl
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= screenWidth || id.y >= screenHeight) return;

    Reservoir center = gReservoirIn[id.xy];
    uint K = CVarSpatialRadius;

    float totalWeight = 0;
    float3 totalRadiance = 0;

    // Pairwise MIS over K×K neighborhood
    for (int dy = -K; dy <= K; dy++) {
        for (int dx = -K; dx <= K; dx++) {
            int2 nPos = int2(id.xy) + int2(dx, dy);
            if (nPos.x < 0 || nPos.y < 0 || nPos.x >= screenWidth || nPos.y >= screenHeight) continue;

            Reservoir neighbor = gReservoirIn[nPos];
            if (neighbor.sampleCount == 0) continue;

            // Jacobian-corrected weight: position-independent p_hat
            float p_hat = luminance(neighbor.radiance);
            float w = p_hat * (1.0 / p_hat) * neighbor.W;  // Jacobian = 1.0
            w = clamp(w, 0.0, 5.0);  // bug-046: W-clamped weighted blend
            totalWeight += w;
            totalRadiance += neighbor.radiance * w;
        }
    }

    float3 finalRadiance = totalRadiance / max(totalWeight, 1e-6);
    gRadianceOut[id.xy] = float4(finalRadiance, 1.0);
}
```

- [ ] **Step 2: Wire spatial pass into FReSTIRPass::DispatchRays()**

After the temporal pass, dispatch the spatial:
```cpp
mPipeline->DispatchSpatial(cmd, mReservoirTex[mHistoryIdx], mRadianceTex);
```

- [ ] **Step 3: Verify on TestCubeOnPlane**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCubeOnPlane --Test`
Expected: Build passes.

- [ ] **Step 4: Commit**

```bash
git add Engine/Source/Runtime/Private/Renderer/Shader/ReSTIR/
git add Engine/Source/Runtime/Private/Renderer/ReSTIR/FReSTIRPass.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(renderer): ReSTIR spatial compute pass with pairwise MIS

W-clamped weighted blend over K*K neighborhood.
Jacobian = 1.0 (position-independent p_hat). bug-046 fix.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.6: Create TestReSTIR_DI.cpp

**Files:**
- Create: `Engine/Source/Runtime/Test/TestReSTIR_DI.cpp`
- Create: `Engine/Source/Runtime/Test/TestReSTIR_DI_Data/Scene.json`
- Modify: `Engine/Source/Runtime/Test/CMakeLists.txt`

- [ ] **Step 1: Create TestReSTIR_DI.cpp**

```cpp
// Engine/Source/Runtime/Test/TestReSTIR_DI.cpp
#include "Test.h"
#include "Renderer/ReSTIR/FReSTIRPass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/Common/FLight.h"
#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/SceneGraph/FScene.h"
#include "Logging/LogMacros.h"
#include "Core/CVar.h"

DECLARE_LOG_CATEGORY(LogTestReSTIRDI)

RECORD(test_restir_di_smoke, true) {
    HLVM_LOG(LogTestReSTIRDI, info, TXT("TestReSTIR_DI smoke test"));

    // 1. Load Sponza scene
    FScene scene;
    HLVM_ENSURE(scene.LoadFromGLTF(TXT("Sponza/Sponza.gltf")));

    // 2. Load light buffer from Scene.json
    std::vector<FLight> lights;
    HLVM_ENSURE(LoadLightsFromJSON(TEXT("Scene.json"), lights));
    nvrhi::BufferHandle lightBuffer = CreateStructuredBuffer(nvrhiDevice, lights.data(), lights.size() * sizeof(FLight), lights.size());

    // 3. Set up GBuffer fill + ReSTIR + denoise
    FDeferredFrameRenderer renderer;
    FReSTIRPass restir;
    FReBLURPass denoise;
    HLVM_ENSURE(renderer.Initialize(...));
    HLVM_ENSURE(restir.Initialize(nvrhiDevice, &scene));
    HLVM_ENSURE(denoise.Initialize(nvrhiDevice));

    // 4. Run for 4 frames
    r_Tonemap.Mode = 1; r_Tonemap.Exposure = 1.0; r_Tonemap.Gamma = 1.0;
    GetCVarManager().LoadAllFromIni();
    for (uint32_t frame = 0; frame < 4; frame++) {
        auto cmd = device->createCommandList();
        renderer.RenderGBuffer(cmd, &scene);
        restir.DispatchRays(cmd, params);
        denoise.Dispatch(cmd, restir.GetOutputTexture());
        denoise.DumpToPNG(...);
        device->executeCommandList(cmd);
    }

    CheckCondition(restir.GetOutputTexture() != nullptr);
}
```

- [ ] **Step 2: Create Scene.json (same lights as TestPathTraceGI)**

```json
{
  "lights": [
    {"type": "directional", "direction": [0.3, -1.0, 0.2], "color": [1.0, 0.95, 0.85], "intensity": 3.0},
    {"type": "area", "position": [-5.0, 4.0, 0.0], "color": [1.0, 0.8, 0.6], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [5.0, 4.0, 0.0], "color": [0.6, 0.8, 1.0], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [0.0, 4.0, -5.0], "color": [0.8, 1.0, 0.7], "intensity": 1.5, "area": 2.0},
    {"type": "area", "position": [0.0, 4.0, 5.0], "color": [0.9, 0.7, 1.0], "intensity": 1.5, "area": 2.0}
  ]
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

```cmake
add_hlvm_test(TestReSTIR_DI SOURCES TestReSTIR_DI.cpp)
```

- [ ] **Step 4: Build and run**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestReSTIR_DI --Test --DumpFrames=4`
Expected: Sponza renders, DI dominant.

- [ ] **Step 5: Commit**

```bash
git add Engine/Source/Runtime/Test/TestReSTIR_DI.cpp
git add Engine/Source/Runtime/Test/TestReSTIR_DI_Data/Scene.json
git add Engine/Source/Runtime/Test/CMakeLists.txt
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "feat(test): TestReSTIR_DI thin reference test

Loads Sponza + 5-light buffer, dispatches FReSTIRPass, denoises, tonemaps.
Fixed ACES tonemap, 4-frame dump.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.7: Add quality tests for FReSTIRPass

**Files:**
- Modify: `Engine/Source/Runtime/Test/TestReSTIR_DI.cpp`

- [ ] **Step 1: Add the bug-046 regression tests**

```cpp
RECORD(test_pairwise_mis_reduces_variance, true) {
    // Render 4 frames with pairwise MIS off
    r_ReSTIR.M.SetValue(32);
    FReSTIRPassDesc descOff; descOff.enablePairwiseMIS = false;
    // ... render, dump, compute variance ...

    // Render 4 frames with pairwise MIS on
    FReSTIRPassDesc descOn; descOn.enablePairwiseMIS = true;
    // ... render, dump, compute variance ...

    // Assert: variance_off / variance_on > 3.0
    CheckCondition(varianceOff / varianceOn > 3.0f);
}

RECORD(test_temporal_does_not_use_frameindex_in_position_hash, true) {
    // Render 2 consecutive frames on static scene
    // Pixel-level diff between frames should be < 0.05 in [0,1] linear radiance
    CheckCondition(pixelDiff < 0.05f);
}

RECORD(test_restir_di_quality, true) {
    // 4-frame stdev < 5 (with fixed ACES tonemap)
    CheckCondition(stdev < 5.0f);

    // Temporal convergence: variance with temporal on <= variance with temporal off
    CheckCondition(varianceWithTemporal <= varianceWithoutTemporal);
}
```

- [ ] **Step 2: Build and run**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestReSTIR_DI --Test --DumpFrames=4`
Expected: All RECORD tests pass. bug-046 regression test catches future regressions.

- [ ] **Step 3: Commit**

```bash
git add Engine/Source/Runtime/Test/TestReSTIR_DI.cpp
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "test(restir): quality gates + bug-046 regression tests

- Pairwise MIS on/off variance ratio > 3.0
- FrameIndex-in-position-hash: pixel-diff < 0.05 in [0,1] linear
- 4-frame stdev < 5 (with fixed ACES tonemap)
- Temporal convergence: variance_with_on <= variance_with_off

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

### Task 2.8: FDeferredFrameRenderer integration (contingent)

**Files:**
- Possibly modify: `Engine/Source/Runtime/Public/Renderer/Deferred/FDeferredFrameRenderer.h`
- Possibly modify: `Engine/Source/Runtime/Private/Renderer/Deferred/FDeferredFrameRenderer.cpp`
- Possibly create: `Engine/Source/Runtime/Public/Renderer/Deferred/INTEGRATION.md` (if FDeferredFrameRenderer doesn't exist)

- [ ] **Step 1: Check if FDeferredFrameRenderer exists**

Run: `ls Engine/Source/Runtime/Public/Renderer/Deferred/`
Expected: Either `FDeferredFrameRenderer.h` exists, or it doesn't.

- [ ] **Step 2a: If FDeferredFrameRenderer exists, add pass registry entries**

```cpp
// In FDeferredFrameRenderer.h:
class FDeferredFrameRenderer
{
    // ...
    void RegisterPass(FGIPass* pass) { mGIPass = pass; }
    void RegisterPass(FReSTIRPass* pass) { mReSTIRPass = pass; }
private:
    FGIPass* mGIPass = nullptr;
    FReSTIRPass* mReSTIRPass = nullptr;
};
```

- [ ] **Step 2b: If FDeferredFrameRenderer does NOT exist, document the integration point**

Create `INTEGRATION.md` describing:
- Where the new passes plug into the rendering pipeline
- The order: GBuffer → FGIPass/FReSTIRPass → FReBLURPass → tonemap
- Required pass registry changes (when a renderer is built)
- CVar hooks to enable/disable the new passes

- [ ] **Step 3: Build and verify**

Run: `./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestPathTraceGI --Test --Target=TestReSTIR_DI --Test`
Expected: All tests pass Debug and RelWithDebInfo.

- [ ] **Step 4: Update .wolf/cerebrum.md with new decision log entries**

Append to `## Decision Log`:
```
- **2026-06-13 ReSTIR/GI Separation**: Extracted FGIPass and FReSTIRPass into Runtime/Renderer/{GI,ReSTIR}/. Created TestPathTraceGI and TestReSTIR_DI as thin reference tests. Deleted kitchen-sink TestFewBounceGI.cpp per project policy. Bug-046 (ReSTIR GI flicker) and bug-053 (temporal alpha) hardening baked in. See docs/superpowers/specs/2026-06-12-restir-gi-separation-design.md.
```

Append to `## Do-Not-Repeat`:
```
- [2026-06-13] **Don't expose pairwise MIS as a runtime CVar** — bug-046 fix is init-time only via FReSTIRPassDesc::enablePairwiseMIS. Runtime toggling invites accidental re-introduction of 12.3-variance flicker.
- [2026-06-13] **Don't put `FrameIndex` in ReSTIR position hash** — produces per-frame flicker on static scenes. Use FrameIndex in RNG only.
```

- [ ] **Step 5: Update CLAUDE.md with CVar list**

Add to the CVar section:
```
**ReSTIR CVars** (Runtime/Renderer/ReSTIR/FReSTIRPass.cpp):
- `r_ReSTIR.M` (int, 32) — candidates per pixel
- `r_ReSTIR.SpatialRadius` (int, 5) — neighborhood radius
- `r_ReSTIR.TemporalAlpha` (float, 0.2) — history weight
- `r_ReSTIR.ResetHistory` (bool, false) — hotkey reset; does NOT auto-clear

**GI CVars** (Runtime/Renderer/GI/FGIPass.cpp):
- `r_GI.MaxBounces` (int, 4)
- `r_GI.SamplesPerPixel` (int, 8)
- `r_GI.MinRayLength` (float, 0.001)
- `r_GI.EnableRR` (bool, true)
- `r_GI.RussianRoulette` (float, 0.95)
- `r_GI.DebugBounceStats` (bool, false) — gates debug UAV for measurement
```

- [ ] **Step 6: Commit**

```bash
git add Engine/Source/Runtime/Public/Renderer/Deferred/
git add .wolf/cerebrum.md
git add CLAUDE.md
git -c user.email="claude@anthropic.com" -c user.name="Claude" commit -m "docs(renderer): integration docs + bug-046/053 regression test entries

FDeferredFrameRenderer registry OR INTEGRATION.md (contingent on existence).
wolf/cerebrum.md: 2 new Do-Not-Repeat entries, 1 Decision Log entry.
CLAUDE.md: CVar list for r_GI.* and r_ReSTIR.* added.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review (Plan vs Spec)

### Spec coverage

| Spec Section | Plan Task(s) |
|--------------|--------------|
| §1 Context | (background only) |
| §1 Goals (1) Two clean reference tests | Tasks 1.8, 2.6 |
| §1 Goals (2) Both features extracted to Runtime/Renderer/ | Tasks 1.1, 2.1 |
| §1 Goals (3) No architectural confusion | Implicit (ReSTIR is in-shader, GI is multi-bounce) |
| §1 Goals (4) Bug-046 regression tests | Task 2.7 |
| §1 Goals (5) TestFewBounceGI deleted | Task 1.10 |
| §1 Goals (6) Bindless textures preserved | Task 1.5 (uses existing bindless in GIClosestHit) |
| §1 Goals (7) BilateralDenoise fix verified | Task 0.1 |
| §2 Non-Goals (1) No scene file format | Tasks 1.8, 2.6 (use ad-hoc Scene.json, not a format) |
| §2 Non-Goals (2) No ReSTIR GI | Out of scope (not in plan) |
| §2 Non-Goals (3) No TAA radiance blend | Task 2.4 (temporal is reservoir-only) |
| §2 Non-Goals (4) No new light types | Task 0.3 (FLight supports 4 types only) |
| §2 Non-Goals (5) No backward compat | Task 2.1 (old methods deleted) |
| §3 Architecture (folder layout) | All tasks (paths match) |
| §3 FGIPass API | Task 1.1 (header), 1.2-1.4 (impl), 1.5-1.6 (shaders), 1.7 (debug UAV) |
| §3 FReSTIRPass API | Task 2.1 (header + impl), 2.3 (RIS), 2.4 (temporal), 2.5 (spatial) |
| §3 FLight struct | Task 0.3 |
| §4 TestPathTraceGI data flow | Task 1.8 |
| §4 TestReSTIR_DI data flow | Task 2.6 |
| §4 Bug-046 hardening | Task 2.3 (RIS), 2.4 (temporal), 2.5 (spatial MIS) |
| §4 3-layer testing | Tasks 1.8-1.9, 2.6-2.7 |
| §4 Quality gates table | Tasks 1.9 (FGIPass gates), 2.7 (FReSTIRPass gates) |
| §4 Tonemap gating | Tasks 1.8, 2.6 (set ACES, Exposure=1.0) |
| §4 Bounce 2 measurement | Task 1.7 (r_GI.DebugBounceStats UAV) |
| §4 Pixel-diff units | Task 2.7 (< 0.05 in [0,1] linear radiance) |
| §5 Day 0 denoiser | Task 0.1 |
| §5 Day 1 stub | Task 1.1, 1.2, 1.3 |
| §5 Day 2 real bodies | Task 1.4, 1.5 |
| §5 Day 3 multi-bounce | Task 1.6 |
| §5 Day 4 Sponza | Task 1.8 |
| §5 Day 5 quality + delete | Tasks 1.9, 1.10 |
| §5 Day 6 move ReSTIR | Task 2.1 |
| §5 Day 7 RIS | Task 2.3 |
| §5 Day 8 temporal | Task 2.4 |
| §5 Day 9 spatial | Task 2.5 |
| §5 Day 10 Sponza | Task 2.6 |
| §5 Day 11 quality | Task 2.7 |
| §5 Day 12 contingent integration | Task 2.8 |

**Coverage**: 100%. Every spec goal, non-goal, and day-by-day milestone has a corresponding task.

### Placeholder scan

Searched plan for: "TBD", "TODO", "implement later", "similar to Task N", "add appropriate error handling".
- No "TBD" / "TODO" found
- 1 reference to spec section 3 (acceptable, refers to spec for context not for required content)
- All error handling explicit (HLVM_ASSERT, HLVM_ENSURE in code blocks)

### Type consistency

- `FGIPass::Initialize(nvrhi::IDevice*, const FScene*, const FGIPassDesc&)` — used in Tasks 1.2, 1.4
- `FGIPass::DispatchRays(nvrhi::ICommandList*, const FGIDispatchParams&)` — used in Tasks 1.2, 1.4, 1.8
- `FReSTIRPass::Initialize(nvrhi::IDevice*, const FScene*, const FReSTIRPassDesc&)` — used in Tasks 2.1, 2.6
- `FReSTIRPass::DispatchRays(nvrhi::ICommandList*, const FReSTIRDispatchParams&)` — used in Tasks 2.1, 2.3, 2.6
- `ResetHistory()` — defined Task 2.1, used Task 2.3 (CVar-triggered)
- `FReSTIRDispatchParams::lightBuffer` — used Task 2.6 (loading lights from JSON)
- `FReSTIRPassDesc::enablePairwiseMIS` — used Task 2.7 (regression test)

All types/method signatures consistent.

### Identified gap to address

The plan does NOT include a task for the **`FReBLURPass` external-texture-overload check** (decision 2026-06-13). This is a small verification step. **Action**: Added to Task 1.8 Step 1 as a pre-flight check.
