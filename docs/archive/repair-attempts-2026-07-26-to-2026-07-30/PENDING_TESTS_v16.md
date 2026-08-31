# Pending Tests v16

- task: parent-driven test plan for v16 cycle (corrected understanding of which GIPathTracing.hlsl copy slangc compiles)
- plan: docs/PENDING_PLAN_v16.md
- commit: docs/PENDING_COMMIT_v16.md
- impl_review: docs/PENDING_IMPL_REVIEW_v16.md
- tests_author: tester (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Test surface

v16 is a doc-only cycle. The only verifiable test is the corrected understanding itself, which is checkable via static file inspection (no shell required for the inspection step, but the parent must run the actual build to verify slangc behavior).

## Staged tests (parent-driven; terminal blocked in cron)

### Test 1: Static source-file inspection (file-only, can be done by parent with read-only tools)

```bash
grep -n "GIPathTracing.hlsl" Engine/Source/Runtime/ShaderMakeBuild.py
grep -n "GIPathTracing.hlsl" Engine/Source/Runtime/CMakeLists.txt
grep -n "GIPathTracing.hlsl" Engine/Source/Runtime/Build/Debug/build.ninja
```

Expected output: all three lines point at `Private/Renderer/Shader/GI/GIPathTracing.hlsl`. Confirms the corrected understanding that slangc compiles the Private master, not the data-dir copy.

### Test 2: Build log inspection (parent-driven, terminal required)

```bash
grep "GIPathTracing.hlsl" Engine/Source/Runtime/build_Debug.log
```

Expected output: a line like `(100.0%) HLVM_ReSTIR_GI_Temporal SPIRV GIPathTracing.hlsl {main} {}` — slangc invocation on the Private master.

### Test 3: Build log freshness check

The build_Debug.log must be from the most recent rebuild (post v15 sync). If the log is from before v15, the parent needs to rebuild before this test gives meaningful evidence.

### Test 4: Carry-over from v12/v13/v15 (parent-driven)

Same tests as v15 (default-mode run, mode-6 run, vision analysis, validator). v16 doesn't change the test surface for the renderer — only the interpretation of evidence.

## Honesty caveats

- All tests are parent-driven. The cron's terminal is blocked (tirith denies every terminal command). Tests cannot run from cron.
- The corrected understanding is a retrospective finding. Forward-looking, the pipeline should always check ShaderMakeBuild.py before assuming a data-dir HLSL patch lands.
- v16 is documentation-only. The renderer status is unchanged.