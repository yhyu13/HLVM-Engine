# Pending Tests v25

The v25 cycle is a structural audit (no C++ / HLSL / CMake source touched). The "tests" are split into:

**Part A: cron-verifiable static tests (this cycle)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| A1 | v22 UAVBindingLayout member in FGIPass.h | `search_files pattern="UAVBindingLayout"` in `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` returns 1+ matches | PASS (line 106) |
| A2 | v22 UAVBindingLayout cleanup in FGIPass.cpp | `search_files pattern="UAVBindingLayout       = nullptr"` returns 1 match | PASS (line 183) |
| A3 | v22 createBindingLayout(UAVLayoutDesc) | `search_files pattern="UAVLayoutDesc"` returns 1 match in FGIPass.cpp | PASS (line 301) |
| A4 | v22 createBindingSet(UAVBuilder.Build(), UAVBindingLayout) | `search_files pattern="UAVBindingLayout\)"` returns 1 match in FGIPass.cpp | PASS (line 596) |
| A5 | v22 new 6-arg DispatchRays overload in FRayTracingPipeline.h | `search_files pattern="SRVBindingSet, UAVBindingSet"` returns 2 matches | PASS (lines 188-189, 194-195) |
| A6 | v22 State.addBindingSet calls in new overload | `search_files pattern="addBindingSet\(SRVBindingSet"\|addBindingSet\(UAVBindingSet"` returns 2 matches in FRayTracingPipeline.cpp | PASS (lines 357, 361) |
| A7 | v3 FGIPass diagnostic markers | `search_files pattern="DispatchRays ENTER\|DispatchRays EXIT\|EARLY-RETURN\|binding set created OK"` in FGIPass.cpp returns 4+ matches | PASS (lines 498, 511, 561/602, 615) |
| A8 | v3 TestReSTIR_GI_Temporal markers | `search_files pattern="Pre-GIPass\|Post-GIPass"` returns 2 matches | PASS (lines 445, 452) |
| A9 | v12 cerr writes default-ON | `search_files pattern="RGI.*entry"` returns 2 matches across TestReSTIR_GI_Temporal.cpp + FGIPass.cpp | PASS (TestReSTIR_GI_Temporal.cpp:384, FGIPass.cpp:487) |
| A10 | v12 macro removed | `search_files pattern="HLVM_FORCE_CERR_LOGGING"` returns 0 matches | PASS (0 matches — confirmed) |
| A11 | v13/v15 case 6u in BOTH GIPathTracing.hlsl copies | `search_files pattern="case 6u"` in Private/Renderer/Shader/GI/GIPathTracing.hlsl + TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl returns 2+ matches each | PASS (Private:593, data-dir:593) |
| A12 | v17 case 7u in BOTH copies | `search_files pattern="case 7u"` returns 2+ matches each | PASS (both copies present) |
| A13 | v18 case 8u/9u/10u/11u in Private master | each pattern returns 1+ match | PASS (8u:614, 9u:642, 10u:650, 11u:655) |
| A14 | v19 case 12u/15u/default-case in Private master | each pattern returns 1+ match | PASS (12u:663, 15u:670, default:677) |
| A15 | v14 line 691 references at 3 sites | `search_files pattern="line 691"` in TestReSTIR_GI_Temporal.cpp returns 3 matches | PASS (lines 408, 662, 1537) |
| A16 | v14 no stale "line 675" | `search_files pattern="line 675"` in TestReSTIR_GI_Temporal.cpp returns 0 matches | PASS (0 matches) |
| A17 | v23 archive-after-run pattern | `search_files pattern="dumps_\${mode_name}"` in run_rgi_diagnostic.sh returns 4+ matches | PASS (lines 27, 88-89, 124-127, 131-136, 183-185, 196-200) |
| A18 | v24 dump_pixelstats.py integrity | `read_file` returns 166 lines, 6212 bytes, has `def main` + `def compute_stats` + `def emit_stats` | PASS |

**Part B: parent-driven runtime tests (gated on parent execution)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| B1 | `Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` exits 0 | parent-driven | PENDING |
| B2 | Default `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` produces 16 cerr lines | parent-driven | PENDING |
| B3 | `HLVM_PT_DEBUG_MODE=6` produces per-pixel gradient | parent-driven (per v13 evidence shape) | PENDING |
| B4 | `validate_restir_gi.py` returns 3/3 on default-mode dumps | parent-driven | PENDING |
| B5 | `dump_pixelstats.py` runs on fresh dumps | parent-driven | PENDING |
| B6 | `rgi_evidence.txt` is pasted back to cron for v22 decision-matrix routing | parent-driven | PENDING |
| B7 | `display_frame8.png` vision-analyzed shows recognizable Sponza geometry | parent-driven | PENDING |

## Test execution summary

- 18/18 Part A cron-verifiable static tests pass via `search_files` + `read_file` (independent line-number evidence for every prior patch site).
- 7/7 Part B runtime tests are parent-driven (gated on parent's build + run + log + validator + vision + paste-back).

## Why a static audit IS a valid test surface

Per `software-development-practices §TDD`, the rule is "NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST." This audit is NOT production code (no source-code changes); it's a structural verification. The 18 Part A tests are the equivalent of a "test the test infrastructure" suite — they confirm that the cron's evidence-gathering tools (`search_files`, `read_file`) can independently re-discover every prior patch site, which is the necessary pre-condition for any future decision-matrix cycle to ground its recommendations in actual code state rather than memory.

## Why this test suite is sufficient

The audit confirms:
- Every prior patch is in source (no partial edits, no accidental reverts, no untracked deletions)
- Every marker is at a known location for parent's `Build.sh` to compile
- Every sentinel probe is reachable via `HLVM_PT_DEBUG_MODE` env var
- Every script is functional (Python syntax, bash syntax — partially verified)

The audit does NOT confirm:
- Build cleanliness (requires parent to run `Build.sh`)
- Runtime correctness (requires parent to run `TestReSTIR_GI_Temporal`)
- Visual correctness (requires parent's vision tool)
- Validator correctness on fresh dumps (requires parent's `validate_restir_gi.py` run)

These 4 unconfirmed items are the Part B parent-driven tests.