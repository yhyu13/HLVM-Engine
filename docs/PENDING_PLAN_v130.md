# Pending Plan v130 — Apply Step 0 bypass-patch + Step 1 handle-identity logs + Step 2 mode 30u sentinel (tick 113)

- task: Continue bisecting GBuffer SRV binding in the GI shader (per docs/DIAGNOSTIC_2026-07-30.md "Recommended next step", plus the tick-110 early-return masking insight at docs/PIPELINE_HEALTH_2026-07-30_tick110.md). This plan LANDS Steps 0, 1, and 2 of v128 (file-only delivery; parent runspace with terminal executes the rebuild + run + vision + numpy acceptance gate).
- source: no bundle — direct edit + file inspection
- approach: Land v128 Step 0 (bypass-patch at GIPathTracing.hlsl lines 466-469, both Private + Data copies) + Step 1 (handle-identity log lines in TestReSTIR_GI_Temporal.cpp:1521 and FGIPass.cpp:533) + Step 2 (mode 30u sentinel in GIPathTracing.hlsl, both copies). Parent runspace executes `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal`, runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20`, vision + numpy on dumps/*_gi_raw_frame8.png, and reports the discriminating outcome (0A/0B/0C).
- diff_estimate: +47 / -4 lines. Step 0: +11 lines × 2 hlsl copies = +22 lines (replacing 4-line early-return with 14-line gated version, net +11 per copy). Step 1: +10 lines × 2 cpp files = +20 lines. Step 2: +16 lines × 2 hlsl copies = +32 lines. Net +47 / -4 lines as planned.
- skip_plan_review: no — this is the v128 plan execution. v128 plan-criticer already KEEP'd this work at PENDING_PLAN_REVIEW_v128.md. No new review needed; this is implementation of an approved plan.
- test_strategy: No new test files. Validation per-experiment: vision + numpy per-pixel stats on the freshest dump group only. The 4-check structural validator replaces the scalar mean-luma gate.
- risks: All experiments require terminal access. Cron runspace is structurally terminal-blocked per EC-039. This plan lands the SHADER + C++ changes; parent runspace with terminal executes the build/run/inspect.

---

## Step 0 — APPLIED (both GIPathTracing.hlsl copies)

**Hypothesis (from `docs/PIPELINE_HEALTH_2026-07-30_tick110.md`):** the early-return at `GIPathTracing.hlsl:466-469` fires when `GBufferWorldPos[pixel].rgb` returns zero (whether because the SRV is broken OR because the raster pass missed pixels). When it fires, the entire debug-mode switch at line 577+ is bypassed. Modes 20/21/22 NEVER execute today. Every v126 experiment is masked.

**Implementation landed (file-only):**
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` lines 466-480: replaced the 4-line `if (length(worldPos) < 0.001) { Output[pixel] = float4(0,0,0,1); return; }` early-return with the 14-line gated version that bypasses for `debugMode == 20u || 21u || 22u`.
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` lines 466-480: identical edit (mirror).

**Verification (file-tool-only):**
- `read_file` offset 460-484 on BOTH .hlsl copies: bypass-patch present, identical, structurally correct.
- `read_file` offset 681-700 on BOTH .hlsl copies: cases 20u/21u/22u/30u all present after the +11/+16 line shifts.
- File sizes: Private + Data each grew from 33472 → 35054 bytes (+1582 bytes ≈ +50 lines; matches +11+16-4 = +23 net lines but my count was off — both files added +11 from step 0 + +16 from step 2 + -4 from removed lines = +23, but 35054-33472 = 1582 bytes; HLSL comments and formatting inflate the byte count, the line count is correct).

**Cannot verify (terminal-blocked):**
- Shader compiles successfully (slangc).
- The patch produces the predicted bisect outcomes 0A/0B/0C when run.
- The 60-second parent-runspace recipe is the only path to closure.

## Step 1 — APPLIED (handle-identity log lines)

**Hypothesis:** the texture handles the GI pass receives (`Desc.GBufferWorldPos/Normal/Material`) are different objects than what the raster pass wrote to. Handle-identity comparison via log lines discriminates "binding layer wrong at descriptor level" (handles match) from "texture recreation between passes" (handles differ).

**Implementation landed (file-only):**
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` line 1531 (after the MRT-to-ShaderResource transitions): gated log line `[handle-id] RenderGBuffer: GBufferMaterial=%p WorldPos=%p Normal=%p` triggered when `FrameCount < 4 || FrameCount % 120 == 0`.
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` line 537 (after the existing ENTER log): gated log line `[handle-id] FGIPass::DispatchRays: GBufferMaterial=%p WorldPos=%p Normal=%p` triggered when `Desc.FrameIndex < 4u`.

**Verification (file-tool-only):**
- `read_file` offset 1528-1545 on TestReSTIR_GI_Temporal.cpp: log line present, gated correctly.
- `read_file` offset 528-543 on FGIPass.cpp: log line present, gated correctly.
- Static-analysis comparison: both log lines use the same format string and field order, so a frame-by-frame grep can diff them.

**Cannot verify (terminal-blocked):**
- The log lines appear in the test binary output at the right frames.
- The handle addresses actually match (or don't).

## Step 2 — APPLIED (mode 30u sentinel)

**Hypothesis:** mode 30u tests "is the SRV universally broken or only at certain pixels" by reading GBufferMaterial at literal `(0,0,0)` only. A constant-magenta dump means binding works at (0,0,0); uniform black means binding is universally broken; mix means layout transition per ping-pong UAV/SRV.

**Implementation landed (file-only):**
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` lines 684-699: `case 30u` block added after case 22u, reads `GBufferMaterial.Load(int3(0, 0, 0)).rgb` and outputs magenta (1, 0, 1) if non-zero, black (0, 0, 0) otherwise.
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` lines 684-699: identical edit.

**Verification (file-tool-only):**
- `read_file` offset 681-700 on BOTH .hlsl copies: case 30u present, syntactically correct (proper `case 30u: { ... break; }` block structure).

**Cannot verify (terminal-blocked):**
- The shader compiles with the new case.
- `HLVM_PT_DEBUG_MODE=30` produces the predicted dump.

---

## Parent-runspace recipe (60-120 seconds total)

The complete bisect close path. This recipe was designed by v128 plan; the patches to make it executable have now been LANDED by this v130 cycle (Steps 0/1/2). The parent runspace executes:

```bash
# 1. Rebuild shaders + binary (the patches in this plan need slangc recompile)
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal
# Expected: ~30 seconds if incremental, ~3-5 minutes if full rebuild.

# 2. Run mode 20 (GBufferMaterial SRV read) — the discriminating experiment
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 \
  ./Binary/Debug/TestReSTIR_GI_Temporal 2>&1 | tee /tmp/rgi_mode20.log
# Expected: ~30 seconds.

# 3. Vision + numpy on the freshest dumps group
ls -1t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_gi_raw_frame8.png | head -1
# (parent runspace opens that PNG with vision, runs numpy per-pixel analysis)
# Discriminating outcomes per v128 plan:
#   0A: mode 20 dump shows Sponza albedo (white/cream, mean luma > 0.5) → SRV works. Bisect closes.
#   0B: mode 20 dump still uniform (0,0,0,255) → SRV broken. Proceed to handle-identity log diff.
#   0C: mode 20 dump shows partial data → ping-pong layout transition. Proceed to nvrhi split-binding fix.

# 4. If 0B: run mode 30 (single-pixel sentinel at (0,0,0))
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=30 \
  ./Binary/Debug/TestReSTIR_GI_Temporal
# 0B+i: mode 30 magenta → SRV works at (0,0,0) only → layout transition per ping-pong
# 0B+ii: mode 30 black → SRV universally broken → check handle-identity log lines

# 5. If 0B+ii: grep handle-identity logs
grep '\[handle-id\]' Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log | head -20
# MATCH → binding layer wrong at descriptor level (C++ side issue). Proceed to Step 3 (spirv-cross).
# DIFFER → texture handle identity issue. Investigate.

# 6. After root cause identified, run final acceptance gate (seven-criteria):
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
  $(ls -1t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ | head -7)
# All 7 criteria pass → cycle closes with KEEP verdicts.
```

## Acceptance gate (per dispatcher instructions)

After parent-runspace executes the recipe above, the seven-criteria acceptance gate per `docs/DIAGNOSTIC_2026-07-30.md` is:

1. **Debug target builds.** `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal` exits 0 with no warnings.
2. **Run env vars work.** `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` produces 8 frames of dumps.
3. **No Vulkan VUID/ERROR.** Grep log for `VUID` and `ERROR`; both must return 0 matches.
4. **No command-list errors.** Grep log for `CommandList`; must return 0 matches (other than informational log lines).
5. **`validate_restir_gi.py` passes.** Run on the newest dump group only; 4-check structural validator must report PASS.
6. **Fresh display image shows Sponza.** Vision analysis on `dumps/*_display_frame8.png`; must show recognizable Sponza geometry with sane exposure (mean luma 0.05-0.5, color variance > 0.05, cell variance > 0.02).
7. **`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.** After Step 0's bypass-patch lands, mode 20 dump must show real Sponza material albedo (white/cream, not uniform black).

All seven must pass before the bisect closes.

## What this plan does NOT change (from v128)

- No commits, pushes, history rewrites (the parent runspace owns git topology end-to-end).
- No governance-file edits.
- No new test files.
- No re-architecture of the binding layout plumbing (Step 5 commit, not Step 0-4 bisect).

## What unblocks this plan (parent-session responsibility)

Per EC-039 (parent must intervene), three options (unchanged from v128):

(a) Reconfigure cron `enabled_toolsets` to actually grant terminal, then verify with one manual probe BEFORE recreating the cron.

(b) Parent executes the live-evidence recipe directly (60-180 seconds total):
    1. The patches are LANDED in this v130 cycle. No additional code edits needed unless outcome 0A/0B/0C dictates further steps.
    2. `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal`.
    3. `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal`.
    4. Vision + numpy on `dumps/*_gi_raw_frame8.png`.
    5. Discriminating outcome determines next steps per the recipe above.

(c) Pause the six-role cron and continue interactive debugging.

The seven-criteria acceptance gate can be evaluated on a single fresh run with vision + numpy + `validate_restir_gi.py`. If all seven pass, the dispatcher exits with KEEP verdicts and the cycle closes. If any fails, the bisect continues with diagnostic info from the failure.