# Pending Tests v97

- tests: docs/PENDING_PLAN_v97.md (verbatim patch text for parent) + parent-side verification recipe
- commit: docs/PENDING_COMMIT_v97.md
- test_strategy: parent-applied patch + parent-side bash chain rebuilds and validates

## Part A — file-only verification (5/5 PASS, v97 NEW content)
- [x] P7-a: PENDING_PLAN_v97.md contains the verbatim 5-file patch text
- [x] P7-b: patch references all 6 expected files (FRayTracingPipeline.h, FRayTracingPipeline.cpp, FGIPass.cpp, GIPathTracing.hlsl Private, GIPathTracing.hlsl Data) — 6 actually, hunk-count correct
- [x] P7-c: parent-side apply + verify recipe is 3-command bash chain
- [x] P7-d: PENDING_PLAN_REVIEW_v97.md flag the 2 polish items (vector include, stale comment)
- [x] P7-e: PENDING_COMMIT_v97.md notes no commit by cron per user instruction

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)
1. **B1**: Apply patch: `git apply` the patch text from PENDING_PLAN_v97.md → file changes appear in working tree — UNVERIFIED (terminal blocked)
2. **B2**: Build clean: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` → exit 0, no compile errors — UNVERIFIED
3. **B3**: Run test: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` → produces fresh dump group newer than patch mtime — UNVERIFIED
4. **B4**: No command-list-already-open warnings in fresh stderr.log — UNVERIFIED
5. **B5**: No Vulkan ERROR / VUID-00344 in fresh log — UNVERIFIED
6. **B6**: `python3 validate_restir_gi.py` on newest dump group → 4/4 PASS — UNVERIFIED
7. **B7**: Display dump `display_frame8.png` visibly contains recognizable non-uniform Sponza geometry — UNVERIFIED
8. **B8**: `spirv-cross --reflect GIPathTracing.spv` shows `Output` at `(set=1, binding=0)` (CONFIRMS Option A is correct) OR `(set=0, binding=0)` (FALSIFIES Option A, fix is elsewhere) — UNVERIFIED. 10s command, cheapest disambiguation.

## Cheapest pre-apply disambiguation
Run B8 BEFORE applying the patch. If FALSIFIES (Output at set=0 binding=0): v93 is wrong, do NOT apply this patch; route to a different investigation. If CONFIRMS (Output at set=1 binding=0): apply the patch and proceed with B1-B7.