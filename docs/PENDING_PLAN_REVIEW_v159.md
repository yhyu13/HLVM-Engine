# Pending Plan Review v159
- plan: docs/PENDING_PLAN_v159.md
- verdict: KEEP
- reviewer: plan-criticer (single-profile self-check)
- timestamp: 2026-08-09T[current-tick]Z

## Design soundness
The v159 plan extends the v155/v156/v157/v158 cycle-stop lineage with a new on-disk evidence channel: the case-label liveness check (via `spirv-cross --reflect GIPathTracing.spv | grep -iE 'case|OpSwitch|OpSelectionMerge'`). This is a targeted narrowing of the remaining 3 hypotheses (slangc dead-strip / image layout / nvrhi binding drop) toward hypothesis (2)/(3) or confirmation of (1). The v158 handle-identity check already falsified hypothesis #4 (stale handles); the v159 case-label check would partially falsify hypothesis #1 (slangc dead-strip). The plan is verification-only — no source change — and adds no new ops requirements beyond the operator-runspace commands the lineage already enumerates.

## Plan completeness
Complete for a terminal-enabled verification pass. The plan names the target (`TestReSTIR_GI_Temporal`), exact environment variables (HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8, HLVM_PT_DEBUG_MODE=20), the validator (validate_restir_gi.py), the newest-group constraint, the log error patterns (VUID/ERROR/CommandList), numpy/statistics requirement, visual inspection requirement, and the new case-label liveness check (spirv-cross --reflect).

## Feasibility check
Source-side fixes (v22 binding-layout split, v137 binding-offset zero, v140 AmbientColor override, v151 ReSTIR layout split) are all INTACT on disk per direct read_file this tick (verified v158 lineage and re-confirmed). The 2026-08-08 17:30 fresh log shows handle identity: Material=0x3cbc40c9300, WorldPos=0x3cbc40c6040, Normal=0x3cbc40c8c00 in BOTH `RenderGBuffer` and `FGIPass::DispatchRays` (v158 falsification). The scheduled runspace's terminal remains blocked (`pending_approval` / `tirith:unknown` / "Security scan: security issue detected" on every probe this tick too), and no vision tool is registered. Those are execution-environment blockers, not plan defects; the plan correctly requires reporting them instead of substituting historical or predicted results.

## Single-profile caveat
This pipeline runs on a single worker profile. The plan-criticer and reviewer verdicts are self-checks, not independent reviews. The honest read of the on-disk evidence (v137+v140+v151 source-side fixes intact; 17:28 non-bypass log shows ReSTIR success; 17:30 bypass log shows clean dispatch with non-zero gi_raw; 0 VUID/ERROR/CommandList lines in both; handle-IDs match across the raster → GI boundary which falsifies the 2026-07-30 hypothesis #4) is that the bisect has reached the end of what file-only work can verify. Closing the remaining 2 acceptance criteria (vision-check, fresh non-bypass mode-20 run) requires an operator runspace with terminal+vision+python3+numpy+spirv-cross.

## Feedback for planner (FIX only)
Not applicable.
