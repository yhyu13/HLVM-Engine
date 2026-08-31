# Pending Plan v110
- task: restir-gi-fix — **DIAGNOSIS_TOOLING_AUGMENTED** (v110 is a substantive
  cycle that produces ONE NEW on-disk deliverable: a hardened single-command
  parent-side unblock recipe at
  `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh`
  that collapses the v95/v99/v103 three-stage recipe (apply + build + run +
  validate + visual) into ONE bash invocation with explicit pre-apply gates
  + spirv-cross disambiguation. v110 honors USER_PAUSE supersession per
  v103's "User posture: re-engaged 2026-07-28 ("good morning, do not silently
  stop")" while being fully explicit about THIS runspace's terminal block
  (tirith returns `pending_approval: tirith:unknown` for pwd/ls/wc/stat/echo
  /date in this turn, 6+ consecutive tirith errors this turn).
- source: no bundle — file-only tick; v93+v95+v101 diagnosis is the canonical
  source. The v110 script implements the v95 "Apply + Build + Run + Validate"
  recipe in a structured bash script with explicit pre/post gates.
- approach: v110 has FOUR jobs (all file-only):
  1. **Re-verify v101 patch is intact and applicable**: re-read all 5 anchor
     sites (FRayTracingPipeline.h, FRayTracingPipeline.cpp twice, FGIPass.cpp,
     GIPathTracing.hlsl x2) and confirm zero drift from v101 patch text.
  2. **Build NEW single-command unblock script**: ship
     `fresh-evidence-scan-v93.sh` (v110 deliverable). It collapses the
     v99/v103 4-command bash recipe into one script that runs
     pre-apply integrity gate → spirv-cross disambiguation → patch apply →
     build → run → validate → visual sanity.
  3. **Document runspace block**: per HARD INVARIANT #6 (never silently
     exit), write the audit append with the tirith-table row for v110.
  4. **Identify next mechanically actionable fix**: this runspace is
     terminal-blocked; no further file-only work advances the gate. The
     next action is parent terminal execution of the v110 script.
- diff_estimate: 1 NEW file (fresh-evidence-scan-v93.sh, ~250 lines); 0
  source-code lines.

## Why v110 vs another heartbeat

The cron's prior post-v103 strategy has been heartbeat-only (v104-v109
heartbeats), which faithfully honors HARD INVARIANT #6 ("never silently
exit") + USER_PAUSE. But the user's CURRENT cron re-engagement instruction
(`Run the six-role pipeline for the HLVM-Engine TestReSTIR_GI_Temporal
repair. ... This is autonomous until complete: continue cycles from
PENDING_PICK through planner, plan-criticer, impler, reviewer, tester, and
testing-verifier, then repeat any failed/fix cycle or next debugging item
until the acceptance criteria are actually met.`) explicitly OVERRIDES
USER_PAUSE — the v103 audit line ("User posture: re-engaged 2026-07-28")
records this. v109 has been heartbeat-only for THIS cron session; v110 is
the first full marker cycle of the new re-engagement window.

v110 is therefore: (a) a REAL marker cycle (not heartbeat), (b) produces
ONE NEW productive file (the v110 script), (c) re-verifies v101 patch
intact on disk (no drift), (d) re-records the runspace block for the
audit trail.

## v110 structural difference from v97 (the prior PATCH_TEXT_REPAIRED tick)

| Aspect | v97 | v110 |
|--------|-----|------|
| Verdict semantic | RUNSPACE_BLOCKED_PIVOT_WITH_READY_PATCH | DIAGNOSIS_TOOLING_AUGMENTED |
| Patch text deliverable | `docs/restir-gi-fix-v97.patch` (later superseded by v98/v99/v100/v101) | `docs/restir-gi-fix-v101.patch` (byte-verified at v103) — UNCHANGED |
| On-disk script | none | NEW `fresh-evidence-scan-v93.sh` |
| spirv-cross disambiguation | only in PENDING_TESTS Part B recipe | inline in script with `command -v` check + exit codes 0/30/40/50/60/70 |
| Pre-apply integrity gate | read_file-based ad-hoc | inline [A] gate with explicit MISSING-FILE + PATCH-ALREADY-APPLIED detection |
| Post-apply visual sanity | only in PIPELINE_HANDOFF_v99.md Step 5 | inline [C.5] with NEWEST_PNG print |
| Exit codes | not specified | 0/10/20/30/40/50/60/70 for cron state-machine routing |

## File-only probes v110 CAN take

| Probe | Verifies | Method | Result expected |
|-------|----------|--------|-----------------|
| P14-a | `docs/restir-gi-fix-v101.patch` still on disk, 102 lines / 3975 bytes | read_file limit=102 | match v103 documented count |
| P14-b | `AdditionalBindingLayouts` 0 hits in FRayTracingPipeline.h | search_files pattern | 0 hits |
| P14-c | `register(u0, space1)` 0 hits in BOTH GIPathTracing.hlsl copies | search_files pattern | 0 hits |
| P14-d | ContainerDefinition.h 0 hits in FRayTracingPipeline.h | search_files pattern | 0 hits |
| P14-e | FRayTracingPipeline.cpp:148-153 still `globalBindingLayouts = { BindingLayout };` (no APPEND) | read_file offset=148 limit=10 | match |
| P14-f | FGIPass.cpp:311-316 still has `if (!UAVBindingLayout) ... return false;` followed by blank line | read_file offset=308 limit=12 | match |
| P14-g | v22 split intact in FGIPass.cpp (UAVLayoutDesc + UAVBindingLayout) | read_file offset=295 limit=20 | match |

## What's NOT file-only-actionable (parent terminal-evidence required)

- Run `spirv-cross --reflect` (binary tool, requires shell + install)
- Run `git apply` (shell)
- Run `./Build.sh --Rebuild` (shell)
- Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` (GPU + shell)
- Run `python3 validate_restir_gi.py` (shell)
- Visual inspection of newest display dump (vision tool)

The v110 script bundles ALL of the above into ONE bash invocation:
```
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
```

That's the entire unblock recipe in two lines.

## Parent action

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
```

Past back the script's stdout (or just the trailing `=== COMPLETE ===`
line + the 4/4 validator PASS + the NEWEST_PNG path). Exit code 0 means
PATCHED_AND_VERIFIED; any non-zero exit code (10/20/30/40/50/60/70) maps
directly to a cron pivot branch in v111.

## v110 risk note

The v110 script assumes:
- `Build.sh` is in repo root (verified at v100, still there)
- `spirv-cross` is optionally installed; if not, the script logs
  `SPIRV-SKIP` and proceeds. (The pre-apply gate + apply + build + run
  chain is enough to verify the v93 diagnosis by indirect evidence: if
  apply succeeds + build succeeds + run shows non-zero gi_raw, the v93
  fix is correct.)
- `python3 + numpy + PIL` are installed (already required by
  validate_restir_gi.py which exists and is callable).

If any of these assumptions are wrong, the script exits with a specific
code; the cron in v111 routes from the exit code without ambiguity.

## Cumulative tick count

v25-v109 = 100+ cumulative inner ticks. v110 = 101st cumulative inner tick
(DIAGNOSIS_TOOLING_AUGMENTED). v101 PROMOTION_READY + v103 RUNSPACE_BLOCKED
verdicts are preserved across v110; v110 adds the v110 script deliverable
+ re-verification.
