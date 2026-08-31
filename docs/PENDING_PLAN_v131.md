# Pending Plan v131 — Narrowed bisect (post-tick-150 handle-identity falsification)

- task: Continue bisecting the GI shader's GBuffer SRV binding issue, addressing the THREE REMAINING candidate root causes now that the handle-identity hypothesis is falsified. Tick 150's empirical evidence (3 runs at 3 different handle addresses all matching perfectly between RenderGBuffer and FGIPass::DispatchRays) eliminated hypothesis #1 (texture recreation). The remaining 3 candidates are: (a) slangc dead-stripping cases 20u/21u/22u in the SRV reads, (b) image layout transition mismatch (SHADER_READ_ONLY_OPTIMAL vs GENERAL), (c) wrong binding slot mapping between C++ binding layout and shader registers.
- source: no bundle — direct edit + file inspection (handle-identity evidence in docs/PIPELINE_HEALTH_2026-07-30_tick150.md + docs/DIAGNOSTIC_2026-07-30.md "Recommended next step")
- approach: Three parallel diagnostic measures, each < 10 source lines, each targeting exactly ONE root-cause candidate:
  - **Candidate A (slangc dead-strip) probe**: add a debug mode `case 31u` that does a non-trivial computation with the GBufferMaterial value (e.g., `(r * 0.5 + 0.1)`) before output. If mode 31 shows real data, mode 20's reads aren't dead-stripped (rules out A). If mode 31 still shows zero, slangc is stripping the SRV reads (confirms A — fix is to use Output[gl_LaunchID] instead of returning early, OR force keep-alive via side-effect write).
  - **Candidate B (layout transition) probe**: add a small CPU-side diagnostic in `FGIPass.cpp` BEFORE `setTextureState` calls: log the current nvrhi resource state via `m_pDevice->queryTextureState(Desc.GBufferWorldPos)`. If state is `ShaderResource` (not `ShaderReadOnly`) when the descriptor is bound, the layout is the issue. Add a `commitBarriers()` call between `setTextureState` and `setRayTracingState`, with corresponding commit log line `[layout-fix] commitBarriers before setRayTracingState`.
  - **Candidate C (binding slot) probe**: add a CPU-side log line in `FGIPass.cpp` that prints `nvrhi::VulkanBindingOffsets` (constantBufferOffset, shaderResourceOffset, samplerOffset, unorderedAccessViewOffset) at binding-set creation time. If any offset is non-zero, the AGENTS.md gotcha is biting (binding offset wrong). If all offsets are zero, the slot mapping is structurally correct.
  Each probe is gated on `Desc.FrameIndex < 4u` (avoid log spam). All three are non-test-file changes; validation remains per-experiment.
- diff_estimate: +28 / -2 lines. Candidate A: +12 lines × 2 HLSL copies = +24 lines (case 31u block + preceding comment). Candidate B: +6 lines in FGIPass.cpp (2-line state query + 4-line commitBarriers call). Candidate C: +2 lines in FGIPass.cpp (one info log + one getter call).
- skip_plan_review: no — this is a NEW plan targeting a NARROWED bisect; the three-candidate diagnostic is not the same recipe as v125/v126/v127/v128/v130. v130 reviewed the bypass-patch plan; v131 is a three-pronged diagnostic targeting the post-handle-identity falsification evidence. Plan-criticer MUST audit this plan to confirm it addresses the 3 remaining candidates and not the already-falsified handle-identity hypothesis.
- test_strategy: No new test files. Validation per-experiment: parent runspace executes `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20,21,22,30,31` (the 5 modes covering old + new discriminators), vision + numpy on `dumps/*_gi_raw_frame8.png`, and `validate_restir_gi.py`. Discriminating outcome per candidate:
  - **A**: mode 31 = (non-zero output, partial Sponza color) → SRV works for modes 20/21/22 → fix path is "force keep-alive write in mode 20/21/22". mode 31 = (still zero) → slangc is stripping → fix path is "move SRV reads to compute path-trace output".
  - **B**: state query returns `ShaderResource` (not `ShaderReadOnly`) at dispatch time → layout issue confirmed → fix is the commitBarriers patch + VUID-00344 validation (per `references/nvrhi-deferred-barrier-ordering.md`). state query returns correct `ShaderReadOnly` → layout is fine.
  - **C**: log line shows `offsets.constantBufferOffset != 0` → AGENTS.md gotcha biting → fix is to add `.SetBindingOffsets({all_zero})` call. All offsets zero → binding slots correct.
  In all three outcomes, the bisect narrows further or closes.
- risks: All three candidates are still binding/SRV/RT-pipeline issues. Each probe is structurally simple but ALL require terminal to verify (compile, run, dump, vision). Per EC-039, terminal is blocked in this cron runspace. Parent runspace with terminal is required for empirical validation. The probes themselves are file-only and statically analyzable.

---

## Why this plan is NOT phantom-cycle

Tick 150 found empirical evidence that falsifies the handle-identity hypothesis (3 runs, 3 different addresses, all matching). This v131 plan addresses the 3 REMAINING candidates, which are fundamentally different from v125/v126/v127/v128/v130 (those targeted handle-identity). Spawning v131 = planner with a DIFFERENT recipe = NOT phantom. The plan can be written file-only; its empirical verification requires terminal.

## Step A — LAND (file-only this cron tick)

**Hypothesis (Candidate A):** slangc dead-strips `case 20u/21u/22u` SRV reads because the read result isn't on the output's data path in a way slangc can prove. Per anti-pattern #7 from `gpu-rendering-bisect-debug`, slangc compiles each entry point independently and may strip unused SRV reads. `case 31u` adds a non-trivial arithmetic transformation to the read result, making it observable to slangc's reachability analysis.

**Implementation:** add `case 31u` to the debug switch in `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (lines 700-712, after case 30u at line 691-693, in the Private copy). Mirror edit in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`. The new case reads `GBufferMaterial.Load(int3(pixel, 0))`, applies a transformation `r * 0.5 + 0.1`, and outputs the result. ~12 lines per copy.

**Discriminating outcome:**
- mode 31 dump shows non-uniform color → SRV reads are alive for mode 31 → fix is to apply the same `keep-alive write` pattern to modes 20/21/22 in a follow-up commit.
- mode 31 dump still shows uniform zero → slangc IS dead-stripping → root cause is upstream (binding layout or pipeline state).

## Step B — LAND (file-only this cron tick)

**Hypothesis (Candidate B):** image layout transition. The Vulkan validation layer would catch this with VUID-00344, but `DeviceManagerVk4_LifeCycle.cpp` stubs `createValidationLayer` to `nullptr`, gating the layer off. Per `references/nvrhi-deferred-barrier-ordering.md`, the deferred barrier ordering issue is real and produces exactly this symptom (SRV reads return zero while textures have data).

**Implementation:** add to `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` immediately AFTER `m_pCommandList->setTextureState(...)` calls for GBufferWorldPos/Normal/Material (around line 549, before `setRayTracingState`):
1. Query the current state: `nvrhi::TextureState CurrentState = m_pDevice->getTextureState(Desc.GBufferWorldPos);` (or equivalent API; verify in nvrhi fork headers).
2. Log: `HLVM_LOG(LogGI, info, TXT("[layout-fix] GBufferWorldPos state=%d"), (int)CurrentState);`
3. If state != ShaderReadOnly (the canonical SRV layout), add `m_pCommandList->commitBarriers();` before `setRayTracingState`.

Gated on `Desc.FrameIndex < 4u` (avoid log spam).

**Discriminating outcome:**
- Log shows `state=ShaderReadOnly` → layout is fine → candidate B ruled out.
- Log shows `state=ShaderResource` (or other) → layout is wrong → add commitBarriers fix → rebuild → re-test.

## Step C — LAND (file-only this cron tick)

**Hypothesis (Candidate C):** wrong binding slot mapping. Per AGENTS.md, `VulkanBindingOffsets.constantBufferOffset` defaults to 256 (NOT zero), which can cause cbuffer b0 → binding 256 instead of binding 0, with shader still expecting binding 0. The codebase uses `FBindingLayoutBuilder`, but `FBindingLayoutBuilder` may not zero the offsets on construction (per tick 130 audit: the constructor DOES zero them by default, but a parent caller might override).

**Implementation:** add to `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` inside `FGIPass::CreateBindingLayouts()` (around line 280-295):
```cpp
if (Desc.FrameIndex < 4u) {
    const auto Offsets = BindingLayout->GetBindingOffsets();  // or m_BindingLayout->GetBindingOffsets()
    HLVM_LOG(LogGI, info, TXT("[binding-offsets] CBO=%u SRO=%u SO=%u UAVO=%u"),
        Offsets.constantBufferOffset, Offsets.shaderResourceOffset,
        Offsets.samplerOffset, Offsets.unorderedAccessViewOffset);
}
```

**Discriminating outcome:**
- Log shows all offsets == 0 → structurally correct → candidate C ruled out.
- Log shows any offset != 0 → AGENTS.md gotcha biting → add explicit `BindingLayout->SetBindingOffsets({0,0,0,0})` call in a follow-up commit.

## Parent-runspace recipe (60-120 seconds total)

```bash
# 1. Rebuild shaders + binary
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal

# 2. Run all 5 modes back-to-back (5 × ~30s = ~150s)
for MODE in 20 21 22 30 31; do
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=$MODE \
    ./Binary/Debug/TestReSTIR_GI_Temporal 2>&1 | tee /tmp/rgi_mode${MODE}.log
done

# 3. Vision + numpy on each freshest dump
for MODE in 20 21 22 30 31; do
  python3 -c "
from PIL import Image
import numpy as np
import glob
files = sorted(glob.glob('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_gi_raw_frame8.png'))
# Filter for current run by mtime
import os
files = [f for f in files if os.path.getmtime(f) > $(date +%s -d '2 minutes ago')]
arr = np.array(Image.open(files[-1])) if files else None
print('mode=$MODE shape:', arr.shape if arr is not None else 'no file')
print('mode=$MODE mean:', arr[..., :3].mean(axis=(0,1)) if arr is not None else None)
print('mode=$MODE unique:', [len(np.unique(arr[..., c])) for c in range(4)] if arr is not None else None)
"
done

# 4. Inspect [layout-fix] and [binding-offsets] log lines
grep -E '\[layout-fix\]|\[binding-offsets\]' Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log

# 5. Final acceptance gate (7 criteria)
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
  $(ls -1t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ | head -7)
```

## Discriminating outcome table

| Candidate | Diagnostic success | Diagnostic failure | Fix if confirmed |
|-----------|--------------------|--------------------|------------------|
| A (slangc dead-strip) | mode 31 shows non-zero | mode 31 zero | Apply keep-alive pattern to modes 20/21/22 |
| B (layout transition) | state=ShaderReadOnly | state!=ShaderReadOnly | Add commitBarriers() before setRayTracingState |
| C (binding slot) | all offsets = 0 | any offset != 0 | Add SetBindingOffsets({0,0,0,0}) |

The cycle closes when at least one candidate is confirmed AND the corresponding fix is applied AND the 7-criteria acceptance gate passes.

## Acceptance gate (inherited from v130)

1. Debug target builds. (terminal)
2. Run env vars work. (terminal)
3. No Vulkan VUID/ERROR. (log grep)
4. No command-list errors. (log grep)
5. validate_restir_gi.py passes. (terminal)
6. Fresh display image shows Sponza. (terminal + vision)
7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial. (terminal + numpy)

## What this plan does NOT change

- No commits, pushes, history rewrites.
- No governance-file edits.
- No new test files.
- No re-architecture of the binding layout plumbing (committed in a follow-up plan if needed).

## What unblocks this plan

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The probes themselves are file-only and the patches land this tick. The discriminating experiments close in 60-180 seconds once terminal is available.