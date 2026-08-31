# Pending Tests v19

- plan: docs/PENDING_PLAN_v19.md
- commit: docs/PENDING_COMMIT_v19.md
- timestamp: 2026-07-27
- tester: tester (six-role-pipeline, single-head, file-only)

## Test 1: HLSL drift elimination (file-only)

- Goal: confirm Private master and data-dir copy of GIPathTracing.hlsl are byte-identical after v19 patch.
- Command: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
- Expected: empty output.
- Status: file-only; parent can run with shell access.

## Test 2: Build cleanliness (parent-driven)

- Goal: confirm clean build from current source tree.
- Command: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- Expected: 0 errors, 0 new warnings.
- Status: parent-driven (cron terminal blocked).

## Test 3: Render regression at debugMode=0 (parent-driven)

- Goal: confirm mode 0 (default) behavior is unchanged from pre-v19.
- Command: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- Expected: identical gi_raw values to pre-v19 mode-0 dumps. All v19 changes are gated by `if (debugMode != 0u)`, and the default-case modification only fires for debugMode not in {1..15}.
- Status: parent-driven.

## Test 4: Mode 12 sentinel — AmbientColor-only (parent-driven)

- Goal: confirm v19 case 12u writes `g_GI.AmbientColor.rgb` to OutputTexture (verifies AmbientColor uniform independently of diffuse and ambientScale).
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=12 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows uniform (1, 1, 1) for every pixel since AmbientColor = (1, 1, 1, 1) per TestReSTIR_GI_Temporal.cpp:441.
- Diagnostic interpretation:
  - Mode 12 = (1, 1, 1) → AmbientColor uniform is healthy
  - Mode 12 = 0 → AmbientColor not bound
  - Mode 12 ≠ (1, 1, 1) and ≠ 0 → AmbientColor uniform has wrong value
- Status: parent-driven.

## Test 5: Mode 15 sentinel — debugMode raw value (parent-driven)

- Goal: confirm v19 case 15u writes `g_GI.Params5.x` raw (no /256 divide) to OutputTexture (sanity check on mode 10).
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=15 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows R=G=B = 15.0 for every pixel (HDR raw radiance before tonemap).
- Diagnostic interpretation:
  - Mode 15 = 15.0 → Params5.x is being set correctly (matches debugMode=15)
  - Mode 15 = 0 → Params5.x is 0 (cbuffer not updated)
  - Mode 15 ≠ 15 and ≠ 0 → Params5.x is being set to a wrong value
- Status: parent-driven.

## Test 6: Mode 99 (default-case trace) sentinel (parent-driven)

- Goal: confirm v19 default-case modification writes gray when debugMode is not in {1..15} (canonical catch-all sentinel).
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=99 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows uniform (0.5, 0.5, 0.5) gray for every pixel (the default-case trace fires for debugMode=99).
- Diagnostic interpretation:
  - Mode 99 = (0.5, 0.5, 0.5) → switch is being entered, no valid case matched, default-case trace fires correctly
  - Mode 99 = 0 → switch not entered at all (debugMode = 0 issue)
  - Mode 99 ≠ (0.5, 0.5, 0.5) and ≠ 0 → switch entered but default-case is not firing (impossible if HLSL compiled correctly)
- Status: parent-driven.

## Test 7: Validator at debugMode=0 (parent-driven, carried from v18)

- Goal: confirm 3/3 validator pass on the freshest dump group at mode 0.
- Command: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- Expected: 3/3 status. Mode 0 is unchanged by v19.
- Status: parent-driven.

## Test 8: stderr capture (parent-driven, carried from v18)

- Goal: confirm v12 cerr default-ON writes still fire after v19 patch.
- Command: same as Test 3 but with `2>stderr.log`. Expected: 8 `[RGI] Render() entry:` lines + 8 `[RGI] FGIPass::DispatchRays() entry:` lines per `HLVM_RGI_ACCUM=8` run.
- Status: parent-driven.

## Test priority ordering

Tests 4 (mode 12), 5 (mode 15), and 6 (mode 99 default) are the decisive new probes. Combined with v18's modes 8/9/10/11 and v17's mode 7 and v13's mode 6, the parent can bisect every hypothesis in a single rebuild + 9 mode runs.

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: PASS
- [x] No test-bug-in-itself: N/A
- [x] No source-incomplete-relative-to-test: PASS
- [x] No missing test isolation fixture: N/A
- [x] No AsyncMock on sync function: N/A

## Honest scope

All 8 tests are parent-driven because the cron's terminal is blocked. The patch is mechanically sound (verified by static inspection), identifiers are correct (verified against existing usage), and parent-driven tests cover the runtime verification path comprehensively.