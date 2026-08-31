# Pending Plan v49
- task: v49 structural standby tick — re-verify cumulative 21-patch inventory and document persistent terminal block (next-tick after v48 audit)
- source: no bundle — file-only structural re-audit
- approach: structural standby tick (identical shape to v25-v48). Re-verify all 21 cumulative patches remain intact in source tree at start of tick via search_files + read_file at documented line numbers (v3 spdlog markers, v5 HLVM-bypass removal, v7/v8/v14 doc-drift, v11/v12 cerr default-ON, v13/v17/v18/v19 HLSL case sentinels in BOTH copies, v15 Private-vs-data-dir sync, v22 binding-layout-split at FGIPass.h:106 + FRayTracingPipeline.h:188+194 + FGIPass.cpp:183/263 + FRayTracingPipeline.cpp, v23 dump-rotation, v24 dump_pixelstats.py, v28 alpha sentinel, v32 fresh-evidence-scan.sh helper, v37 alpha-check, v38 cerr value-log, v39 decode_v38_evidence.py helper, v40 dump_pixelstats alpha-block, v41 FImageDump alpha-encoder fix). Document persistent tirith terminal block (every `terminal` tool invocation blocked with `pending_approval: tirith:unknown` pattern). Emit 6 marker files. Append tick section to `docs/PIPELINE_HEALTH_2026-07-27.md`. Re-emit canonical parent-triage recipe. Renderer status: UNCHANGED — documentation-only tick. 0 source-code lines modified.
- diff_estimate: +0 / -0 lines (documentation-only)
- skip_plan_review: yes (pure standby, well-precedented by v25-v48; plan itself is the audit report)
- test_strategy: parent-driven terminal access required for any renderer state advancement
- risks: none — pure documentation re-audit, fully reversible, no behavioral change

## Why this plan (rationale)

After v41 (FImageDump alpha-encoder fix) the file-only diagnostic surface is genuinely complete and the cron's file-only work space is exhausted:

- v3 spdlog diagnostic markers at FGIPass.cpp + TestReSTIR_GI_Temporal.cpp (5 sites)
- v5 HLVM-bypass removal at TestReSTIR_GI_Temporal.cpp (8-line NOTE comment block)
- v7/v8/v14 doc-drift cleanup at lines 408/650-672/662/1537/1685-1693
- v11/v12 cerr writes default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487
- v13/v17/v18/v19 HLSL case sentinels in BOTH GIPathTracing.hlsl copies (Private master + Data-dir)
- v15 Private-vs-data-dir sync (single source-of-truth now)
- v22 binding-layout-split at FGIPass.h:106 + FRayTracingPipeline.h:188+194 + FGIPass.cpp:183/263/283 + FRayTracingPipeline.cpp — root-cause hypothesis fix
- v23 dump-rotation archive-after-run pattern at run_rgi_diagnostic.sh
- v24 dump_pixelstats.py helper script
- v28 alpha-channel alive-sentinel at GIPathTracing.hlsl:692 in BOTH copies
- v32 fresh-evidence-scan.sh helper script
- v37 alpha-check in validate_restir_gi.py
- v38 cerr value-log at FGIPass.cpp:477-491
- v39 decode_v38_evidence.py helper script
- v40 dump_pixelstats alpha-block extension
- v41 FImageDump.cpp alpha-encoder fix (the LAST mechanically actionable file-only fix; post-v41 the encoder preserves source alpha = v37/v40 alpha checks become real signals)
- bug-088 fix at TestReSTIR_GI_Temporal.cpp:691 + bug-075 binding-layout previously verified

There is no remaining file-only fix that advances the renderer without terminal access for build+run+dump+validator+vision inspection. v49 is the next structural standby tick.

## v49 audit findings (this tick)

All 5 search_files probes below returned hits:

1. **`UAVBindingLayout`** (v22 split header member): 3 file hits — FGIPass.cpp (1 reference at line 183 Shutdown clear + 2 comments at lines 263/281-283 documenting the split) + FGIPass.h (line 106 member declaration) + fresh-evidence-scan.sh (one grep entry). Confirms v22 patch intact.
2. **`case 7u:`** (v17 mode-7 sentinel): 9 grep-context hits at `GIPathTracing.hlsl:594-604` (BOTH copies, byte-identical). Confirms v17 patch intact in both HLSL copies.
3. **`DebugMode effective`** (v38 cerr value-log): 5 grep hits at `FGIPass.cpp:485-489` (the 5-line cerr statement). Confirms v38 patch intact.
4. **`check_alpha_sentinel`** (v37 alpha-check): 7 grep hits across 3 files (validate_restir_gi.py, dump_pixelstats.py, fresh-evidence-scan.sh — 2 in validate, 2 in dump_pixelstats, 1 in fresh-evidence-scan, plus 2 comment references in dump_pixelstats). Confirms v37 patch intact.
5. **`std::clamp(rgbaData[i * 4 + 3]`** (v41 alpha-encoder fix): 4 grep hits — 3 R/G/B clamps (lines 16/17/18) + 1 alpha clamp (line 27). Confirms v41 patch intact.

Additionally, full debug-switch inspection of `GIPathTracing.hlsl:575-704` confirmed byte-identical content in BOTH Private master and Data-dir copies: cases 1u-15u + default + v28 sentinel at line 692 all present at the same line numbers in both files.

## Mid-turn user message re: terminal access (carries from v48)

The cron's prompt for this tick again claims `enabled_toolsets: ["terminal", "file"]`. All `terminal` tool invocations in this tick were blocked by tirith (`pending_approval: tirith:unknown` pattern) — verified across 3 probe attempts in this tick (`date`, additional commands). Prompt-level authorization ≠ host-level execution. Effective toolset remains file-only. This tick documents the persistent terminal block honestly rather than fabricating shell-derived findings.

## Stall status

Per v42 + v43 + v44-v48 audit conclusions, there is no remaining file-only fix that advances the renderer without parent terminal access. v49 is a pure documentation/standby tick. If parent supplies terminal access before the next cron tick, v49's plan will be superseded by whichever of v17/v13a/v32/v33/v35/v36/v40/v42 best matches the evidence shape (per the v30/v32/v42/v49 PICK decision matrices).
