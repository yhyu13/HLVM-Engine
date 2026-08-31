# Pending Tests v99

- tests: docs/PENDING_PLAN_v99.md (re-derived patch text) + docs/PENDING_COMMIT_v99.md (corrected patch text) + docs/restir-gi-fix-v99.patch (standalone patch file)
- commit: docs/PENDING_COMMIT_v99.md
- test_strategy: file-only verification of patch hunk anchors AND full context blocks against actual file content (Part A) + parent-side verification recipe (Part B)

## Part A — file-only hunk-anchor + context-block verification (7/7 PASS this turn, all by first-hand read_file)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P9-a | FRayTracingPipeline.h #1 hunk: anchor `-112,6` + full context matches lines 112-117 | read_file offset=112 limit=6 | **PASS** — lines 112-117 are `The bindless layout is added...`, `* binding layout...`, `*/`, `void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);`, blank, `/**`, `@brief Create...`; matches patch context exactly (6 lines: signature comment closure through `@brief Create`) |
| P9-b | FRayTracingPipeline.h #2 hunk: anchor `-223,6` + full context matches lines 223-228 (correcting +8 cumulative offset for new_start=231) | read_file offset=218 limit=12 | **PASS** — lines 218-228 are (218=`nvrhi::ShaderHandle ClosestHitShader;`, 219=`nvrhi::ShaderHandle MissShader;`, 220=`nvrhi::ShaderHandle ShadowMissShader;`, 221=blank, 222=`// Pipeline objects`, 223=`nvrhi::rt::PipelineHandle Pipeline;`, 224=`nvrhi::rt::ShaderTableHandle ShaderTable;`, 225=`nvrhi::BindingLayoutHandle BindingLayout;`, 226=`nvrhi::BindingLayoutHandle BindlessLayout;`, 227=blank, 228=`// Builder state`). Patch's 6-line context shows lines 223-228 exactly. ✓ |
| P9-c | FRayTracingPipeline.cpp #1 hunk: anchor `-121,4` (4 context lines starting at 121: lines 121,122,123,124) AND continuation line 125 starting next hunk | read_file offset=119 limit=8 | **PASS** — lines 119-126 are: 119=`void FRayTracingPipeline::SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout)`, 120=`{`, 121=`BindlessLayout = InBindlessLayout;`, 122=`bHasBindlessLayout = true;`, 123=`}`, 124=blank, 125=`bool FRayTracingPipeline::FinalizePipeline(uint32_t MaxPayloadSize, uint32_t MaxAttributeSize)`, 126=`{`. The patch's OLD context (lines 121-124) match lines 121-124 exactly: `BindlessLayout = InBindlessLayout;`, `bHasBindlessLayout = true;`, `}`, blank. ✓ |
| P9-d | FRayTracingPipeline.cpp #2 hunk: anchor `-148,7` + new_start `+156` + full context matches lines 148-154 (corrected +8 cumulative offset) | read_file offset=148 limit=8 | **PASS** — lines 148-154 are: 148=`nvrhi::rt::PipelineDesc PipelineDesc;`, 149=`PipelineDesc.globalBindingLayouts = { BindingLayout };`, 150=`if (bHasBindlessLayout && BindlessLayout)`, 151=`{`, 152=`PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);`, 153=`}`, 154=`PipelineDesc.shaders = {`. Patch's 7-line context matches lines 148-154 exactly. New_start=156 is correct (148 + 8 from hunk 3 = 156). ✓ |
| P9-e | FGIPass.cpp hunk: anchor `-311,7` (7 context lines starting at 311: UAVBindingLayout creation through both comments) + 12-space indentation matching actual file | read_file offset=300 limit=22 | **PASS** — lines 311-318 are: 311=`UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`, 312=`if (!UAVBindingLayout)`, 313=`{`, 314=`HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create UAV binding layout (v22 split)"));`, 315=`return false;` (12-space indent), 316=`}`, 317=blank, 318=`// The actual SRV binding layout handle is created inside FRayTracingPipeline::FinalizePipeline();`. Patch's 7 context lines are 311-317 with 12-space indent on `return false;`. ✓ |
| P9-f | GIPathTracing.hlsl Private hunk: anchor `-85,9` + full context matches lines 85-93 | read_file offset=80 limit=15 | **PASS** — lines 85-93 are: 85=`// =============================================================================`, 86=`// Resources`, 87=`// =============================================================================`, 88=blank, 89=`RWTexture2D<float4> Output : register(u0);`, 90=blank, 91=`#if GI_DEBUG_STATS`, 92=`RWTexture2D<float4> DebugStatsTexture : register(u1);`, 93=`#endif`. Patch's 9-line context matches lines 85-93 exactly. The replacement swaps `register(u0)`→`register(u0, space1)` and `register(u1)`→`register(u1, space1)`. ✓ |
| P9-g | GIPathTracing.hlsl Data copy hunk: same as P9-f but for Test/TestReSTIR_GI_Temporal_Data/ copy | read_file offset=80 limit=15 | **PASS** — identical content to Private copy at lines 85-93. ✓ |

**Total: 7/7 PASS, all by first-hand read_file byte verification in the same turn the patch was written**.

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

1. **B1**: Apply patch via `git apply --check docs/restir-gi-fix-v99.patch` then `git apply docs/restir-gi-fix-v99.patch` → exit 0, no fuzz warnings, file changes appear in working tree — UNVERIFIED (terminal blocked)
2. **B2**: Build clean: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` → exit 0, no compile errors — UNVERIFIED
3. **B3**: Run test: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` → produces fresh dump group newer than patch mtime — UNVERIFIED
4. **B4**: No "Cannot open a command list that is already open" warnings in fresh stderr.log — UNVERIFIED
5. **B5**: No Vulkan ERROR / VUID-00344 in fresh log — UNVERIFIED
6. **B6**: `python3 validate_restir_gi.py` on newest dump group → 4/4 PASS — UNVERIFIED
7. **B7**: Display dump `display_frame8.png` visibly contains recognizable non-uniform Sponza geometry (vision analysis recommended) — UNVERIFIED
8. **B8**: `spirv-cross --reflect GIPathTracing.spv` shows `Output` at `(set=1, binding=0)` (CONFIRMS Option A) OR `(set=0, binding=0)` (FALSIFIES Option A) — UNVERIFIED. 10s command, cheapest disambiguation.

## Cheapest pre-apply disambiguation
Run B8 BEFORE applying the patch. If FALSIFIES (Output at set=0 binding=0): v93 wrong, do NOT apply this patch, route to a different investigation. If CONFIRMS (Output at set=1 binding=0): apply v99 patch and proceed with B1-B7.

## v98 → v99 patch text diff (the corrections)

| # | Hunk | v98 bug (independently re-verified) | v99 fix |
|---|------|-------------------------------------|---------|
| 1 | FRayTracingPipeline.h #1 | OK | unchanged |
| 2 | FRayTracingPipeline.h #2 | OK | unchanged |
| 3 | FRayTracingPipeline.cpp #1 | `@@ -119,6 +119,13 @@` context omits lines 119-120 (sig + `{`) — off by 2 | `@@ -121,4 +121,12 @@` (4 context lines 121-124 instead of 6) |
| 4 | FRayTracingPipeline.cpp #2 | `@@ -148,7 +148,11 @@` new_start must be 156 (cumulative +8 from hunk 3) | `@@ -148,7 +156,11 @@` |
| 5 | FGIPass.cpp | `@@ -315,6 +315,7 @@` used 8-space indent, actual file has 12-space | `@@ -311,7 +311,8 @@` with 12-space indent |
| 6, 7 | GIPathTracing.hlsl Private/Data | OK | unchanged |

**Bottom line**: v98's PATCH_TEXT_CORRECTED verdict was wrong; the v98 patch text has 3 broken hunks. v99 re-derives each hunk from first-hand byte verification.
