# Pending Tests v17

- plan: docs/PENDING_PLAN_v17.md
- commit: docs/PENDING_COMMIT_v17.md
- timestamp: 2026-07-27
- tester: tester (six-role-pipeline, single-head, file-only)

## Test 1: HLSL drift elimination (file-only, can run now)

- Goal: confirm Private master and data-dir copy of GIPathTracing.hlsl are byte-identical after v17 patch.
- Command: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
- Expected: empty output (or only header-comment whitespace differences).
- Status: file-only; can be run by parent in next session. Cron terminal blocked.

## Test 2: Build cleanliness (parent-driven)

- Goal: confirm clean build from current source tree.
- Command: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- Expected: 0 errors, 0 new warnings. The v17 patch adds 11 lines to each HLSL copy with no new keywords, casts, or includes — same risk profile as v15 case 6u.
- Status: parent-driven (cron terminal blocked).

## Test 3: Render regression at debugMode=0 (parent-driven)

- Goal: confirm mode 0 (default) behavior is unchanged from pre-v17.
- Command: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- Expected: identical gi_raw values to pre-v17 mode-0 dumps. The patch is gated by `if (debugMode != 0u)`, so mode 0 falls through to the unchanged default path.
- Status: parent-driven.

## Test 4: Mode 6 sentinel (parent-driven, carried from v15)

- Goal: confirm v15 case 6u UAV-write sentinel still works after v17 patch.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=6 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows per-pixel gradient `(float(pixel.x)/256, 0, float(pixel.y)/256)`. If mode 6 is 0, the dispatch isn't running OR slangc dead-stripped both cases (very unlikely).
- Status: parent-driven.

## Test 5: Mode 7 sentinel (parent-driven, new in v17)

- Goal: confirm v17 case 7u TraceRay-bypass sentinel produces the expected known lighting result.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=7 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows a non-zero image reflecting `diffuse * g_GI.AmbientColor.rgb * ambientScale`. Since `g_GI.AmbientColor.rgb=(1.0, 1.0, 1.0)` and `ambientScale=1.5`, the image should equal mode-1 (diffuse) scaled by 1.5. Predicted: mode 7 ≈ mode 1 × 1.5.
- Diagnostic interpretation:
  - Mode 7 produces non-zero scene-shape image → dispatch body runs, UAV write lands, GBufferMaterial SRV works, g_GI.AmbientColor uniform works, ambientScale uniform works, lighting math works. Bug is constrained to TraceRay / payload / SRV-read chain.
  - Mode 7 produces 0 → bug is in diffuse term (GBufferMaterial SRV), g_GI.AmbientColor uniform, ambientScale uniform, or slangc dead-stripped case 7u (very unlikely if mode 6 works).
  - Mode 7 produces garbage uniform → some downstream pass is overwriting OutputTexture after the GI dispatch.
- Status: parent-driven.

## Test 6: Mode 1 comparison (parent-driven, decodes mode 7)

- Goal: cross-reference mode 7 against mode 1 (diffuse) to confirm the "mode 7 = mode 1 × 1.5" prediction.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=1 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows the diffuse term (Sponza materials white = (1,1,1) everywhere). Mode 7 should equal mode 1 × 1.5 per pixel.
- Diagnostic interpretation: confirms the mode 7 multiplier is correct; if mode 7 = mode 1 × 1.5 exactly, the entire non-ray-tracing pipeline is verified.
- Status: parent-driven.

## Test 7: Validator at debugMode=0 (parent-driven, carried from v15)

- Goal: confirm 3/3 validator pass on the freshest dump group at mode 0.
- Command: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- Expected: 3/3 status. Mode 0 is unchanged by v17.
- Status: parent-driven.

## Test 8: stderr capture (parent-driven, carried from v12)

- Goal: confirm v12 cerr default-ON writes still fire after v17 patch.
- Command: same as Test 3 but with `2>stderr.log`. Expected: 8 `[RGI] Render() entry:` lines + 8 `[RGI] FGIPass::DispatchRays() entry:` lines per `HLVM_RGI_ACCUM=8` run.
- Status: parent-driven.

## Test 9: vision analysis of display + gi_raw (parent-driven, carried from v15)

- Goal: visually inspect display_frame8.png and gi_raw (modes 1, 6, 7) for recognizable non-uniform Sponza geometry.
- Command: open dumps with vision tools (or run pixel-statistics script).
- Expected: display shows Sponza geometry with sane exposure. Mode 1 shows white-ish Sponza materials. Mode 6 shows per-pixel gradient. Mode 7 shows Sponza × 1.5.
- Status: parent-driven (vision tools unavailable in cron).

## Test priority ordering

Tests 4 (mode 6) and 5 (mode 7) are the decisive new probes. Run them first to determine which branch the bug is on:
- Both modes produce expected output → bug is in ray-tracing chain (v18 stages next probe for TraceRay / payload)
- Mode 6 produces gradient but mode 7 is 0 → bug is in diffuse/AmbientColor/ambientScale uniforms (v18a)
- Both modes produce 0 → bug is in dispatch body / slangc dead-strip (v13a-2)
- Both modes produce garbage → bug is in downstream overwrite (v13a-3)

Then run Test 3 (mode 0) and Test 7 (validator) to confirm the default path is unchanged.

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: PASS — no imports, no Python code.
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A — no test files.
- [x] No source-incomplete-relative-to-test: PASS — source patch is complete; no test code references incomplete source.
- [x] No missing test isolation fixture: N/A — no test files.
- [x] No AsyncMock on sync function (or vice versa): N/A — no Python tests.

## Honest scope

Tests 1-9 are all parent-driven because the cron terminal is blocked by tirith. The only test that can be run in cron is Test 1 (file-only diff check), and even that requires shell access to invoke `diff`. The cron is structurally unable to verify the v17 patch's runtime effect without parent's interactive session.

This is consistent with all prior cycles' test staging (v1-v16). The patch is mechanically sound (verified by static inspection), the identifiers are correct (verified against the primary contribution expression), and the parent-driven tests cover the runtime verification path comprehensively.