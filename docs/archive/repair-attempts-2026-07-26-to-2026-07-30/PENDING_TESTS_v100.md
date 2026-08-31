# Pending Tests v100

- tests: docs/PENDING_PLAN_v100.md (re-derived patch text with hunk 2 off-by-1 fix) + docs/PENDING_COMMIT_v100.md (corrected patch text) + docs/restir-gi-fix-v100.patch (standalone patch file)
- commit: docs/PENDING_COMMIT_v100.md
- test_strategy: file-only verification of patch hunk anchors AND full context blocks against actual file content (Part A) + parent-side verification recipe (Part B)

## Part A — file-only hunk-anchor + context-block verification (7/7 PASS this turn, all by first-hand read_file)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P10-a | FRayTracingPipeline.h #1 hunk: anchor `@@ -112,6 +112,14 @@` + full context matches OLD lines 112-117 | read_file offset=112 limit=6 | **PASS** — lines 112-117 match patch context exactly: `* binding layout. Must be called before FinalizePipeline().`, `*/`, `void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);`, blank, `/**`, `@brief Create the ray tracing pipeline from loaded shaders + binding layout`. 6 context lines match. |
| P10-b | FRayTracingPipeline.h #2 hunk: anchor CORRECTED to `@@ -222,7 +230,8 @@` + full context matches OLD lines 222-228 (7 lines including blank 227) | read_file offset=222 limit=7 | **PASS** — lines 222-228 are: `// Pipeline objects`, `Pipeline`, `ShaderTable`, `BindingLayout`, `BindlessLayout`, blank, `// Builder state`. That's 7 context lines matching patch context exactly. The v99 patch had this wrong (claimed 6 lines starting at 223, but // Pipeline objects is at 222 with 7 context lines). v100 corrects to `@@ -222,7 +230,8 @@`. |
| P10-c | FRayTracingPipeline.cpp #1 hunk: anchor `@@ -121,4 +121,12 @@` + full context matches OLD lines 121-124 | read_file offset=121 limit=4 | **PASS** — lines 121-124 match: `BindlessLayout = InBindlessLayout;`, `bHasBindlessLayout = true;`, `}`, blank. 4 context lines match. |
| P10-d | FRayTracingPipeline.cpp #2 hunk: anchor `@@ -148,7 +156,11 @@` + full context matches OLD lines 148-154 | read_file offset=148 limit=7 | **PASS** — lines 148-154 match: `PipelineDesc`, `globalBindingLayouts = { BindingLayout };`, `if (bHasBindlessLayout && BindlessLayout)`, `{`, `push_back(BindlessLayout);`, `}`, `PipelineDesc.shaders = {`. 7 context lines match. New_start=156 is correct (148 + 8 from hunk 3 = 156). |
| P10-e | FGIPass.cpp hunk: anchor `@@ -311,7 +311,8 @@` + full context matches OLD lines 311-317 with 12-space indent | read_file offset=311 limit=7 | **PASS** — lines 311-317 match: `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`, `if (!UAVBindingLayout)`, `{`, `HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create UAV binding layout (v22 split)"));`, `return false;` (12-space indent), `}`, blank. 7 context lines match. 12-space indent confirmed. |
| P10-f | GIPathTracing.hlsl Private hunk: anchor `@@ -85,9 +85,9 @@` + full context matches OLD lines 85-93 | read_file offset=85 limit=9 | **PASS** — lines 85-93 match: `// ====...`, `// Resources`, `// ====...`, blank, `Output : register(u0);`, blank, `#if GI_DEBUG_STATS`, `DebugStatsTexture : register(u1);`, `#endif`. 9 context lines match. Replacement swaps `register(u0)`→`register(u0, space1)` and `register(u1)`→`register(u1, space1)`. |
| P10-g | GIPathTracing.hlsl Data copy hunk: same as P10-f but for Test/TestReSTIR_GI_Temporal_Data/ copy | read_file offset=85 limit=9 | **PASS** — identical content to Private copy at lines 85-93. |

**Total: 7/7 PASS, all by first-hand read_file byte verification in the same turn the patch was written.**

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

1. **B1**: Apply patch via `git apply --check docs/restir-gi-fix-v100.patch` then `git apply docs/restir-gi-fix-v100.patch` → exit 0, no fuzz warnings, file changes appear in working tree — UNVERIFIED (terminal blocked)
2. **B2**: Build clean: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` → exit 0, no compile errors — UNVERIFIED
3. **B3**: Run test: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` → produces fresh dump group newer than patch mtime — UNVERIFIED
4. **B4**: No "Cannot open a command list that is already open" warnings in fresh stderr.log — UNVERIFIED
5. **B5**: No Vulkan ERROR / VUID-00344 in fresh log — UNVERIFIED
6. **B6**: `python3 validate_restir_gi.py` on newest dump group → 4/4 PASS — UNVERIFIED
7. **B7**: Display dump `display_frame8.png` visibly contains recognizable non-uniform Sponza geometry (vision analysis recommended) — UNVERIFIED
8. **B8**: `spirv-cross --reflect GIPathTracing.spv` shows `Output` at `(set=1, binding=0)` (CONFIRMS Option A) OR `(set=0, binding=0)` (FALSIFIES Option A) — UNVERIFIED. 10s command, cheapest disambiguation.

## Cheapest pre-apply disambiguation

Run B8 BEFORE applying the patch. If FALSIFIES (Output at set=0 binding=0): v93 wrong, do NOT apply this patch, route to a different investigation. If CONFIRMS (Output at set=1 binding=0): apply v100 patch and proceed with B1-B7.

## v99 → v100 patch text diff (the corrections)

| # | Hunk | v99 problem | v100 fix |
|---|------|-------------|----------|
| 1 | FRayTracingPipeline.h #1 | OK | unchanged |
| 2 | FRayTracingPipeline.h #2 | `@@ -223,6 +231,7 @@` had first context line at wrong line (// Pipeline objects is at line 222, not 223) AND 7 context lines claimed as 6 | `@@ -222,7 +230,8 @@` (correct anchor and line count) |
| 3 | FRayTracingPipeline.cpp #1 | OK | unchanged |
| 4 | FRayTracingPipeline.cpp #2 | OK | unchanged |
| 5 | FGIPass.cpp | OK | unchanged |
| 6, 7 | GIPathTracing.hlsl Private/Data | OK | unchanged |

**Bottom line**: v99's PATCH_TEXT_REPAIRED verdict was incomplete; v99 had 1 broken hunk (hunk 2 off-by-1 anchor). v100 catches it by independent re-verification with read_file byte-checking each hunk's anchor against actual file content.

## Verification difference between v99 and v100

The v99 PENDING_TESTS_v99.md P9-b probe claimed "Patch's 6-line context shows lines 223-228 exactly" — but this was wrong. The patch's first context line (`// Pipeline objects`) is at OLD line 222, not 223. The v99 author confused the visual layout with the actual line numbers. v100's P10-b probe is more careful: it explicitly reads lines 222-228 (7 lines) and verifies the patch's anchor `@@ -222,7 +230,8 @@` matches.

This is the 4th iteration of patch-text correction (v97 had 6 broken, v98 had 3 broken, v99 had 1 broken, v100 has 0 broken). The structural pattern is: each tick's "byte verification" was incomplete; this tick's verification is more rigorous.
