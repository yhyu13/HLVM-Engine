# Pending Tests v98

- tests: docs/PENDING_PLAN_v98.md (corrected patch text) + PENDING_COMMIT_v98.md (final corrected patch text)
- commit: docs/PENDING_COMMIT_v98.md
- test_strategy: file-only verification of patch hunk anchors against actual file content (Part A) + parent-side verification recipe (Part B)

## Part A — file-only hunk-anchor verification (7/7 PASS this turn)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P8-a | FRayTracingPipeline.h #1 hunk: anchor `-112,6` matches actual lines 112-117 | read_file offset=112 limit=6 | PASS — lines 112-117 are `* binding layout...` through `@brief Create...`; matches patch context |
| P8-b | FRayTracingPipeline.h #2 hunk: anchor `-223,6` matches actual lines 223-228 | read_file offset=223 limit=6 | PASS — lines 223-228 are `Pipeline;`, `ShaderTable;`, `BindingLayout;`, `BindlessLayout;`, blank, `// Builder state`; matches patch context |
| P8-c | FRayTracingPipeline.cpp #1 hunk: anchor `-119,7` matches actual lines 119-125 | read_file offset=119 limit=7 | PASS — lines 119-125 are SetBindlessLayout signature/body/`}`/blank/FinalizePipeline signature; matches patch context |
| P8-d | FRayTracingPipeline.cpp #2 hunk: anchor `-148,7` matches actual lines 148-154 | read_file offset=148 limit=7 | PASS — lines 148-154 are PipelineDesc through PipelineDesc.shaders = {; matches patch context |
| P8-e | FGIPass.cpp hunk: anchor `-315,6` matches actual lines 315-320 | read_file offset=315 limit=6 | PASS — lines 315-320 are `return false;`, `}`, blank, SRV comment, we-only comment, `return true;`; matches patch context |
| P8-f | GIPathTracing.hlsl Private hunk: anchor `-85,9` matches actual lines 85-93 | read_file offset=85 limit=9 | PASS — lines 85-93 are Resources comment block through `Output : register(u0);`, blank, `#if GI_DEBUG_STATS`, `DebugStatsTexture : register(u1);`, `#endif`, blank; matches patch context |
| P8-g | GIPathTracing.hlsl Data copy hunk: same as P8-f but for Test/TestReSTIR_GI_Temporal_Data/ copy | read_file offset=85 limit=9 | PASS — identical content to Private copy at lines 85-93 |

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

1. **B1**: Apply patch: `git apply restir-gi-fix-v98.patch` → file changes appear in working tree — UNVERIFIED (terminal blocked)
2. **B2**: Build clean: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` → exit 0, no compile errors — UNVERIFIED
3. **B3**: Run test: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` → produces fresh dump group newer than patch mtime — UNVERIFIED
4. **B4**: No command-list-already-open warnings in fresh stderr.log — UNVERIFIED
5. **B5**: No Vulkan ERROR / VUID-00344 in fresh log — UNVERIFIED
6. **B6**: `python3 validate_restir_gi.py` on newest dump group → 4/4 PASS — UNVERIFIED
7. **B7**: Display dump `display_frame8.png` visibly contains recognizable non-uniform Sponza geometry — UNVERIFIED
8. **B8**: `spirv-cross --reflect GIPathTracing.spv` shows `Output` at `(set=1, binding=0)` (CONFIRMS Option A is correct) OR `(set=0, binding=0)` (FALSIFIES Option A, fix is elsewhere) — UNVERIFIED. 10s command, cheapest disambiguation.

## Cheapest pre-apply disambiguation
Run B8 BEFORE applying the patch. If FALSIFIES (Output at set=0 binding=0): v93 is wrong, do NOT apply this patch; route to a different investigation. If CONFIRMS (Output at set=1 binding=0): apply the patch and proceed with B1-B7.

## v97 → v98 patch text diff (the corrections)

The v97 patch text had 6 broken hunks that v98 fixes:

| Hunk | v97 bug | v98 fix |
|------|---------|---------|
| FRayTracingPipeline.h #2 | `@@ -240,6 +240,9 @@` + WRONG PATH (Private/ not Public/) | `@@ -223,6 +231,7 @@` with correct Public/ path |
| FRayTracingPipeline.cpp #1 | `@@ -119,6 +119,13 @@` (6 context / 13 new — wrong) | `@@ -119,7 +119,15 @@` (7 context / 15 new) |
| FRayTracingPipeline.cpp #2 | `@@ -148,6 +155,10 @@` (wrong new_start) | `@@ -148,7 +156,11 @@` (correct cumulative new_start) |
| FGIPass.cpp | `@@ -313,6 +313,7 @@` (2-line offset error) | `@@ -315,6 +315,7 @@` (correct anchor) |
| GIPathTracing.hlsl Data copy last `+` | Typo `RWTexture2D<floatLayoutHandle>` | Corrected to `RWTexture2D<float4>` |
| Other 3 hunks | (no bugs) | Kept verbatim |