# Pending Commit v172 (DRAFT — operator-side commit; cron writes this proposal only, does not apply)

- plan: docs/PENDING_PLAN_v172.md
- plan_review: docs/PENDING_PLAN_REVIEW_v172.md (KEEP with refinements)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: file-only diagnostic this tick; supersedes v170 (ComposeDisplay hypothesis, REFUTED) and v171 (ACES saturation hypothesis, INCOMPLETE)
- target: local working tree (no push per job hard rules)
- task: Add a DirectionalLight via Desc.LightsBuffer + reduce AmbientScale so per-pixel NdotL variation reaches the display readout
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- skip_impl_review: no — patch touches TestReSTIR_GI_Temporal.cpp with multi-line addition (light setup), and the plan-criticer surfaced a caveat about CVar-default Directional light; operator-side test cycle will catch mis-configuration before merge if reviewer runs the build
- produces_test_files: no
- notes: Patch uses `Renderer::MakeDirectionalLight` (declared at `Engine/Source/Runtime/Public/Renderer/Common/FLightBuilder.h:18-20`) + `Renderer::UploadLightBuffer` (declared at `FLightBuilder.h:42-52`). All API surface already exists in the codebase (used by TestCornellBoxGI for the proven control). Includes `#include "Renderer/Common/FLightBuilder.h"` in test if not already included.

## Proposed patch

### Edit site
File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
Location: Inside the per-frame Desc init block (line 793-810 area, search for `Desc.AmbientScale = 0.35f;`)

### Code changes (+15/-1 lines net)

```cpp
// v172: replace ambient-only lighting with explicit Directional sun + reduced ambient baseline.
// Root cause (per PENDING_PLAN_v172.md): LightCount=0 + ambient term dominates → gi_raw uniform.
// This add (~12 lines) + change (1 line) introduces NdotL per-pixel variation.
//
// API used:
//   Renderer::MakeDirectionalLight (Engine/Source/Runtime/Public/Renderer/Common/FLightBuilder.h:18-20)
//   Renderer::UploadLightBuffer    (Engine/Source/Runtime/Public/Renderer/Common/FLightBuilder.h:42-52)
// Both are already in use by TestCornellBoxGI (proven control).
//
// (a) Add 1 Directional light so primaryDirect contributes NdotL variation across Sponza geometry.
// Direction chosen to hit gallery arches + floor (camera looks down into bowl):
{
    const float Dir[3]   = { 0.3f, -0.85f, 0.45f };  // shallow downward + slight forward
    const float Color[3] = { 1.0f, 0.95f, 0.85f };  // warm white (slight amber)
    Renderer::FLight SunLight = Renderer::MakeDirectionalLight(
        Dir, Color, /*Intensity*/ 4.0f);
    DescGI.LightsBuffer   = Renderer::UploadLightBuffer(NvrhiDevice, &SunLight, 1);
    DescGI.LightCount     = 1;
}

// (b) Reduce ambient baseline so constant term doesn't dominate gi_raw + push display off ACES saturation asymptote.
DescGI.AmbientScale     = 0.10f;   // was 0.35f
DescGI.AmbientColor[0]  = 0.75f;   // unchanged
DescGI.AmbientColor[1]  = 0.80f;   // unchanged
DescGI.AmbientColor[2]  = 1.00f;   // unchanged
DescGI.AmbientColor[3]  = 0.0f;    // unchanged
```

### Include (if not already present)
Verify `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` includes `Renderer/Common/FLightBuilder.h`. If not, add to the include block near the top of the file (alongside `Renderer/Common/FBindingCache.h`):

```cpp
#include "Renderer/Common/FLightBuilder.h"
```

## Alternative: 1-line minimal patch (try first, escalate if insufficient)

If the CVar-default Directional light at `r_GI_LightDirX/Y/Z = 0.577/0.577/0.577` provides adequate illumination of visible Sponza geometry, the simpler fix is just reduce `Desc.AmbientScale`. Try this first:

```cpp
// In TestReSTIR_GI_Temporal.cpp line 802:
Desc.AmbientScale = 0.10f;   // was 0.35f
```

Rebuild + run + check log. If `display std ≥ 0.10`, done. If `display std < 0.05`, escalate to the full 15-line patch above.

**Why try this first**: cheaper diff, easier to revert if wrong, validates whether the synthesized Directional light is contributing meaningful NdotL. If yes, the ambient reduction alone is sufficient (no light add needed).

## Plan Deviations

None. The patch matches `PENDING_PLAN_v172.md` §"Concrete code edits" block 1:1.

## Self-review checklist (operator-side)

- [ ] Validation: `validate_restir_gi.py` exits 0 with 6/6 PASS after rebuild+run
- [ ] Error handling: no Vulkan validation layer errors in new log (grep `VUID-` → 0)
- [ ] Tests: post-fix log shows recognizable Sponza (vision gate)
- [ ] Diff size: +15/-1 lines (within 50-line budget per `skip_impl_review: no` rule; patch is multi-line surgical addition; reviewer recommended)
- [ ] No new files created
- [ ] No cmake regen (only `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` modified)
- [ ] No FetchContent / nvrhi fork changes
- [ ] No shader recompile needed (only test-side Desc config)

## Rebuild + verify recipe (verbatim from plan)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Edit
$EDITOR Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
# Apply the patch above

# Build
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Run + dump
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# Verify log
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0

# Validate
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 6/6 PASS

# Vision check
ls Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | tail -1
# Open in image viewer — expect: Sponza gallery arches + floor + back wall + directional shadow

# Mode-20 sanity
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Expect gi_raw dump to be NON-UNIFORM (per-pixel albedo variation)
```

**Total operator-side effort**: ~5 min for incremental build + ~25 sec for run + ~30 sec for grep/validate/vision.

## Acceptance criteria (from PENDING_PLAN_v172.md, re-stated)

| # | Criterion | Cron-verifiable? | Empirical source |
|---|-----------|------------------|------------------|
| 1 | Debug target builds | NO (operator-only) | Build.sh exit code |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | NO (operator-only) | exit code 0 |
| 3 | No Vulkan VUID/ERROR/CommandList errors | NO | grep `VUID\|ERROR\|CommandList error` returns 0 |
| 4 | `validate_restir_gi.py` passes newest dump | NO | exit code 0, 6/6 PASS |
| 5 | Fresh display PNG (vision) shows recognizable Sponza | NO (terminal + vision) | vision check |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | NO (terminal-side) | grep gi_raw stats non-uniform |
| 7 | All 7 acceptance criteria pass | NO (depends on 1-6) | aggregate |

3/7 are also PARTIALLY file-only-verifiable via the on-disk log evidence (display std check requires grep `Binary/Debug/TestReSTIR_GI_Temporal.log` after the operator rebuilds — i.e., post-fix log is file-only-readable).

**Honest capability-vs-permission table for this cron tick:**

| Action | Capability | Permission | Result |
|--------|-----------|-----------|--------|
| Read PENDING_*.md | yes | yes | Wrote v172 plan + plan-review + commit |
| Write PENDING_COMMIT_v172.md | yes | yes | Done (this file) |
| Modify source code | yes | **no** (job rule: "Do not commit, push, or modify governance files") | NOT DONE |
| Build.sh --Rebuild | NO (terminal-blocked) | NO | NOT DONE |
| ./TestReSTIR_GI_Temporal | NO | NO | NOT DONE |
| python3 validate_restir_gi.py | NO | NO | NOT DONE |
| vision_analyze display PNG | NO | NO | NOT DONE |

## Cumulative status for the lineage (post-this-tick)

- v170 → v171 → v172 supersetions documented in PICK.md and PENDING_PLAN_v172.md
- v166 patch INTACT on disk (graphics-pipeline rebind in 3 nvrhi fork copies)
- v137+v140+v151 source fixes INTACT (binding zero-offset, AmbientColor override, ReSTIR Generate split)
- 4/7 acceptance criteria file-only-verifiable post-fix (build artifact, log, validator, vision)
- 3/7 require operator-side terminal+vision+numpy
- This cron lineage has reached the file-only ceiling of progress — termination requires operator-side recipe execution
