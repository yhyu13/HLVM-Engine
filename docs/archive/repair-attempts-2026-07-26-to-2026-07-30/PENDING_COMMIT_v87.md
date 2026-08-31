# Pending Commit v87
- plan: docs/PENDING_PLAN_v87.md
- files: 0 source-code files (verification-only cycle); 6 v87 marker files + 1 PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md + 1 PENDING_PICK.md update + 1 PIPELINE_HEALTH_2026-07-28.md append
- source: no bundle — direct edit (parent terminal access required for build/run/validate/vision; structurally blocked in this cron runspace)
- target: worktree-only (no git operation; cron directive: do not commit/push/rewrite history)
- task: restir-gi-fix — One fresh Part A probe at the gi_raw read site + explicit runspace-blocked escalation
- verify: `cat docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md docs/PENDING_PLAN_v87.md docs/PENDING_PLAN_REVIEW_v87.md docs/PENDING_COMMIT_v87.md docs/PENDING_IMPL_REVIEW_v87.md docs/PENDING_TESTS_v87.md docs/PENDING_TEST_AUDIT_v87.md` (parent may read these in any interactive session to see what v86/v87 cycles produced)
- skip_impl_review: no (impl-review covers the Part A finding — reviewable in this runspace)
- produces_test_files: no
- notes: Part A spot-check found a NEW diagnostic comment in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:1695-1703` that the v25-v86 records had not surface. The comment says: "If gi_raw = 0 but this log shows the right handle, the GI pass's write was dropped by something OTHER than the (now-removed) HLVM-bypass — check v3 ENTER/EXIT to confirm the dispatch body was reached and the v5 NOTE near line 1521 for the current RenderGBuffer shape." This means the 2026-07-25 fixes (2fab7d6 + e6b3d52 + aa2cc53 + v22 binding layout split) targeted the bug class "GI pass produces magenta noise" but the **CURRENT** symptom is the DIFFERENT bug class "GI pass produces all zeros, dispatched correctly but body never writes." This is the finding worth surfacing to parent — the 22-patch inventory may not cover the current bug.

## Part A probe detail

Probed site: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:1695-1703` — diagnostic comment in `DumpRGBA32FTexture`.

What the probe found:
- The dumper (`DumpRGBA32FTexture`) is correct. It opens a fresh `CommandList`, transitions Texture→CopySource, `copyTexture(staging, texture)`, closes the CL, executes, waitForIdle, then maps the staging texture and prints per-channel min/max.
- The "gi_raw R[0,0,0] G[0,0,0] B[0,0,0]" log line at `TestReSTIR_GI_Temporal.log:76` means `MinR == MaxR == 0.0`, `MinG == MaxG == 0.0`, `MinB == MaxB == 0.0`. i.e., every pixel of the staging texture reads as the literal float `0.0` after the staging copy.
- The dumper is NOT the bug. The dump-encoding, per-channel normalization, commandlist lifecycle, and `DumpToPNG` clamp are all correct (recent fixes `2fab7d6`, `e6b3d52`).
- The bug is upstream of the dumper: the GI pass must be writing `OutputTexture` as all zeros, OR the copyTexture is reading from a cleared/default-zero texture (which would happen if the FGIPass's dispatch was skipped entirely, or the bind layout doesn't include the output UAV correctly).
- The comment at line 1695 names the precise next diagnostic: "v3 ENTER/EXIT [binding-set log] to confirm the dispatch body was reached and the v5 NOTE near line 1521 for the current RenderGBuffer shape." `v3` and `v5` are diagnostic markers left from the 2026-07-25 session. The reviewer should check whether these v3/v5 NOTE markers still exist in this code (they may have been removed during fix cleanup).

This is a NARROWING, not a fix — actual repro requires terminal access.

## What this commit does NOT do (consistent with cron's "do not loop indefinitely")
- Does NOT bump any lighting constant.
- Does NOT re-add `WriteGBufferSentinels`.
- Does NOT change worldpos dumper.
- Does NOT modify source code (0 lines, by design).
- Does NOT claim gi_raw is fixed.
- Does NOT fabricate KEEP.

## What the parent should do with this finding
Per the v82 BLOCKER 4-command recipe, but specifically focused on the upstream-of-dumper question: the parent should run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` to confirm 22-patch inventory intact, then run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>TestReSTIR_GI_Temporal_stderr.log` and look for:
1. Any "ENTER DispatchRays" log printed by FGIPass (the v3 marker, if it still exists) — its ABSENCE proves the dispatch body was skipped.
2. Any "EXIT DispatchRays" log.
3. Whether the OutputTexture SRV/UAV binding is set up correctly (check that the v22 split is in effect — should produce 0× `VUID-VkDescriptorImageInfo-imageLayout-00344` warnings).
4. The dumper's `gi_raw normalized per-channel` per-channel min/max values.

If (1) is ABSENT and (4) is 0,0,0: the bug is "dispatch body never ran." Most likely fix-candidates: OutputTexture UAV binding missing/wrong slot, OR DispatchRays dispatch was dropped because of a CommandList lifecycle issue (still bug-088-family even after 9a09df2). If (1) is PRESENT and (4) is 0,0,0: the bug is "dispatch body ran, all output writes produced zero — likely NVRHI binding descriptor layout desync." If (1) is PRESENT and (4) is nonzero garbage: the bug is downstream and probably covered by the 22-patch inventory.

The cron itself cannot determine which branch — only the parent's terminal output can.
