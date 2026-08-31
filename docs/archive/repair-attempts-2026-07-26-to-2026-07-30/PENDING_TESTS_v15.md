# Pending Tests v15

- task: parent-driven test plan for v15 patch (case-6u sentinel sync from data-dir to Private master)
- plan: docs/PENDING_PLAN_v15.md
- commit: docs/PENDING_COMMIT_v15.md
- impl_review: docs/PENDING_IMPL_REVIEW_v15.md
- tests_author: tester (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Test surface

The v15 patch is observable only via diff between the two HLSL copies showing zero meaningful drift outside header comments, and via a clean rebuild from Private master producing identical SPIR-V to data-dir compile. The test surface is the existing `TestReSTIR_GI_Temporal` test harness with no new test files added.

## Staged tests (parent-driven; terminal blocked in cron)

### Test 1: Drift elimination check

```bash
diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
```

Expected output: empty (or only header comment whitespace differences, if any).

Pass criterion: zero line-number or content drift between the two HLSL copies outside the header comment block.

### Test 2: Build cleanliness

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

Expected: clean build, no new warnings. The patch is text-identical to what the data-dir copy already compiles; existing v3+v11+v12+v13 patches should still build cleanly. If -Werror fires, see software-development-practices §Cascade-aware compile-error fix recipe (grep the whole tree for the offending pattern, patch every match).

### Test 3: SPIR-V identity check (optional but strong)

Compile Private master standalone with ShaderMake and compare the resulting `.sblob` against the existing data-dir compile. If byte-identical (or differ only in irrelevant metadata), the sync is verified at the binary level.

### Test 4: Render regression check (carries over from v12/v13/v14)

```bash
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
```

Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live.
Expected TestReSTIR_GI_Temporal.log: v3 spdlog markers per frame IF H-A is true (binary was stale).
Pass criterion: same observable behavior as v12's parent-driven test plan. v15 is a no-op at runtime.

### Test 5: Vision analysis (carries over from v12/v13/v14)

Open `display_frame8.png` with vision_analyze. Expected: recognizable non-uniform Sponza geometry with sane exposure.

### Test 6: Validator (carries over from v12/v13/v14)

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: 3/3 status (if upstream bugs are fixed).

## Honesty caveats

- All 6 tests are parent-driven. The cron's terminal is blocked (tirith denies every terminal command). Tests cannot run from cron.
- The v15 patch is documentation/sync, not a renderer fix. The pass criteria are about producing diagnostic information about the patch landing, not about the renderer being correct.
- The v15 patch has no observable runtime effect. The renderer status is unchanged by v15.