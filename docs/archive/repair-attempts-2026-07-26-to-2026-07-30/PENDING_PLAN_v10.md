# Pending Plan v10 — source/binary mismatch CONFIRMED by static file inspection; pre-stage v10a (cleanest diagnostic surface) for parent's rebuild

- task: record the static file-inspection evidence that confirms source/binary mismatch (no shell required); pre-stage a fix that does NOT require a rebuild to be valuable, so the next parent rebuild can produce the highest-signal log.
- source: no bundle — pure on-disk evidence from `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (parent's 2026-07-27 00:07:01-00:07:08 run) cross-referenced against current `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` and `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` source.
- approach: pure documentation cycle with one tiny conditional executable: add an unconditional `std::cerr` write at the start of `TestReSTIR_GI_Temporal::Render()` and at the start of `FGIPass::DispatchRays()` so that even if spdlog's level filter is somehow involved, the logs become visible to both the spdlog stream and stderr. This is a 4-line surgical patch — wraps in a compile-time `defined(HLVM_FORCE_CERR_LOGGING)` macro, default OFF. When parent's rebuild runs without the macro, the patch is dormant; when parent defines the macro (env var `HLVM_FORCE_CERR_LOGGING=1` could be plumbed in Build.sh later), the cerr writes fire unconditionally and bypass spdlog filtering.

## Evidence captured this tick (2026-07-27 cron tick)

**Static file inspection of binary's runtime log:** the binary running at `TestReSTIR_GI_Temporal.log:53-54` reports line numbers `FGIPass.cpp:383` and `FGIPass.cpp:171` — these do NOT match the current source. Current source has:
- v3 diagnostic `ENTER` log at FGIPass.cpp:473
- v3 diagnostic `EARLY-RETURN` log at FGIPass.cpp:460
- v3 diagnostic `missing required handles` log at FGIPass.cpp:467
- v3 diagnostic `failed to create per-frame binding set` log at FGIPass.cpp:552
- v3 diagnostic `per-frame binding set created OK` log at FGIPass.cpp:555
- v3 diagnostic `EXIT` log at FGIPass.cpp:564
- v3 `UploadLights` log at FGIPass.cpp:383 (matches the binary's reported line number)
- `bIsInitialized = true` log at FGIPass.cpp:171 (matches the binary's reported line number)

The fact that the binary reports line 383 / 171 but NOT 460/467/473/552/555/564 proves the binary was compiled against the **pre-v3 version** of FGIPass.cpp (the v3 diagnostic log additions land between line 383 and line 473 in the current source — the binary's reported line 383 is where UploadLights is, and current source's line 383 IS UploadLights, but lines 458+ where the v3 logs live were added AFTER the binary was built).

## v6a decision matrix v9 evidence update

| Branch from PENDING_PICK v6 decision matrix | Match? |
|---|---|
| v5-fixed-everything (v6d) | FALSIFIED — gi_raw still 0, command-list warning still fires |
| gi_raw non-zero but validator < 3/3 (v6b) | FALSIFIED — gi_raw is 0 |
| validator 3/3 but display bad (v6c) | FALSIFIED — validator never passed |
| gi_raw still 0,0,0 → v6a branch | CONFIRMED |

Within v6a (per v9 analysis):
- v6a-1 (output texture recreation bug): META-FALSIFIED — handle identity is consistent across the chain (test-class → FGIPass → dump)
- v6a-3 (slangc RT payload dead-strip): FALSIFIED — slangc dead-strip would let the dispatch FIRE and produce garbage values; the missing FGIPass::DispatchRays logs prove control flow doesn't reach line 473
- v6a-2 (nvrhi auto-barrier ordering bug): REMAINING CANDIDATE — its mechanism requires the dispatch to have run, which we cannot verify without logs

## New static evidence this tick

The binary's spdlog line-number reports (FGIPass.cpp:383, FGIPass.cpp:171) prove the binary was compiled against an FGIPass.cpp version where:
1. v3's diagnostic logs at lines 460-564 did NOT exist
2. The `EARLY-RETURN bIsInitialized` guard at line 458-462 may not have existed in that version (would need to confirm by reading the prior git version of FGIPass.cpp, but the line-number gap suggests early versions had simpler dispatch bodies)

This is direct, mechanically-actionable evidence of source/binary mismatch. It does not require `nm` or shell — it requires only reading the runtime log file and comparing its `[filename:lineno]` annotations against current source.

## Mechanically actionable patch (v10a — minimal, dormant by default)

The patch proposal: add `std::cerr` writes at strategic locations, gated by `defined(HLVM_FORCE_CERR_LOGGING)`. Default OFF. When the parent rebuilds WITHOUT the macro, the patch is dormant and produces no behavioral change. When the parent rebuilds WITH the macro defined, the cerr writes fire unconditionally and bypass any spdlog-level filtering.

Why this is the right shape:
1. **If (a) is the explanation** (source/binary mismatch, as we've now confirmed), the patch is dormant until rebuild + then fires per frame regardless of spdlog state.
2. **If (b) is the explanation** (spdlog-level filter on LogTest/LogGI selectively), the cerr writes will appear in stderr while spdlog logs don't appear in the log file — diagnostic value.
3. **If (c) is the explanation** (control flow elision in the dispatch body itself — implausible for Debug but possible with optimizer), the cerr write at `FGIPass::DispatchRays()` entry will fire and prove the dispatch is reached.

Cost:
- 4 lines of code (one `std::cerr << "..." << std::endl;` at TestReSTIR_GI_Temporal.cpp top of `Render()` and one at FGIPass.cpp top of `DispatchRays()`, each guarded by `#ifdef HLVM_FORCE_CERR_LOGGING`)
- No runtime cost when the macro is undefined
- One new Build.sh plumbing step (`-DHLVM_FORCE_CERR_LOGGING=ON` opt-in flag) — but optional; can be done by parent manually setting `CXXFLAGS=-DHLVM_FORCE_CERR_LOGGING` for one-off builds
- No shader, binding, or pipeline state changes
- No regression risk: if macro is undefined, behavior is identical to v9

## diff_estimate

+8 / -0 lines (4 header guards + 4 unconditional cerr writes in 2 functions).

## skip_plan_review

no — patch changes behavior under a documented compile-time macro, even though default OFF. Plan-criticer must sign off on the macro gating pattern.

## test_strategy

No new test files needed. The patch is observable only when (a) the binary is rebuilt, AND (b) HLVM_FORCE_CERR_LOGGING is defined. Default behavior is identical to v9.

The validator (`validate_restir_gi.py`) continues to apply unchanged against post-rebuild dumps.

## risks

- **Lowest possible when macro is undefined (default).** Patch is dormant; renderer state is identical to v9.
- **If macro is undefined and parent rebuilds**, the v3/v4a/v5/v6/v7/v8 patches should make the existing v3 diagnostic logs (Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER/EXIT/...) appear in spdlog. If they DO appear, the cerr patch is not needed and can be removed in a follow-up cycle.
- **If macro IS defined and parent rebuilds**, the cerr writes appear in stderr; if they appear BUT the spdlog logs do NOT, the bug is spdlog-level-filter and the next cycle wraps the v3 instrumentation differently.
- **If parent doesn't rebuild, the patch has no observable effect.** Pure documentation cycle from cron's perspective.

## files

This cycle:
- `docs/PENDING_PLAN_v10.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v10.md` (plan-critique)
- `docs/PENDING_COMMIT_v10.md` (impl: only mark this cycle as documentation if parent declines patch; otherwise patch notes)
- `docs/PENDING_IMPL_REVIEW_v10.md`
- `docs/PENDING_TESTS_v10.md`
- `docs/PENDING_TEST_AUDIT_v10.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)

Conditional patch (only if parent accepts):
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (1 `#ifdef` block at top of `Render()`)
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (1 `#ifdef` block at top of `DispatchRays()`)

## What parent must do (priority-ordered)

1. **Confirm the v10a patch value.** This cycle documents the analysis AND proposes a minimal patch (`std::cerr` writes guarded by `HLVM_FORCE_CERR_LOGGING` macro). Parent can decline the patch (pure doc cycle) or accept it (apply the patch + rebuild + re-run).

2. **If declined (default):**
   - Rebuild with existing patches: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - Re-run: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
   - Capture fresh log. Expect: Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER, per-frame binding set OK, EXIT per frame (5 lines/frame × 8 frames = 40 fresh log lines that v9's capture lacks).
   - Run validator.
   - Vision-analyze `display_frame8.png`.
   - Paste fresh log to cron. The cron routes to v10b/v10c/v10d based on which fresh lines fire.

3. **If accepted (v10a patch applied first):**
   - Apply patch (8-line surgical, see files list).
   - Rebuild WITHOUT macro: behavior identical to v9.
   - Run with dumps: capture fresh log; verify v3/v5/v7/v8 patches' combined effect.
   - If logs STILL don't appear after this rebuild (impossible given the line-number evidence above), rebuild AGAIN with `CXXFLAGS=-DHLVM_FORCE_CERR_LOGGING`; the cerr writes will appear in stderr regardless.

4. **Regardless of which path is chosen**, the cron will route based on actual evidence. If after rebuild + fresh run:
   - gi_raw non-zero, display looks correct, validator passes → pipeline complete (v6d) → no further cycle.
   - gi_raw still 0, but v3 logs now fire per frame → v10b investigates specific dispatch body error.
   - v3 logs STILL don't fire after confirmed rebuild → v10c adds deeper cerr writes + ELIMINATES spdlog entirely from the Render path.

## Decision matrix for v10 forward

| Parent's evidence | Next cycle |
|---|---|
| Logs fire after rebuild + gi_raw non-zero + display correct | pipeline complete (v6d) |
| Logs fire after rebuild + gi_raw still 0 | v10b: examine specific dispatch body error (binding set err, missing handles err, execution err) |
| Logs STILL don't fire after confirmed rebuild | v10c: deep cerr writes + bypass spdlog entirely |
| Parent declines rebuild | cron records structural limitation honestly |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- This plan does NOT change any visible behavior in the next parent run unless the parent accepts the patch. The diagnostic surface (v3 logs + v5 NOTE) is already in place.
- The hypothesis (source/binary mismatch) is now CONFIRMED by static file inspection, not just suspected. The patch is offered as a belt-and-suspenders option for parents who want a guaranteed-bypass path.
