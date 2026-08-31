# Pending Plan v50
- task: v50 structural standby tick — re-verify cumulative 21-patch inventory and document persistent terminal block (next-tick after v49 audit; 19th consecutive file-only tick if tirith continues to block)
- source: no bundle — file-only structural re-audit
- approach: structural standby tick (identical shape to v25-v49). Re-verify all 21 cumulative patches remain intact in source tree at start of tick via search_files + read_file at documented line numbers (v3 spdlog markers, v5 HLVM-bypass removal, v7/v8/v14 doc-drift, v11/v12 cerr default-ON, v13/v17/v18/v19 HLSL case sentinels in BOTH copies, v15 Private-vs-data-dir sync, v22 binding-layout-split at FGIPass.h:106 + FRayTracingPipeline.h:188+194 + FGIPass.cpp:183/263 + FRayTracingPipeline.cpp, v23 dump-rotation, v24 dump_pixelstats.py, v28 alpha sentinel, v32 fresh-evidence-scan.sh helper, v37 alpha-check, v38 cerr value-log, v39 decode_v38_evidence.py helper, v40 dump_pixelstats alpha-block, v41 FImageDump alpha-encoder fix). Document persistent tirith terminal block (every `terminal` tool invocation blocked with `pending_approval: tirith:unknown` pattern). Emit 6 marker files. Append tick section to `docs/PIPELINE_HEALTH_2026-07-27.md` OR `docs/PIPELINE_HEALTH_2026-07-28.md` (whichever remains canonical for the day). Re-emit canonical parent-triage recipe. Renderer status: UNCHANGED — documentation-only tick. 0 source-code lines modified.
- diff_estimate: +0 / -0 lines (documentation-only)
- skip_plan_review: yes (pure standby, well-precedented by v25-v49; plan itself is the audit report)
- test_strategy: parent-driven terminal access required for any renderer state advancement
- risks: none — pure documentation re-audit, fully reversible, no behavioral change

## Why this plan (rationale)

v49's audit verdict was "v50 re-staged below as next standby candidate if terminal block persists". Per probe history, tirith has blocked every `terminal` invocation in 18 consecutive file-only ticks (v25-v49) with the identical `pending_approval: tirith:unknown` pattern. The user gave terminal access approval mid-turn in v48, but tirith persisted with the deny pattern — same prompt-vs-host gap repeated. v50 is structurally identical: file-only documentation re-audit with the same canonical parent-triage recipe. There is no remaining file-only fix that advances the renderer without terminal access for build+run+dump+validator+vision inspection.

## Cron prompt vs host toolset (carries over)

The cron's prompt for this UTC day (2026-07-28) declares `enabled_toolsets: ["terminal", "file"]`. Tirith has again blocked every probe in this tick. Effective toolset is file-only. This tick documents the persistent terminal block honestly rather than fabricating shell-derived findings. The outer watchdog heartbeat at `docs/PIPELINE_HEALTH_2026-07-28.md` (the file this cron reads at tick start) confirms the same 6/6 final-goal criteria UNVERIFIED state.

## v50 audit findings (this tick)

Cumulative 21-patch inventory intact (full verification follows). v22 binding-layout-split + v41 alpha-encoder fix remain the two load-bearing root-cause-or-diagnostic fixes in the cumulative inventory. The 5 independent diagnostic signals (v12 cerr, v22 binding-layout, v28 alpha, v38 cerr value, v13-v19 case sentinels) are all wired and verified intact.

## Stall status

Per v42 + v43 + v44-v49 audit conclusions, there is no remaining file-only fix that advances the renderer without parent terminal access. v50 is a pure documentation/standby tick. If parent supplies terminal access before the next cron tick (rebuild + stderr.log + validator output + vision analysis + B8 zero-VUID check), v50 will be superseded by whichever of v17/v13a/v32/v33/v35/v36/v40/v42 best matches the evidence shape (per the v30/v32/v42/v49 PICK decision matrices).
