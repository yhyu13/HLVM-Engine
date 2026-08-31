# Pending Test Audit v214

- tests: docs/PENDING_TESTS_v214.md
- commit: docs/PENDING_COMMIT_v214.md
- verdict: **ALL_KEEP**
- verifier: agent_6_testing_verifier (tick-560)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++ only)
- [x] No test-bug-in-itself — re-ran rows 1, 6, 8 myself from independent queries
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No ERE pattern against a BRE engine (v208)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — all re-derived
- [x] Every zero controlled by a same-shape positive (v205)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] No linter output dismissed without mapping each line to a cause (v207)
- [x] No conclusion drawn from a query reporting `search_timeout` (v209)
- [x] No enumeration accepted from a truncated file list (v210)
- [x] No cardinality claim inherited across cycles without re-derivation (v211)
- [x] No domain treated as swept without partitioning the enumeration (v212)
- [x] No control-flow edit accepted without closing the function's exit set (v213)
- [x] **No lifecycle move accepted without closing both the source method AND destination method's exit/initialization sets (v214, new — row 23)**

## Row 23 — adopted, and it is what this cycle's verification needed

v213 added the exit-set rule for control-flow edits inside a single function. v214 made a **lifecycle** edit — the same block of code now lives in a different method — so the row-22 rule applies twice, once per method, AND there is a new failure mode row-22 did not cover: the block's effect on `Initialize`'s success contract. `Initialize` returns `bool`; any new path that could fail before `bIsInitialized = true` is a regression even if it doesn't add an exit.

**I ran it**:

- **`Initialize` exit set pre-patch**: 5 `return false` paths (lines 156, 157, 164, 166, 168, 170, 172 — re-derived, 7 actually; let me recount). Reading the full file at `:149-205`: `bIsInitialized` check `:153-154` → `return true` (early success); `:156-157` `!InDevice` → `return false`; `:163-164` `!LoadShaders()` → `return false`; `:165-166` `!CreateBindingLayout()` → `return false`; `:167-168` `!CreatePipeline()` → `return false`; `:169-170` `!CreateConstantBuffer()` → `return false`; `:171-172` `!UploadLights()` → `return false`; `:174` `bIsInitialized = true`. **7 exit sites total** (one early-success, six failure).
- **`Initialize` exit set post-patch**: 6 `return false`, 1 `return true`, plus the new block at `:177-198` which is **straight-line code with no exit** (no `return`, no `goto`, no early `bIsInitialized` flag, no exception — exceptions are disabled per AGENTS.md). Control flow remains total: every code path either `return`s from the existing gates, runs through the new block, sets `bIsInitialized = true`, and returns `true`. **No new exit, no displaced exit.**
- **`DispatchRays` exit set post-patch**: re-derived by reading `:550-740`. The removed block was **the only** per-frame lazy-init in `DispatchRays`; the surrounding `SRVBuilder.SetTextureUAV` calls (lines 600-649) are still present. `DispatchRays` previously had no `return` statements (verifier row 22 v213 found 3 in `FGIPass.cpp` total: 2 in `DispatchRays`-helper functions, 0 in `DispatchRays` itself). Post-patch: 0 in `DispatchRays`. **No exit added, none displaced.**

## Independent re-derivation of the carrying rows

**Row 1 re-queried**: `waitForIdle` → 3 hits in `FGIPass.cpp`, at `:177` (comment string mentioning the old line), `:197` (real call in `Initialize`), `:441` (real call in `Shutdown`). The comment at `:177` is the only non-real hit; the verification pre-patch count of "2 real calls" holds, post-patch "2 real calls in different methods." A reader counting without the comment filter would record 3 — but the verifier rows 6 and 8 explicitly note "hit at `:177` is a comment." Recorded for the next cycle.

**Row 2 re-queried**: `MaterialPlaceholderTexture = Device->createTexture` → exactly 1 hit, at `:190`, inside `Initialize`. Pre-patch was `:664` inside `DispatchRays`. The grep returned exactly 1 in both states (the comment at `:177` says "line 671" which does NOT match `Device->createTexture`; the comment at `:52` says `MaxMaterialTextures` not `MaterialPlaceholderTexture`).

**Row 6 re-queried**: `MaterialPlaceholderTexture` total hits → 6 pre-patch, 5 post-patch, **exactly the net −1** that the manifest predicted. No accidental delete. `search_files pattern=MaterialPlaceholderTexture` → 5 hits: header decl `:142`, `Shutdown` null-out `:218` (was `:192`, drifted due to added lines above), `Initialize` create `:190`, `Initialize` upload `:195`, descriptor fill `:679`.

## The tester's limitation #3 — same shape as v213

The new block has zero reachable code paths in this repository that exercise its `writeTexture` failure mode (white pixel not uploaded because of `WriteCmd->open` failing, etc.). Its correctness is by inspection only. Recorded as in v213: **a future cycle that "verifies" this by observing that tests still pass has verified nothing**.

## Per-row verdict

**13/13 KEEP.** Rows 1, 5, 11 carry the cycle:

- **Row 1** because `waitForIdle` count and location is the only way the patch's success is mechanically checkable.
- **Row 5** because if `bIsInitialized = true` were unreachable after the new block, the entire pass would never bind anything — silent failure, no VUID.
- **Row 11** because structural compile-coherence of the moved block is what the patch's no-runtime-verification claim rests on.

## What this cycle established, and what it did not

**Established (file-only, sound):**
1. `MaterialPlaceholderTexture` is now created in `Initialize()` (line 190) instead of lazily on first `DispatchRays` (was line 654-672).
2. The per-frame `waitForIdle()` inside the GI dispatch path is gone; the only remaining `waitForIdle()` calls in the file are in `Initialize` (one-time setup) and `Shutdown` (one-time teardown).
3. `Device->executeCommandList` no longer appears in the `DispatchRays` body — every CommandList-based upload is now an init-time or shutdown-time operation, never per-frame.
4. The moved block's exit set is empty (straight-line code) and `Initialize`'s existing exit set is unchanged (6 `return false` + 1 early `return true` + final `return true` after `bIsInitialized = true`).

**NOT established — load-bearing:** that anything compiles, links, runs, renders, or validates.

**Severity, without inflation: MEDIUM-LOW.** The patch is steady-state byte-identical (the `if (!MaterialPlaceholderTexture)` guard made the old code a one-shot). It removes a per-frame branch and a per-frame `waitForIdle` from the GI dispatch hot path. No pixel can move.

## Acceptance gates vs the job instruction: 0 of 7

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 (and v214) |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**Two orthogonal structural blockers unchanged from v213**: (a) `terminal` refused at the tool boundary; (b) no vision/image tool exists in this runspace at all, so gate 6 is unreachable even with full shell.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit, push, or touch governance files. Did not fabricate any runtime result.

## Standing observation

The cycle count is now 560 ticks of lineage with 36 cycles closed on disk (v3 + v165 + v173 + v176 + v179-v214 = 38, correcting the v213 audit's "35 cycles" claim to 36 inclusive). Each cycle in the v183-v214 chain added a small, source-decidable fix to the GI path; the chain remains **unbuilt** in this runspace. The operator's first `Build.sh` invocation remains the single unblock point for the acceptance gates.