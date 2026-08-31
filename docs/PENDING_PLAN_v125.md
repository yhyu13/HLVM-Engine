# Pending Plan v125 — GBuffer SRV binding bisect (single-variable per experiment)

- task: Continue bisecting GBuffer SRV binding in the GI shader (per docs/DIAGNOSTIC_2026-07-30.md "Recommended next step").
- source: no bundle — direct edit + file inspection
- approach: Three independent single-variable experiments to localize the empty-SRV-binding zero readback signature documented in DIAGNOSTIC_2026-07-30.md and gpu-rendering-bisect-debug/references/empty-srv-binding-zero-readback.md. Each experiment changes one thing, then we run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` and vision/numpy analyze the `gi_raw_frame8.png` + `display_frame8.png`. The current bisect has only two state variables left (binding identity vs binding layout vs slangc dead-strip); the experiments below narrow the search space.
- diff_estimate: +30 / -8 lines (one new debug mode per experiment; one handle-log per dispatch)
- skip_plan_review: no — this is a multi-experiment bisect plan that should be critiqued for design soundness before spending 4 build cycles on it.
- test_strategy: No new test file. Validation is per-experiment: vision analyze + numpy stats on the freshest dump group only (validate_restir_gi.py has documented false-positive bias on uniform-white dumps per DIAGNOSTIC_2026-07-29.md, so it is not the verdict gate).
- risks: All experiments require terminal access. The cron runspace is structurally terminal-blocked per OVERSEER_ESCALATION.md (EC-039); the impler cannot run these without parent-side toolset reconfiguration or parent-side execution.

---

## Experiment A — Handle identity probe (zero source code cost, highest signal)

**Hypothesis:** The GI shader's `GBufferMaterial.Get()` returns a different handle than the raster pass wrote to (e.g., the texture was recreated mid-frame and the GI pass holds a stale handle).

**Implementation:** Add one log line in `FGIPass::DispatchRays` (line 533 of FGIPass.cpp, just before `WriteConstants`):

```cpp
HLVM_LOG(LogGI, info, TXT("FGIPass: GBuffer handles (this frame) WorldPos=0x{:x} Normal=0x{:x} Material=0x{:x} Frame={}"),
    reinterpret_cast<uintptr_t>(Desc.GBufferWorldPos.Get()),
    reinterpret_cast<uintptr_t>(Desc.GBufferNormal.Get()),
    reinterpret_cast<uintptr_t>(Desc.GBufferMaterial.Get()),
    Desc.FrameIndex);
```

Add the same line at the end of `RenderGBuffer` (line 1519 of TestReSTIR_GI_Temporal.cpp, just before the loop-close `}`):

```cpp
HLVM_LOG(LogTest, info, TXT("RenderGBuffer: GBuffer handles (this frame) WorldPos=0x{:x} Normal=0x{:x} Material=0x{:x} Frame={}"),
    reinterpret_cast<uintptr_t>(GBufferWorldPos.Get()),
    reinterpret_cast<uintptr_t>(GBufferNormal.Get()),
    reinterpret_cast<uintptr_t>(GBufferMaterial.Get()),
    FrameCount);
```

**Verify:**
```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 Binary/Debug/TestReSTIR_GI_Temporal
grep "GBuffer handles" Binary/Debug/TestReSTIR_GI_Temporal.log
```

**Predicted outcomes:**
- A1: Handles match across RenderGBuffer + FGIPass → handle identity is innocent, binding layer is the bug. Move to experiment B.
- A2: Handles differ → stale handle in FGIPass; trace `Desc.GBufferWorldPos` through `FillGBufferHardcoded`/`WriteGBufferSentinels`/etc. for any re-assignment. (The texture members are `nvrhi::TextureHandle` per line 1795; if they're re-assigned via `NvrhiDevice->createTexture()` somewhere, that's the bug.)
- A3: Handles match but address is 0 → texture is null in Desc; the dispatch body should have caught this earlier but the early-return at line 522-523 only checks `Desc.SceneTLAS`, `Desc.OutputTexture`, `Desc.ViewConstants`, NOT `Desc.GBufferWorldPos/Normal/Material`. Add three lines to that early-return gate:

```cpp
if (!Desc.GBufferWorldPos || !Desc.GBufferNormal || !Desc.GBufferMaterial)
{
    HLVM_LOG(LogGI, err, TXT("FGIPass::DispatchRays: missing GBuffer handles WorldPos=0x{:x} Normal=0x{:x} Material=0x{:x}"),
        reinterpret_cast<uintptr_t>(Desc.GBufferWorldPos.Get()),
        reinterpret_cast<uintptr_t>(Desc.GBufferNormal.Get()),
        reinterpret_cast<uintptr_t>(Desc.GBufferMaterial.Get()));
    return;
}
```

This is the cheapest probe (~3 lines, no shader rebuild, runs in <5s).

---

## Experiment B — Single-pixel sentinel debug mode

**Hypothesis:** The `case 20u/21u/22u` SRV reads in GIPathTracing.hlsl are returning zero because of binding identity OR slangc dead-strip. A single-pixel sentinel that outputs magenta if non-zero (vs. all-zero) gives a binary "is the binding universally broken" answer that the per-pixel mode 20 cannot.

**Implementation:** Add a new case in GIPathTracing.hlsl (BOTH copies — Private and Test Data — must match) at line 672:

```hlsl
case 30u: {
    // Single-pixel sentinel. Read GBufferWorldPos at literal (0,0,0).
    // If binding works at all, the value is non-zero (Sponza worldpos at
    // the corner). Output magenta (1, 0, 1) if non-zero, else (0, 0, 0).
    float3 wp = GBufferWorldPos.Load(int3(0, 0, 0)).rgb;
    debugColor = (length(wp) > 0.001f) ? float3(1.0f, 0.0f, 1.0f) : float3(0.0f, 0.0f, 0.0f);
    break;
}
```

**Verify:**
```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild  # forces slangc rebuild of GIPathTracing.sblob
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=30 Binary/Debug/TestReSTIR_GI_Temporal
python3 -c "import numpy as np; from PIL import Image; a = np.array(Image.open('dumps/*_gi_raw_frame8.png')); print('R mean=', a[...,0].mean(), 'magenta pixel fraction=', float(np.sum((a[...,0]>250)&(a[...,1]<5)&(a[...,2]>250))) / (a.shape[0]*a.shape[1]))"
```

**Predicted outcomes:**
- B1: gi_raw is 100% magenta → GBufferWorldPos SRV read works at literal (0,0,0). The per-pixel case 22 must be sampling off the texture, OR case 22 has a slangc compile error. Try re-running case 22 alone and diffing the dump.
- B2: gi_raw is 100% zero → GBufferWorldPos SRV read returns zero at all sampled positions, including (0,0,0). The binding is universally broken at the descriptor level. Move to experiment C.
- B3: gi_raw is partial magenta + partial zero → sampler is broken, not binding. Check `LinearSampler` binding (register s2). Likely an SRV read without a sampler is fine, but worth checking.

---

## Experiment C — Binding layout descriptor reflection

**Hypothesis:** If experiments A and B both confirm "binding universally broken", the binding layout may declare different binding indices than what the SPIR-V expects. (E.g., slangc placed `GBufferWorldPos` at set=0 binding=4 due to dead-strip, but the C++ binding layout declares binding=1.)

**Implementation:** Use `spirv-cross --reflect` on the compiled shader to dump binding layout:

```
spirv-cross --reflect Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob | grep -A 2 "GBuffer\|Output\|SceneBVH"
```

If the sblob is not the underlying format, look for the .spv it was built from. The Test Data dir's `GIPathTracing.sblob` is the loaded artifact.

**Predicted outcomes:**
- C1: `GBufferWorldPos` reflected at `set=0 binding=1` → SPIR-V matches C++ binding layout. Binding layer is correct. The bug is downstream (sampler? format?) — try registering the GBuffer textures at `nvrhi::Format::RGBA8_UNORM` instead of `RGBA32_FLOAT`? Unlikely. The next suspect is the Vulkan validation layer silently dropping the second binding set.
- C2: `GBufferWorldPos` reflected at `set=1 binding=N` → SPIR-V has it in space1 (like `Output : register(u0, space1)` at line 88). The C++ binding layout is set 0. The SRV binding set should still bind at set 0, but slangc may have moved the texture to set 1 too. This is a real bug — fix is to remove `, space1` from the GBuffer texture declarations in GIPathTracing.hlsl OR add a separate binding layout for them.
- C3: `GBufferWorldPos` not in the reflection at all → slangc dead-stripped the binding declaration. The shader expects register(t1) but the SPIR-V has no t1 binding at all. This would be a slangc bug or a dead-code elimination that confused it. Fix: add a dummy read of `GBufferWorldPos` somewhere unconditional in the shader to prevent DCE.

---

## Cycle map

- **v125 (this plan):** experiments A + B + C as above. Build cycle per experiment.
- **v126 (impler):** runs experiment A (cheapest, no shader rebuild). Records handle addresses from log.
- **v127 (impler):** runs experiment B (requires slangc rebuild). Records magenta-pixel fraction.
- **v128 (impler):** runs experiment C (requires `spirv-cross` on disk — check for it; if absent, install via vcpkg or skip and use `nvrhi::utils` reflection on the binding layout desc directly).
- **v129 (plan-criticer/review):** evaluate results, pick next experiment from the bisect table in `gpu-rendering-bisect-debug/references/empty-srv-binding-zero-readback.md`.

---

## Acceptance gate (per dispatcher instructions)

Each experiment's success criteria is independent and concrete:

- Experiment A success: handle addresses logged for both RenderGBuffer and FGIPass dispatch sites, comparison made, mismatch identified or "all match" confirmed in `docs/PIPELINE_HEALTH_<date>.md`.
- Experiment B success: vision-analyzed + numpy-analyzed gi_raw dump with `HLVM_PT_DEBUG_MODE=30`, magenta-pixel-fraction recorded, predicted outcome B1/B2/B3 logged.
- Experiment C success: spirv-cross reflection output captured, GBuffer binding locations recorded, predicted outcome C1/C2/C3 logged.

The dispatcher's full acceptance criteria (Debug build, dumps, no VUID, validator, vision Sponza, mode 20 non-zero) **cannot be satisfied** in this runspace because terminal is structurally blocked. They require parent-side execution per `docs/OVERSEER_ESCALATION.md` Option A (reconfigure cron) or B (parent executes the recipe directly).

---

## What this plan does NOT change

- No edits to GIPathTracing.hlsl case 20u/21u/22u (those are already in both copies, per DIAGNOSTIC_2026-07-30.md "Sync'd to the test data dir copy").
- No edits to the binding layout in FGIPass.cpp — that is already v22-split correctly (verified this tick against PIPELINE_HEALTH_2026-07-30_tick105.md).
- No edits to the GI shader register declarations (`register(t1/t2/t3)` at line 96-98 match `AddTextureSRV(1/2/3)` at FGIPass.cpp:288-290).
- No commits. The plan lives only in `docs/PENDING_PLAN_v125.md`.

---

## Plan Deviations policy (per six-role-pipeline skill)

If during implementation the impler discovers:
- The handle identity experiment (A) reveals a different code path than expected (e.g., the texture is recreated in a hook not anticipated), document in `## Plan Deviations` section of PENDING_COMMIT_v<N>.md. The reviewer will audit deviation justification.
- The slangc reflection (C) reveals a binding layout mismatch, document the deviation and propose a fix in PENDING_COMMIT_v<N>.md. The reviewer must verify the fix doesn't break the v22 split.

Per HARD INVARIANT #2, the reviewer must run on any commit that produces test files. None of these experiments produce test files, so `skip_impl_review: yes` is acceptable IF no path under `tests/` is touched. None will be.