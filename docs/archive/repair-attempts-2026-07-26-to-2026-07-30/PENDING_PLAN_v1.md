# Pending Plan v1
- task: restir-gi-fix
- source: https://github.com/yhyu13/HLVM-Engine (workdir: /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine)
- approach: Make the ReSTIR GI Temporal test produce a visually non-uniform display with recognizable Sponza geometry at sane exposure. Prior session (2026-07-25) landed two fixes (worldpos dump normalization, WriteGBufferSentinels removal) but the fresh 2026-07-27 00:07 log shows the *gi_raw* radiance buffer is uniform zero across the frame and the NVRHI command-list-reopen warnings at frame 1-3 are still present. This v1 plan orders three mechanically actionable fixes the next tick can land in one round, then re-runs the test.
- diff_estimate: +30 / -20 lines (DiagnosticDumpInstrument + DispatcherSentinelGate + LightMargin const) — capped per cron-tick `<50` non-test diff rule.
- skip_plan_review: no  (cross-stage GPU bug; plan-criticer value is high: confirm we bisect before patching)
- test_strategy: tester (role #5) re-runs TestReSTIR_GI_Temporal with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8`, verifies fresh display PNG is non-uniform and visually contains Sponza geometry, validator (validate_restir_gi.py) PASS on the fresh dump group only.
- risks:
  1. **bisect-before-fix** (gpu-rendering-bisect-debug §2): the bug may be in one of three independent layers (RayGen payload read, SRV barrier ordering, constant buffer ambient scale). Fixing all three blind will mask which one mattered. The first sub-step must add a debug-mode selector (mode 1: output `gRTInstanceInfo[0].AlbedoColor` directly to gi_raw; mode 2: output world-space position; mode 3: return whatever the closest-hit payload returned) — pick ONE additive hypothesis-test and ship it.
  2. **workaround-mask anti-pattern** (skill §Don't #2): do not bump `AmbientScale` before confirming the GI pipeline itself contributes. The 2026-07-25 commit `aa2cc53` already raised ambient to `1.5` for the post-sentinel-fix material; bumping again without evidence will bury the real bug.
  3. **-Werror cascade** (skill §Verification hygiene): if any of the three sub-fixes triggers `-Werror,-Wold-style-cast` or `spdlog::level::warning`-vs-`warn`, grep the whole tree first, fix every match in one pass, then verify with `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`.
  4. **NVRHI command-list reopen** (anti-pattern #5): symptom is `warning: LogRHI:[DeviceManager.cpp:52] A command list should be executed before it is reopened` every frame. The frame 1-3 silence is acceptable; per-frame noise indicates two immediate command lists are being opened without an execute barrier between them in the dump path. Track as a separate card after gi_raw is non-zero, NOT in this cycle.
  5. **single-profile pipeline caveat** (skill §Anti-patterns #7): this dispatcher and all 5 sub-roles run on the same model — KEEP verdicts are self-checks. Plan-criticer and reviewer weight accordingly.

## Sub-steps for the impler role (role #3)

Sub-step A — **single instrumentation probe, NO payload-field changes**:
- Add `HLVM_PT_DEBUG_MODE = 1` temporary mode to FGIPass that writes `gRTInstanceInfo[0].AlbedoColor` (a non-zero known value) directly into `gi_raw.rgb` before any RT work. If `gi_raw` becomes uniform `(0.85, 0.85, 0.85)`, the SRV→UAV copy works and the RT pipeline is the broken stage. If `gi_raw` stays zero, the copy/UAV binding is the broken stage. (One variable.)
- Do NOT add a new payload field. Do NOT change `gbuffer_worldpos` dump normalization (already done). Do NOT touch `WriteGBufferSentinels` (already removed).

Sub-step B (only after A returns a signal and the reviewer routes back with plan-fidelity KEEP on A) — **add `payload.sentinelInOut` round-trip** if A says RT-pipeline-stage is guilty:
- Add a `float4` to the RT payload, write `gRTInstanceInfo[0].AlbedoColor` at top of ClosestHit, read it at end of RayGen BEFORE the TraceRay consumption, and write it to gi_raw. Apply slangc padding rule (skill critical-gotcha: every entry-point reads the new field on both sides, no dead-strip).
- If gi_raw shows the constant value, the SRV is innocent; the bug is downstream. If gi_raw is garbage, slangc dead-stripped the field — fix per anti-pattern.

Sub-step C — **only after gi_raw is provably non-zero AND consistent with mode 1 sentinel**:
- The actual fix. Until sub-step A signals, we don't know whether the fix is in (i) the RT payload layout, (ii) the SRV barrier ordering in nvrhi PingPong (see skill `nvrhi-deferred-barrier-ordering.md`), or (iii) the constant buffer ambient term. Do NOT pre-pick this in v1.

## What this plan deliberately does NOT include

- Do not bump ambient/lighting constants (anti-pattern #2, commit `aa2cc53` history).
- Do not touch the NVRHI command-list-reopen warning (separate card; not the visual regression).
- Do not change the worldpos dump normalization (already correct post-2fab7d6).
- Do not re-add `WriteGBufferSentinels` (already removed in e6b3d52).
- Do not delete or rewrite prior commit history (user instruction).

## Verify command (parent runs after this plan lands)

```
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal 2>&1 | tail -200
# Inspect dumps/2026*/gi_raw_frame8.png with vision analysis
# Run Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py against newest dump group only
```

Acceptance signal for v1: `DumpRGBA32FTexture: gi_raw normalized per-channel` no longer reports R[0,0] G[0,0] B[0,0] — instead reports a non-zero range with per-channel variance > 0.001.
