# Pending Tests v14

- task: parent-driven test plan for v14 patch (3-line-number documentation drift fix)
- plan: docs/PENDING_PLAN_v14.md
- commit: docs/PENDING_COMMIT_v14.md
- impl_review: docs/PENDING_IMPL_REVIEW_v14.md
- tests_author: tester (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Test surface

The v14 patch is observable only via shell commands:
1. `grep` on the patched file to confirm 3 "line 675" → "line 691" replacements landed
2. Build cleanliness check (the patch is comment-only; no compile errors expected)
3. Render regression check (patch is comment-only; no behavior change expected)

The test surface is the existing `TestReSTIR_GI_Temporal` test harness with no new test files added.

## Staged tests (parent-driven; terminal blocked in cron)

### Test 1: Documentation drift grep

```bash
grep -n "line 675\|line 691" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
```

Expected output (3 lines):
```
408:        // The whole frame submits at end of Render via line 691.
662:        // into. The whole frame submits at end of Render via line 691.
1537:        // `executeCommandList` at line 691 then submits the whole frame.
```

Pass criterion: 0 "line 675" matches, 3 "line 691" matches at the expected line numbers.

### Test 2: Build cleanliness

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

Expected: clean build, no new warnings. The patch is comment-only; existing v3+v11+v12+v13 patches should still build cleanly. If -Werror fires, see software-development-practices §Cascade-aware compile-error fix recipe (grep the whole tree for the offending pattern, patch every match).

### Test 3: Render regression (carries over from v12/v13)

```bash
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
```

Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live.

Expected TestReSTIR_GI_Temporal.log: v3 spdlog markers per frame IF H-A is true (binary was stale). The v14 patch should have zero observable effect on the renderer (it is comment-only).

Pass criterion: same observable behavior as v12's parent-driven test plan. v14 is a no-op at runtime.

### Test 4: Vision analysis (carries over from v12/v13)

Open `display_frame8.png` with vision_analyze. Expected: recognizable non-uniform Sponza geometry with sane exposure (the acceptance criterion from the user's prompt).

### Test 5: Validator (carries over from v12/v13)

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: 3/3 status (if upstream bugs are fixed).

## Honesty caveats

- All 5 tests are parent-driven. The cron's terminal is blocked (tirith denies every terminal command). Tests cannot run from cron.
- The v14 patch is documentation drift fix, not a renderer fix. The pass criteria are about producing diagnostic information about the patch landing, not about the renderer being correct.
- The v14 patch has no observable runtime effect. The renderer status is unchanged by v14.