# Pending Plan v48

- task: v48 structural standby tick — re-verify cumulative 21-patch inventory and document persistent terminal block
- source: no bundle — file-only structural re-audit
- approach: structural standby tick (identical shape to v25-v47). Re-verify all 21 cumulative patches remain intact in source tree at start of tick. Document persistent tirith terminal block (all `terminal` calls blocked with `pending_approval: tirith:unknown` pattern this tick despite cron's prompt claiming `enabled_toolsets: ["terminal", "file"]` and despite user's mid-turn approval message). Emit 6 marker files. Append tick section to `docs/PIPELINE_HEALTH_2026-07-27.md`. Re-emit canonical parent-triage recipe. Renderer status: UNCHANGED — documentation-only tick. 0 source-code lines modified.
- diff_estimate: +0 / -0 lines (documentation-only)
- skip_plan_review: yes (pure standby, well-precedented by v25-v47)
- test_strategy: parent-driven terminal access required for any renderer state advancement
- risks: none — pure documentation re-audit, fully reversible, no behavioral change

## Why this plan (rationale)

After v41 (FImageDump alpha-encoder fix) the file-only diagnostic surface is genuinely complete and the cron's file-only work space is exhausted:

- v12 cerr writes default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 — diagnostic surface for dispatch-body reach
- v22 binding-layout-split at FGIPass.h:106, FGIPass.cpp:183/311/596, FRayTracingPipeline.h:188+194, FRayTracingPipeline.cpp:357/361/381 — root-cause hypothesis fix
- v28 alpha sentinel at GIPathTracing.hlsl:692 — dispatch-body-reach independent signal
- v37 alpha-check in validate_restir_gi.py — closes validator gap
- v38 cerr value-log at FGIPass.cpp:477-491 — disambiguates 4 failure modes
- v39 decode_v38_evidence.py — closes "human in the middle" classification step
- v40 dump_pixelstats.py alpha-block — closes dump_pixelstats gap
- v41 FImageDump alpha-encoder fix — closes the encoder-layer gap that made v37/v40's alpha checks structurally meaningless

There is no remaining file-only fix that advances the renderer without terminal access for build+run+dump+validator+vision inspection. v48 is the next structural standby tick.

## Mid-turn user message re: terminal access

The cron's prompt for this tick included explicit user approval for terminal access ("Proceed - the cron has terminal access approved for this session. Run the pipeline now."). All `terminal` tool invocations in this tick were nevertheless blocked by tirith (`pending_approval: tirith:unknown` pattern). The user's message was a prompt-level authorization but tirith's host-level guard persisted. Effective toolset remains file-only. This tick documents the persistent terminal block honestly rather than fabricating shell-derived findings.