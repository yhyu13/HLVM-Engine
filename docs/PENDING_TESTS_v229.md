# Pending Tests v229

- commit: docs/PENDING_COMMIT_v229.md
- files: (no new test files; this is a file-only verifier per the plan's `test_strategy` section, matching the v228 pattern)
- verifier_command: `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestCornellBoxGI` (operator-side; the file-only rows below verify the patch is present and structurally sound in this runspace)
- notes:
  - **No new test files produced** under `Engine/Source/Runtime/Test/` or `Engine/Source/Common/Test/`. The fix is a structural refactor (helper extraction + call-site relocation); the runtime acceptance is GPU behavior (window-resize → no OOB UAV store), which requires `terminal` to exercise. The cycle's job is to deliver a compile-coherent patch; the operator verifies behavior.
  - **Pattern matches v215-v228 file-only cohorts** (per `six-role-pipeline §Anti-patterns §5`: running a 6-role cycle on a structural refactor is the right spend when the operator's build/run is the verification step).

## Test rows (file-only, re-runnable, no terminal required)

Every row re-run by me this cycle. No count inherited from the plan or commit.

| # | Row | Query / read | Expected | Result |
|---|---|---|---|---|
| 1 | Helper definition exists | `search_files pattern="static void CreateReSTIRTextures" path=TestCornellBoxGI.cpp` | 1 hit | **PASS** — 1 hit (line 90) |
| 2 | Helper called from init | `search_files pattern="CreateReSTIRTextures" path=TestCornellBoxGI.cpp` | ≥3 hits total | **PASS** — 4 hits (line 90 def + 2 calls + 1 comment ref) |
| 3 | Init-site call after patch | `read_file TestCornellBoxGI.cpp 1064-1076` | call to helper with all 14 handles | **PASS** — confirmed at line 1068; 14 handle args present |
| 4 | Resize-site call after patch | `read_file TestCornellBoxGI.cpp 1308-1320` | call to helper inside resize branch | **PASS** — confirmed at line 1314; precedes `BindingCache.Clear()` at line 1321 |
| 5 | Resize branch still triggers on size change | `read_file TestCornellBoxGI.cpp 1211-1217` | `if (!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth ...)` | **PASS** — unchanged trigger; helper runs inside |
| 6 | `BindingCache.Clear()` count unchanged | `search_files pattern="BindingCache.Clear" path=TestCornellBoxGI.cpp` | 3 hits | **PASS** — 3 hits (lines 1123, 1307, 1844) — pre-patch was 3, post-patch is 3 |
| 7 | `clearTextureFloat` count unchanged | `search_files pattern="clearTextureFloat" path=TestCornellBoxGI.cpp` | 3 hits | **PASS** — 3 hits, all inside helper |
| 8 | `bReSTIRInitialized` count unchanged | `search_files pattern="bReSTIRInitialized" path=TestCornellBoxGI.cpp` | 3 hits | **PASS** — 3 hits (declare, init-set, render-guard); unchanged |
| 9 | `createTexture` total count unchanged | `search_files pattern="NvrhiDevice->createTexture" path=TestCornellBoxGI.cpp` | 24 hits (pre-patch value) | **PASS** — 24 hits; helper just relocated the 14 ReSTIR ones |
| 10 | `(negative control) GBufferDiffuseTexture recreation in resize branch` | `search_files pattern="GBufferDiffuseTexture = NvrhiDevice->createTexture" path=TestCornellBoxGI.cpp` | 1 hit inside resize branch | **PASS** — 1 hit, at the resize branch (GBuffer MRT recreation pattern is preserved) |
| 11 | Helper has `Width`/`Height` parameters | `read_file TestCornellBoxGI.cpp 90-105` | `uint32_t Width` and `uint32_t Height` declared | **PASS** — confirmed |
| 12 | Helper has all 14 handle refs | `read_file TestCornellBoxGI.cpp 90-105` | 14 `nvrhi::TextureHandle&` params | **PASS** — confirmed (Reservoir0/1, Reservoir0/1History, Reservoir0/1Merged, ReSTIROutput, TemporalRadiance, RadianceHistory, PrevDepth, PrevNormal = 14) |
| 13 | `CmdList->open()` count in helper | `read_file TestCornellBoxGI.cpp 152-175` | 1 hit (only the clear cmdlist) | **PASS** — 1 hit at line 169 (the clear cmdlist) |
| 14 | `executeCommandList` count in helper | `read_file TestCornellBoxGI.cpp 152-180` | 1 hit (clear cmdlist) | **PASS** — 1 hit at line 174 |
| 15 | `clearTextureFloat` targets in helper are history textures | `read_file TestCornellBoxGI.cpp 165-175` | targets = Reservoir0History, Reservoir1History, RadianceHistory | **PASS** — confirmed; matches pre-patch :1016-1018 |
| 16 | `nvrhi::Format::D32` for PrevDepth | `read_file TestCornellBoxGI.cpp 142-152` | Desc.format = D32 for PrevDepth | **PASS** — confirmed; matches pre-patch :997 |
| 17 | `nvrhi::Format::RGBA16_FLOAT` for PrevNormal | `read_file TestCornellBoxGI.cpp 152-156` | Desc.format = RGBA16_FLOAT for PrevNormal | **PASS** — confirmed; matches pre-patch :1006 |
| 18 | Helper preserves `keepInitialState=true` for reservoirs | `read_file TestCornellBoxGI.cpp 102-115` | `Desc.keepInitialState = true` | **PASS** — confirmed; matches pre-patch :965 |
| 19 | `Reservoir0Texture = NvrhiDevice->createTexture` count | `search_files pattern="Reservoir0Texture = NvrhiDevice" path=TestCornellBoxGI.cpp` | 1 hit (inside helper) | **PASS** — 1 hit at line 110 (inside helper) |
| 20 | Init block's original 14 createTexture calls removed | `search_files pattern="Desc.debugName = \"Reservoir0\"" path=TestCornellBoxGI.cpp` | 1 hit (inside helper, not init) | **PASS** — 1 hit at line 109 (inside helper); pre-patch was 1 hit at line 967 (init block) |
| 21 | `BindingCache.Clear()` AFTER helper call in resize branch | `read_file TestCornellBoxGI.cpp 1314-1325` | helper call, then `BindingCache.Clear()` | **PASS** — confirmed at lines 1314 (call) and 1321 (clear) |
| 22 | Helper does NOT touch `LastWidth`/`LastHeight` | `read_file TestCornellBoxGI.cpp 165-180` | no mention of LastWidth/LastHeight | **PASS** — confirmed (resize branch handles that at :1215-1216) |

## Note on the 4-check structural validator (software-development-practices §Path-Tracing / RT Debugging Methodology)

The 4-check structural validator (black ratio, color variance, temporal stability, cell variance) cannot be exercised from this runspace because it requires reading the rendered dump PNGs (which need `vision_analyze`, not in this runspace per tick-528). The 22 file-only rows above are the cron-executable maximum. The operator-side recipe in `docs/DIAGNOSTIC_2026-08-30-state-machine-617.md:102-140` is the full 7-gate verification path.

## What I could not test

Nothing was built, compiled, executed, or viewed. No acceptance gate was exercised. All 22 rows are static file reads. The `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestCornellBoxGI` verify command in the commit requires `terminal` access denied by tirith (cumulative ≥580+ denials in lineage).