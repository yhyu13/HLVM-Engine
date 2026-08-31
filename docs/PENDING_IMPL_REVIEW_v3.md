# Pending Impl Review v3

- plan: docs/PENDING_PLAN_v3.md
- commit: docs/PENDING_COMMIT_v3.md
- verdict: KEEP
- reviewer: impler+reviewer (single-profile host; same head)
- timestamp: 2026-08-17 (estimated wall-clock; cron session)

## plan_fidelity_check

The plan called for three code changes:
1. Extend handle-id logging from 4 frames to 64 frames.
2. Add a sentinel-compare debug mode (HLVM_PT_DEBUG_MODE=23) that
   distinguishes handle mismatch from descriptor mismatch in a single
   dump.
3. Stage the v3 commit BUT NOT APPLY until the operator verifies v2.

The commit is staged, not applied. This is correct: the plan explicitly
says "DO NOT APPLY until operator reports whether v2 fixed the bug." If
v2 worked, the v3 changes are unnecessary and applying them would be
debug code in shipping tests.

The staged diff summary is mechanically correct:
- `if (FrameIndex < 4u)` → `if (FrameIndex < 64u)` is a one-character
  change to two files (FGIPass.cpp + TestReSTIR_GI_Temporal.cpp). It
  gets us the handle-id log for all 8 accumulated frames.
- The mode-23 case in GIPathTracing.hlsl adds the sentinel-compare read
  pattern. Both HLSL copies need editing to stay in sync.
- The sentinel upload in TestReSTIR_GI_Temporal.cpp must be gated behind
  HLVM_PT_DEBUG_MODE=23.

All three are minimal, debug-only changes. The plan is followed faithfully.

## TDD evidence

- [ ] Test file present: N/A — no test file added; this is a diagnostic
      instrumentation patch, not a feature.
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A

The change is **diagnostic instrumentation**, not test-driven in the
classical sense. The "test" is the operator running the build with
HLVM_PT_DEBUG_MODE=20/23 and inspecting the resulting logs/dumps.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection
- [x] No eval/exec
- [x] No SQL injection

Pure C++/HLSL debug instrumentation with no security implications.

## Self-review checklist

- [x] Validation: the mode-23 sentinel-compare is gated behind the env
      var check pattern (`if (getenv("HLVM_PT_DEBUG_MODE") && ...)`)
      so production users don't hit it.
- [x] Error handling: the sentinel upload uses the same
      createCommandList / executeCommandList / waitForIdle pattern that
      the codebase already uses (see
      `FGIPass::UploadLights` at line 411-416 for the canonical recipe).
- [x] Tests: existing modes 20/21/22/30/31 are the regression tests;
      mode 23 is a NEW diagnostic, not a replacement.

## Concerns

1. **Operator-execution gap.** The v3 commit is staged but not applied,
   AND even if applied, the build/run/inspect cycle is operator-side
   (terminal blocked in cron profile). This card cannot close without
   the operator's intervention. The skill's HARD INVARIANT #6 says
   "Never silently exit" — this audit is the loud-non-silent exit for
   the cron tick.

2. **The handle-id extension (`< 4u` → `< 64u`) is debug-only and must
   be reverted before any production commit.** If the operator
   verifies v2 worked, the v3 changes never land; if v3 lands, the
   reviewer should flag this in v4's IMPL_REVIEW.

3. **Sentinel-upload CPU upload is the "sentinels-then-overwrite"
   pattern from the skill's gotchas.** Specifically:
   - "Sentinels can mask real GPU writes if left enabled in shipping
     code." → mitigated by gating behind `HLVM_PT_DEBUG_MODE=23`.
   - "Sentinels can corrupt downstream shader reads even when the dump
     looks correct." → mitigated by the same gating.
   - "CPU-upload transitions can race with subsequent GPU work." →
     mitigated by the createCommandList / executeCommandList /
     waitForIdle pattern.

4. **Mode 23 sentinel-compare depends on the raster pass NOT overwriting
   the sentinel.** If the raster pass writes magenta-colored worldpos
   data (which is unlikely but possible), the sentinel flag would be
   false-positive. The mitigation: only use the sentinel flag for the
   GBufferWorldPos texture (where the sentinel is a CPU-uploaded
   constant, not a raster-pass write). The raster pass writes
   worldpos from the actual mesh, so its value won't be magenta.

5. **HLSL dual-copy drift.** As noted in the plan review, mode 23 must
   be added to BOTH HLSL copies. The commit message makes this
   explicit; the impler must not forget.

## Feedback for impler (FIX only)

None — verdict is KEEP with the caveats above. Move to tester (but
note: tester cannot execute; the deliverable is staged code + clear
operator instructions).

## Next role

Tester (file-only): cannot run the build. The "test" is operator-side
execution per PENDING_TESTS_v2.md's 5-test plan, with the addition of
test 6 (mode 23 sentinel-compare) once v3 lands.

## Operator action required (this is the hard blocker)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Step 1: Verify v2 first (cheapest measurement)
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test 2>&1 | tee /tmp/rgi_v2_build.log

# Step 2: Run with mode 20 (GBuffer SRV sentinel)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 HLVM_RGI_DIAG=1 \
    ./Binary/Debug/.../TestReSTIR_GI_Temporal 2>&1 | tee /tmp/rgi_mode20.log

# Step 3: Inspect dumps
python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('<DUMP_DIR>/gi_raw_frame8.png'))
print(f'per-channel std = {img[:,:,0].std():.2f}, {img[:,:,1].std():.2f}, {img[:,:,2].std():.2f}, {img[:,:,3].std():.2f}')
print(f'unique values per channel = {len(np.unique(img[:,:,0]))}, {len(np.unique(img[:,:,1]))}, {len(np.unique(img[:,:,2]))}, {len(np.unique(img[:,:,3]))}')
"

# Step 4a: If v2 worked (per-channel std > 5/255): close the card.
# Step 4b: If v2 didn't work: apply PENDING_COMMIT_v3.md changes,
#   rebuild, run with HLVM_PT_DEBUG_MODE=20 (now with full-frame
#   handle-id logs) and HLVM_PT_DEBUG_MODE=23, and report the
#   handle-id mismatch + sentinel-flag results.
```

This is the work that the cron tick CANNOT perform. The skill explicitly
describes this pattern in `references/gpu-rendering-repair.md` (referenced
in the six-role-pipeline skill): "the parent's PICK body says something
like 'autonomous until complete', 'fix until validator passes', or 'the
agent must actually run the test' ... terminal-enabled is necessary when
the work cannot be meaningfully verified without execution." The cron
profile here is file-only; the operator session is terminal-enabled.
The pipeline's value is in the design + audit, not the execution.
