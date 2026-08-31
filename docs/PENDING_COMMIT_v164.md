# Pending Commit v164
- plan: docs/PENDING_PICK.md card 5 (skip_planning: yes; no PENDING_PLAN_v164 opened — card inherits v162 recipe per lineage)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
- source: no bundle — direct config edit per card 5 recipe
- target: current working tree (commit/push not authorized; operator owns git topology per `six-role-pipeline §Cron job configuration` + DISPATCHER_PROMPT §Hard rules)
- task: enable `HLVM_RGI_DEBUG_VIS` define on `GIPathTracing.hlsl` so the mode 20/21/22/30/31 SRV-read discriminators are compiled into the .sblob; this is the missing precondition for verifying PICK card 5 acceptance criterion #6 (`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial) by operator-side run.
- verify: after operator rebuilds, `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` should produce `dumps/<new_ts>*gi_raw_frame*.png` with non-zero, spatially-varying pixel data (mode 20 = `GBufferMaterial.Load(int3(pixel, 0)).rgb` per `Private/Renderer/Shader/GI/GIPathTracing.hlsl:747`)
- skip_impl_review: no (reviewer verifies the cfg edit landed on disk; the patch must be confirmed against the on-disk file)
- produces_test_files: no
- notes: **This v164 commit is a re-application of the v163 commit's recipe, which the tick313 honest re-verification found was NOT on disk** (`ShaderMake.cfg` line 1 read `GIPathTracing.hlsl -T lib` without `-D HLVM_RGI_DEBUG_VIS`). v163's PENDING_COMMIT_v163.md §Concrete cfg edit claimed the patch was applied via the `patch` tool and verified by reading the file post-edit, but tick313's independent `read_file` showed the flag was missing. This v164 commit APPLIES the edit again, this time with explicit pre/post `read_file` evidence captured in this file. The cause of the v163 discrepancy is not known (could be patch-tool failure, file rewrite by another cron between v163 and tick313, or fabrication in v163's verification — the v163 lineage never recaptured the file content with new evidence after the patch). The v164 edit is independently verified this tick (see "Concrete cfg edit (APPLIED this tick)" below) so it does not propagate the prior uncertainty.

## Plan Deviations (impler fills this in if it deviated)

N/A — v164 IS the v162/v163 recipe applied for the second time, this time with verified-on-disk evidence. No deviation from card 5's intent.

## Concrete cfg edit (APPLIED this tick)

**Pre-edit on-disk verification (this tick):**
- `read_file` of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` (12 lines, 318 bytes):
  - Line 1: `GIPathTracing.hlsl -T lib`
  - Lines 2-12 unchanged from v25/v161/v162/v163 lineage documented baseline

**Applied via `patch` tool on 2026-08-17 (tick314, this cron tick)**:
```
-old_string: GIPathTracing.hlsl -T lib
+new_string: GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS
```
- `patch` tool reported `success: true` and a 1-line diff.
- Files modified: 1 (the cfg file only).
- Lint: skipped (no linter for .cfg files).

**Post-edit on-disk verification (this tick):**
- `read_file` of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` (12 lines, 340 bytes):
  - Line 1: `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS` ✓
  - Lines 2-12 unchanged (verified by re-reading entire file).

## Why the cron applied this edit (file-only, no terminal required)

Per `software-development-practices §Destructive Action Protocol`, the default is "plan, present, wait" for destructive steps. The cfg edit is **not destructive**:
- It is the explicit recipe from `docs/PENDING_PICK.md` card 5 (`skip_planning: yes`) and was previously documented as the v162 commit's `## Concrete cfg edit`.
- It can be reverted by reverting line 1 of ShaderMake.cfg.
- It does not affect the runtime binary until `./Build.sh --Rebuild` is run (operator-side, terminal required).
- It is a config edit, not production code (low-blast-radius; only changes future compilation).
- It does NOT touch AGENTS.md, CLAUDE.md, .cursorrules, or any governance file.

The cron applied it to advance the state machine, reduce the operator's manual steps, and break the cycle-stop pattern that has persisted across tick1..tick313.

## Implementation status (this tick)

- [x] Pre-edit `read_file` of `ShaderMake.cfg` confirms line 1 lacked the flag (tick313 honest read replicated by this tick's pre-edit verification)
- [x] `ShaderMake.cfg` edited to add `-D HLVM_RGI_DEBUG_VIS` — APPLIED this tick via `patch`; `patch` tool reported success
- [x] Post-edit `read_file` of `ShaderMake.cfg` confirms line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS` (this tick's post-edit verification)
- [ ] Shader rebuilt — NOT YET (terminal blocked; operator-side)
- [ ] Mode-20 run — NOT YET (terminal blocked; operator-side)
- [ ] Validator 4/4 on mode-20 dump group — NOT YET (terminal blocked; operator-side)
- [ ] PENDING_PICK card 5 moved to `[x]` — NOT YET (awaits operator completion)

## Operator action required (only remaining steps)

```bash
# Step 0: Verify the cfg edit (line 1 of ShaderMake.cfg should now read with -D HLVM_RGI_DEBUG_VIS)
head -1 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
# Expected: GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS

# Step 1: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 2: Run mode-20 discriminator
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# Step 3: Validate
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 4: Numpy-check mode-20 gi_raw
python3 -c "
import numpy as np
from PIL import Image
import glob
files = sorted(glob.glob('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*gi_raw_frame*.png'))
img = np.array(Image.open(files[-1]))
print('Per-channel mean:', img[..., :3].mean(axis=(0,1)))
print('Per-channel std:', img[..., :3].std(axis=(0,1)))
print('Non-zero ratio:', (img[..., :3] > 0).any(axis=-1).mean())
"
```

Expected fresh artifacts:
- `dumps/<timestamp>_gi_raw_frame*.png` — non-zero, spatially-varying GBufferMaterial SRV read (mode 20)
- `dumps/<timestamp>_display_frame*.png` — sanity check (should still be sane exposure)
- Updated `Binary/Debug/TestReSTIR_GI_Temporal.log` — fresh validation layer enabled line + zero VUID/ERROR/CommandList
- Validator 4/4 PASS on the new dump group

If the operator-rerun mode-20 produces non-zero gi_raw, the next cron tick:
1. Upgrades v161, v162, v163, v164 audits SOME_RELAX → ALL_KEEP
2. Marks PICK card 5 `[x]`
3. PICK exhausted → Rule 10 → cycle-stop with audit per HARD INVARIANT #6

If mode-20 still produces zero (very unlikely per binding-set integrity evidence): v165 fix cycle opens with mode-20 evidence as anchor.

## Hard rules honored

- `six-role-pipeline HARD INVARIANT #3` (impler deviates and documents): no deviation from card 5's recipe.
- `software-development-practices §Destructive Action Protocol`: cfg edit is the explicit recipe from card 5 + v162; not destructive; revertible by reverting line 1.
- User task constraint "Do not commit, push, or modify governance files": honored — no commit/push (terminal blocked + per dispatcher rules); cfg edit is not a governance file.
- User task constraint "Do not use Kanban as the workflow": honored.
- User task constraint "Never fabricate": honored — pre/post edit both captured by independent `read_file` calls; the previous v163 lineage's unverified "applied" claim is explicitly flagged in `## notes` rather than silently propagated.

## Cross-references

- **v163 commit (the prior cycle that the tick313 honest re-verification found did not land)**: `docs/PENDING_COMMIT_v163.md`
- **v162 recipe (the source of the v163/v164 cfg edit)**: `docs/PENDING_COMMIT_v162.md §Concrete cfg edit`
- **v162 review**: `docs/PENDING_IMPL_REVIEW_v162.md`
- **v161 chain (the mode-20 discriminator workflow v164 continues)**: `docs/PENDING_PLAN_v161.md`, `docs/PENDING_PLAN_REVIEW_v161.md`, `docs/PENDING_COMMIT_v161.md`, `docs/PENDING_IMPL_REVIEW_v161.md`, `docs/PENDING_TESTS_v161.md`, `docs/PENDING_TEST_AUDIT_v161.md`
- **Compile-gate evidence**: `Private/Renderer/Shader/GI/GIPathTracing.hlsl:645-651` + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:645-651` + the now-edited `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` line 1
- **PICK card 5** (still `[ ]`): `docs/PENDING_PICK.md` line 7
- **Tick313 anchor audit (which discovered the v163 fabrication)**: `docs/PIPELINE_HEALTH_2026-08-17_six-role-tick313.md`
- **Authoritative current-state per user instruction**: `docs/DIAGNOSTIC_2026-07-30.md` (155 lines, 7589 bytes, INTACT)
