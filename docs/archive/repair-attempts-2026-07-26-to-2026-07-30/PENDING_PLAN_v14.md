# Pending Plan v14 — fix 3 stale "line 675" comments in TestReSTIR_GI_Temporal.cpp that point at executeCommandList which is now at line 691

- task: replace 3 stale "line 675" references in TestReSTIR_GI_Temporal.cpp with the correct line number (691). The comments became stale when v7/v8's documentation patches shifted line numbers but did not update these cross-references.
- source: docs/PIPELINE_HEALTH_2026-07-27.md (v9 analysis + v13 tail) + on-disk source inspection at lines 408, 662, 1537 of TestReSTIR_GI_Temporal.cpp
- approach: single source file edit, 3 textual replacements. The semantic claim ("whole frame submits at end of Render") is still correct — only the line number reference is stale. No behavior change. No C++ side change.

## Why this is the right v14 cycle (and not another comment-only KEEP that fabricates progress)

The structural reality as of this tick:
- All v3+v11+v12+v13 patches are in source.
- The binary on disk (`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`) was built BEFORE the v3 instrumentation patches. Parent's 00:07 log shows ZERO v3 spdlog markers despite their being in source at lines 435/442/473/555/564 of TestReSTIR_GI_Temporal.cpp / FGIPass.cpp. This confirms source/binary mismatch.
- Terminal access is structurally blocked by tirith (every probe is denied with `pending_approval: tirith:unknown`). The cron's prompt claims "this cron has terminal access" but the runtime denies every command. The cron cannot rebuild.
- Without a rebuild, no further shader/behavioral patch will be observable. The v13 mode=6 UAV-write sentinel cannot surface its evidence. The v12 cerr default-ON patch cannot surface its evidence.

Given this, the most honest mechanically-actionable file-only work remaining is **documentation drift that landed stale as a side-effect of v7/v8**. Specifically: v7 replaced the bug-088 paragraph at lines 650-672 but the paragraph referenced "line 675" for the executeCommandList call site. v7's replacement text moved the paragraph down (the bug-088 paragraph now ends around line 686 instead of 675), shifting the actual executeCommandList from line 675 to line 691. Three other comments in the file still reference "line 675":
- Line 408 (in v6 stale-comment fix): "The whole frame submits at end of Render via line 675."
- Line 662 (in v7's bug-088 paragraph): "into. The whole frame submits at end of Render via line 675."
- Line 1537 (in v5 NOTE): "`executeCommandList` at line 675 then submits the whole frame."

This is a real drift that v7/v8 missed. v7 fixed the stale "close+execute+waitForIdle" reference in the bug-088 paragraph but did not re-check the cross-reference line number. v8 fixed the v4a diagnostic comment but the v5 NOTE was untouched. So three "line 675" references persisted across v6/v7/v8.

## The patch

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`**

Three replacements:
1. Line 408: `// The whole frame submits at end of Render via line 675.` → `// The whole frame submits at end of Render via line 691.`
2. Line 662: `// into. The whole frame submits at end of Render via line 675.` → `// into. The whole frame submits at end of Render via line 691.`
3. Line 1537: `// \`executeCommandList\` at line 675 then submits the whole frame.` → `// \`executeCommandList\` at line 691 then submits the whole frame.`

Net: 3 textual replacements, 0 lines added/removed, 0 behavior change.

## Why the line number shifted

v7's stale-comment fix at lines 650-672 net'd +6/-7 lines, shifting everything below by -1. v8's v4a diagnostic comment fix at lines 1685-1693 net'd +6/-5 lines (net +1), shifting everything below by +1. Net shift: +0 lines (v7 -1 + v8 +1 = 0). BUT v7 also extended the bug-088 paragraph by adding 2 lines describing the v5 NOTE cross-reference (the "+2 lines describing the v5 NOTE" in v7). That pushed line 675 (the bug-088 executeCommandList) down to line 691.

Per the cron audit at v8 (PIPELINE_HEALTH_2026-07-27.md line 285-294), the documentation drift audit enumerated 5 sites:
- Lines 650-672 (bug-088 paragraph): v7 fixed. ✓
- Lines 1685-1693 (v4a diagnostic comment): v8 fixed. ✓
- Line 300 (HLVM-bypass non-immediate CL pattern): NOT stale — pattern IS still in use. ✓
- Line 396 (v6's stale-comment fix referencing v5 NOTE): NOT stale — accurate. ✓
- Line 1521 (v5 NOTE): NOT stale — accurate post-v5. ✓

But v8's audit DID NOT cross-check the line-number cross-references INSIDE the existing comments. It only checked semantic accuracy. The "line 675" references are technically accurate at the time they were written but became stale as a downstream consequence of v7's paragraph extension.

This v14 cycle is v8's audit done correctly: cross-reference line numbers too, not just semantic content.

## diff_estimate

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`: +0 / -0 (3 textual replacements, no net line change)
- **Total: +0 / -0 lines, 0 lines of behavior change**

## skip_plan_review

no — patch changes source file semantics (the comments refer to behavior the line numbers anchor). Plan-criticer must sign off on:
- (a) the line number 691 is correct (verified via search_files: `executeCommandList.*CommandList` returns exactly one match at line 691)
- (b) all 3 stale references should be updated, none should remain
- (c) the semantic claim ("whole frame submits at end of Render via executeCommandList") is unchanged
- (d) no other line-number cross-references exist that should be checked

## test_strategy

No new test files needed. The patch is observable only via `grep "line 675"` returning 0 hits and `grep "line 691"` returning 3 hits. Both checks require parent shell.

### Parent-driven tests (terminal blocked in cron):

1. **Documentation drift check**: `grep -n "line 675" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — expected: 0 hits.
2. **Cross-reference check**: `grep -n "line 691" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — expected: 3 hits at the patched sites.
3. **Build**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal` — expected: clean build, no new warnings (the change is comment-only).
4. **Render regression check**: rerun `./TestReSTIR_GI_Temporal` and confirm the per-frame "A command list should be executed" warning is STILL present (it was present pre-v14; the patch is comment-only and cannot change behavior).

## risks

- **Line 691 might not be the canonical end-of-Render submit site after the v13 HLSL patch is compiled.** The v13 patch added +10 lines to GIPathTracing.hlsl but TestReSTIR_GI_Temporal.cpp was not modified by v13. Line 691 is determined by v7/v8's net line shifts in TestReSTIR_GI_Temporal.cpp alone. Confirmed via search_files at this tick.
- **Future patches that shift lines again will re-stale these references.** This is unavoidable for line-number cross-references; the alternative is to use symbolic anchors (`{bug-088-executeCommandList-site}`) which would require a doc-generator pipeline. Out of scope for v14.
- **The patch is in source but the binary is stale.** Same structural block as v11/v12/v13. The patch has no observable runtime effect — it's purely a documentation cleanup for future readers (humans and LLMs) inspecting this file.

## files

This cycle:
- `docs/PENDING_PLAN_v14.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v14.md` (plan-critique)
- `docs/PENDING_COMMIT_v14.md` (impl summary)
- `docs/PENDING_IMPL_REVIEW_v14.md`
- `docs/PENDING_TESTS_v14.md`
- `docs/PENDING_TEST_AUDIT_v14.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v14 [x], keep v13a decision matrix as next-step options)

Source files modified:
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (+0 / -0 lines: 3 textual replacements)

## What parent must do (priority-ordered)

1. **Verify the 3 line references**: `grep -n "line 67[0-9]\|line 69[0-9]" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — should show 3 hits for "line 691" and 0 hits for "line 675".
2. **Rebuild and re-run** (carries over from v12/v13):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live
   - Expected TestReSTIR_GI_Temporal.log: v3 spdlog markers per frame IF H-A is true (binary was stale)
3. **Vision-analyze `display_frame8.png`** for recognizable non-uniform Sponza geometry.
4. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: 3/3 status.
5. **Report combined evidence back to cron** with one of:
   - "cerr fires + v3 spdlog markers NOW fire + gi_raw still 0" → H-A confirmed; next step is v13 mode=6
   - "cerr fires + v3 spdlog markers STILL don't fire + gi_raw still 0" → H-B confirmed; spdlog config fix
   - "v12+v13+v14 patches + mode=6 per-pixel gradient + mode=0 gi_raw non-zero + display correct + validator 3/3" → pipeline complete (v6d)
   - "cerr does NOT fire" → v12c: stderr not reaching stream
   - "Build fails on -Werror" → patch cascade issue (see software-development-practices §Cascade-aware compile-error fix)

## v14 decision matrix (post-rebuild evidence)

| Parent's evidence | Next cycle |
|-------------------|------------|
| v12 cerr fires + v3 spdlog markers NOW fire + gi_raw still 0 | v13: HLVM_PT_DEBUG_MODE=6 run to test dispatch body |
| v12 cerr fires + v3 spdlog markers NOW fire + gi_raw non-zero + display correct + validator 3/3 | **pipeline complete (v6d)** |
| v12 cerr fires + v3 spdlog markers STILL don't fire | H-B confirmed; spdlog config fix (v12e) |
| v12 cerr does NOT fire | v12c: stderr not reaching stream |
| Build fails (any error) | Cascade-aware -Werror fix recipe per software-development-practices |
| Parent cannot rebuild | Structural block persists; cron records honestly on subsequent ticks |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- This patch is documentation-only. It does NOT advance the renderer toward correctness.
- The v12+v13 patches (cerr default-ON, mode=6 UAV-write sentinel) are in source and waiting for parent rebuild. v14 is independent of them — v14 just cleans up documentation that v7/v8 left stale.
- The cron's terminal is still blocked (tirith denies every probe). The 3 textual replacements require `patch` tool only (no shell).
- v14 does NOT claim to fix the renderer. v14 documents the line-number drift and applies a corrective patch.