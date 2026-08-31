# Pending Plan v6 — contingency for v5 verification outcome

- task: parent-driven v5 verification branch — this plan documents the four contingency fixes based on parent's v5 log evidence
- source: no bundle
- approach: **NOT executed yet**. v6 is staged but NOT triggered until parent's v5 verification produces one of the four documented log-evidence shapes. This is a contingency plan, not an active cycle.

## Why v6 is staged before v5 verification

The cron cannot run terminal commands (tirith blocks every command despite the prompt's claim of "this cron has terminal access"). The cron is structurally file-only. v5's verification (build, run, validator, vision) requires terminal. Until the parent verifies v5, the pipeline state is: v5 patch landed on disk, renderer status unknown.

The prompt says: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop." v6 documents the four mechanically actionable fixes that the parent could trigger based on v5's log evidence. This is honest planning, not silent stop.

## v6 sub-plans (triggered by parent's v5 log shape)

### v6a — if v5 log shows gi_raw STILL 0,0,0

**Diagnosis**: HLVM-bypass removal didn't fix the bug. Bug is downstream of the bypass removal.

**Mechanically actionable hypothesis tree** (ranked by evidence strength):
1. **Output texture recreation bug** — if v3's Pre-GIPass OutputTex handle ≠ v4a's dump gi_raw handle, then the test is dumping the wrong texture. Fix: change DumpCurrentFrame to capture the OutputTexture from GIPass.OutputTexture getter (line 567 in FGIPass.cpp shows it's stored) and use that for the dump.
2. **nvrhi auto-barrier ordering bug** — if v3's ENTER and EXIT both fire, and OutputTex matches, then the GI pass wrote but the dump's `copyTexture` reads stale storage. Fix: insert explicit `CommandList->setTextureState(OutputTexture, CopySource)` and `commitBarriers()` before the dump's `copyTexture` call in DumpCurrentFrame.
3. **Slangc RT payload dead-strip** — if RT instance IDs are correct but radiance is zero, the slangc dead-strip of ClosestHit payload fields may be at play. Fix: switch GIPathTracing.hlsl to use `payload<>` with full field reads in RayGen (per gpu-rendering-bisect-debug §3 sentinel pattern).

### v6b — if v5 log shows gi_raw non-zero but validator < 3/3

**Diagnosis**: GI dispatch is working, but accumulate/ReBLUR/display chain has its own bug.

**Mechanically actionable fix**: identify which validator check fails. Each check targets a different texture:
- mean check < 5 → display texture uniform black/dim
- spatial std < 30 → spatial texture uniform
- cell-variance std < 8 → denoised texture uniform

The validator prints which check failed. That identifies the broken texture, which identifies the broken pass (GIAccumulate, ReBLUR, or tonemap).

### v6c — if v5 validator = 3/3 but display still bad (vision)

**Diagnosis**: passes write to textures correctly, but final display is broken.

**Mechanically actionable fix**: inspect GIAccumulate pass output transition (same auto-barrier pattern from bug-075 may apply to the display blit). The blit at end-of-Render copies from the accumulator's output to the swap chain.

### v6d — if v5 fixes everything

**Pipeline complete.** v5 marked [x] in PENDING_PICK. No v6 work needed. Cron exits [SILENT] on next tick unless parent adds new tasks.

## Risk profile per sub-plan

- v6a-1 (output texture recreation): low risk. Pure test-side change.
- v6a-2 (barrier insertion): medium risk. nvrhi's auto-barriers might fight the manual transition.
- v6a-3 (shader payload): HIGH risk. Touches GIPathTracing.hlsl + slangc recompile. May regress working sibling tests. Apply last and only with strong evidence.
- v6b: medium risk. Affects GIAccumulate/ReBLUR/tonemap passes.
- v6c: medium risk. Affects end-of-frame blit.

## Test strategy for v6

Per-cycle: parent-driven build + run + log capture + validator run + vision check. Same shape as v5.

## Honest caveats

- v6 is staged but NOT active. The cron cannot decide which sub-plan to execute without terminal.
- The cron is file-only. Any "mechanically actionable" fix in v6 still requires parent to build and run.
- If parent can grant terminal access to the cron (the prompt claims it should be granted; reality is tirith blocks), v6's sub-plans can be executed by the cron instead of documented for the parent.
- Until parent verification, the renderer status is unknown. v5 patches are in source. v6 patches are not.
- diff_estimate per sub-plan: v6a-1 = +5 lines, v6a-2 = +3 lines, v6a-3 = shader rewrite (10+ lines), v6b = depends on check, v6c = +3 lines.
- skip_plan_review: no — v6 sub-plans involve runtime state changes and need plan-criticer review before execution.
- produces_test_files: no — validator unchanged.

## Files this cycle will touch (per sub-plan, when triggered)

- v6a-1: TestReSTIR_GI_Temporal.cpp (DumpCurrentFrame) + FGIPass.h (getter for OutputTexture).
- v6a-2: TestReSTIR_GI_Temporal.cpp (DumpCurrentFrame).
- v6a-3: GIPathTracing.hlsl (shader rewrite).
- v6b: TBD based on validator output.
- v6c: TestReSTIR_GI_Temporal.cpp (end-of-Render blit).

## What the parent must do (decision matrix)

1. Run v5: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` then `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`.
2. Capture `TestReSTIR_GI_Temporal.log`.
3. Run validator: `cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data && python3 validate_restir_gi.py`.
4. Vision-analyze `display_frame8.png`.
5. Report one of:
   - "v5 fixed everything" → pipeline complete; v6 not needed.
   - "gi_raw still 0,0,0" → trigger v6a (which sub-plan depends on log evidence of v3+v4a logs).
   - "validator < 3/3 but gi_raw non-zero" → trigger v6b.
   - "validator = 3/3 but display still bad" → trigger v6c.
6. Paste the relevant log lines + validator result back to the cron.

The cron then executes the matching sub-plan through plan-criticer/impler/reviewer/tests/audit cycles (6-role pipeline still applies).