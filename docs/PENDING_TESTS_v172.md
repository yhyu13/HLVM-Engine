# Pending Tests v172

- commit: docs/PENDING_COMMIT_v172.md
- plan: docs/PENDING_PLAN_v172.md
- runner: tester (file-only, single-profile host, terminal-blocked)
- timestamp: 2026-08-15T-tick1564-Z

## Test plan (operator-side, written for the human to execute)

The v172 commit proposes a 14-line addition to `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (add DirectionalLight setup + change 1 AmbientScale value). The tester role cannot empirically execute the required commands in this runspace (terminal blocked by tirith). This marker captures the test design the operator runs.

### Recipe (from `PENDING_PLAN_v172.md` and `PENDING_COMMIT_v172.md`)

#### Step 1: Apply patch (choose minimal or full)

**Option A — Minimal** (try first; lower risk):
```cpp
// In TestReSTIR_GI_Temporal.cpp line 802:
Desc.AmbientScale = 0.10f;   // was 0.35f
```

**Option B — Full** (if Option A yields low display std):
```cpp
// Add to TestReSTIR_GI_Temporal.cpp near line 802 (the per-frame Desc init block):
{
    const float Dir[3]   = { 0.3f, -0.85f, 0.45f };
    const float Color[3] = { 1.0f, 0.95f, 0.85f };
    Renderer::FLight SunLight = Renderer::MakeDirectionalLight(
        Dir, Color, /*Intensity*/ 4.0f);
    DescGI.LightsBuffer   = Renderer::UploadLightBuffer(NvrhiDevice, &SunLight, 1);
    DescGI.LightCount     = 1;
}
DescGI.AmbientScale     = 0.10f;
DescGI.AmbientColor[0]  = 0.75f;   // unchanged
DescGI.AmbientColor[1]  = 0.80f;   // unchanged
DescGI.AmbientColor[2]  = 1.00f;   // unchanged
DescGI.AmbientColor[3]  = 0.0f;    // unchanged
```

Verify `Renderer/Common/FLightBuilder.h` is included (add if not):
```cpp
#include "Renderer/Common/FLightBuilder.h"
```

#### Step 2: Rebuild Debug
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
```

#### Step 3: Run with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
```bash
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

#### Step 4: Check freshness + structural logs
```bash
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1               # expect std >= 0.10
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1               # expect non-uniform R/G/B min/max range
```

#### Step 5: Run validator
```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 6/6 PASS (was ~3-5/6 PASS pre-fix)
```

#### Step 6: Vision-check display frame
```bash
ls -t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open in image viewer — expect recognizable Sponza gallery + floor + directional shadow
```

#### Step 7: Mode-20 sanity (the user-specified discriminator)
```bash
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Expect: gi_raw dump is NON-UNIFORM (per-pixel albedo color, not solid zero or constant mid-gray)
```

#### Step 8: If Option A insufficient, escalate to Option B
Two-stage recipe: minimal first, then escalate. Operator runs Option A; if std < 0.10, revert and try Option B.

## Per-acceptance-criterion test design (mapping user's 7 criteria)

| # | Criterion | Test step | Cron-verifiable? |
|---|-----------|-----------|------------------|
| 1 | Debug target builds | Step 2 (`./Build.sh --Rebuild` exit code 0) | NO (operator) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | Step 3 exit code 0 + Step 4 grep returns 0 VUIDs | NO (operator); file-only-verifiable post-run via grep new log |
| 3 | No Vulkan VUID/ERROR/CommandList errors | Step 4 grep `VUID\|ERROR\|CommandList error` returns 0 | PARTIAL — file-only after operator runs |
| 4 | `validate_restir_gi.py` passes newest dump | Step 5 exit code 0 + 6/6 PASS | PARTIAL — file-only after operator runs |
| 5 | Vision: display PNG shows recognizable Sponza | Step 6 image-viewer check | NO (terminal + vision) |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | Step 7 grep gi_raw stats non-uniform + PNG non-uniform | PARTIAL — file-only after operator runs |
| 7 | All 7 acceptance criteria pass | aggregate of 1-6 | depends on operator execution |

## HARD-ENV-FINDING (cron-verified this tick)

The tester role requires shell commands (`./Build.sh`, `HLVM_*` env-var run, `grep`, `python3`, `validate_restir_gi.py`, vision via image viewer). All are routed through `terminal`. **Terminal is blocked by tirith at the security-pattern gate.**

This is the **1564th cumulative `terminal` denial** on this runspace. The host has been structurally file-only for the entire duration of this diagnostic chain (1563+ prior ticks confirmed the same).

## Verdict

**ALL_TESTS_BLOCKED.** The 8-step recipe above cannot be executed by the cron. The patch + recipe constitute a complete deliverable that an operator with terminal access closes in 2 rebuilds + ~5 minutes.

## Broken-pattern audit (file-only pre-screen)

- [ ] No `from-x-import-y` patch propagation bugs (this is C++ — N/A)
- [x] No test-bug-in-itself: the recipe is identical to v170/v171's recipes with the v172 patch applied; follows the canonical 8-step validation pattern from the lineage
- [x] No source-incomplete-relative-to-test: source-side patch is the proposal; test invocation reads binary outputs after rebuild
- [x] No missing test isolation fixture: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` env-vars are the isolation gate
- [x] No AsyncMock on sync function (N/A)
- [x] No test asserts against wrong fixture: validator script reads from `dumps/` dir directly, which is the actual output location
- [x] Test plan covers all 7 user acceptance criteria

## What the operator must do

Apply the 2-block recipe above. Report back via the next cron invocation (or by editing `PENDING_PICK.md` to mark the v170/v171/v172 cards DONE) once:
1. `validate_restir_gi.py` exits 0 with 6/6 PASS
2. vision confirms recognizable Sponza with directional shadow
3. `HLVM_PT_DEBUG_MODE=20` returns non-uniform GBufferMaterial

— tester, tick 2026-08-15, file-only, single-profile host, terminal-blocked.
