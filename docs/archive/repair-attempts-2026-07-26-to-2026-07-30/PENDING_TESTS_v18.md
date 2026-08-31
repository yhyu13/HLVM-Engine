# Pending Tests v18

- plan: docs/PENDING_PLAN_v18.md
- commit: docs/PENDING_COMMIT_v18.md
- timestamp: 2026-07-27
- tester: tester (six-role-pipeline, single-head, file-only)

## Test 1: HLSL drift elimination (file-only, can run now)

- Goal: confirm Private master and data-dir copy of GIPathTracing.hlsl are byte-identical after v18 patch.
- Command: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
- Expected: empty output (or only header-comment whitespace differences).
- Status: file-only; can be run by parent in next session. Cron terminal blocked.

## Test 2: Build cleanliness (parent-driven)

- Goal: confirm clean build from current source tree.
- Command: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- Expected: 0 errors, 0 new warnings. The v18 patch adds 51 lines to each HLSL copy with no new keywords, casts, or includes — same risk profile as v17.
- Status: parent-driven (cron terminal blocked).

## Test 3: Render regression at debugMode=0 (parent-driven)

- Goal: confirm mode 0 (default) behavior is unchanged from pre-v18.
- Command: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- Expected: identical gi_raw values to pre-v18 mode-0 dumps. The patch is gated by `if (debugMode != 0u)`, so mode 0 falls through to the unchanged default path.
- Status: parent-driven.

## Test 4: Mode 8 sentinel — TraceRay-only (parent-driven)

- Goal: confirm v18 case 8u TraceRay-only sentinel produces expected output without crashing.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=8 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows a per-pixel value where R = 1.0 if `tracePayload.hitDistance > 0` (ray hit geometry), R = 0.0 if missed; G = `hitDistance * 0.1`; B = `flags / 8`. Pixels where the ray hits should appear green-ish (R+G), pixels where it misses appear red-ish (B only).
- Diagnostic interpretation:
  - Mode 8 doesn't crash + gi_raw shows per-pixel variation → TraceRay setup is healthy; bug is in payload write or post-dispatch merge
  - Mode 8 crashes the dispatch → bug is in TraceRay setup (RT flags, TMin/TMax, BVH traversal)
  - Mode 8 produces all-NaN or all-0 → TraceRay is returning garbage
- Status: parent-driven.

## Test 5: Mode 9 sentinel — diffuse × 1.5 (parent-driven)

- Goal: confirm v18 case 9u diffuse-only sentinel shows scene-shape identical to mode 1 × 1.5.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=9 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows scene-shape image, with values exactly 1.5× mode 1's values per pixel.
- Diagnostic interpretation:
  - Mode 9 = mode 1 × 1.5 → GBufferMaterial SRV is healthy; uniforms are decoupled from the bug
  - Mode 9 = 0 → GBufferMaterial SRV binding is broken
  - Mode 9 differs from mode 1 × 1.5 → multiplier is wrong (unlikely)
- Status: parent-driven.

## Test 6: Mode 10 sentinel — debugMode cbuffer reach (parent-driven)

- Goal: confirm v18 case 10u writes `g_GI.Params5.x / 256.0` to OutputTexture (verifies GI cbuffer reach).
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=10 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows R ≈ 0.04 (= 10 / 256, since debugMode=10), G = 0, B = 0 for every pixel (uniform color).
- Diagnostic interpretation:
  - Mode 10 = (0.04, 0, 0) → GI cbuffer reach is fine
  - Mode 10 = 0 → GI cbuffer not bound or not being updated by FGIPass::WriteConstants
  - Mode 10 ≠ (0.04, 0, 0) and ≠ 0 → GI cbuffer is bound but Params5.x is being set to a different value than expected
- Status: parent-driven.

## Test 7: Mode 11 sentinel — View cbuffer reach (parent-driven)

- Goal: confirm v18 case 11u writes `g_View.FrameIndex / 256.0` to OutputTexture (verifies View cbuffer reach).
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=11 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows a uniform gray value at all pixels with R=G=B = FrameIndex / 256.
- Diagnostic interpretation:
  - Mode 11 = (X, X, X) where X > 0 → View cbuffer reach is fine
  - Mode 11 = 0 → View cbuffer not bound
  - Mode 11 = NaN or garbage → View cbuffer bind has wrong data
- Status: parent-driven.

## Test 8: Mode 1 comparison (parent-driven, decodes mode 9)

- Goal: cross-reference mode 9 against mode 1 (diffuse) to confirm the "mode 9 = mode 1 × 1.5" prediction.
- Command: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=1 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal`
- Expected: gi_raw shows the diffuse term (Sponza materials white = (1,1,1) everywhere). Mode 9 should equal mode 1 × 1.5 per pixel.
- Diagnostic interpretation: confirms the mode 9 multiplier is correct; if mode 9 = mode 1 × 1.5 exactly, the entire non-ray-tracing pipeline is verified (mode 7 = mode 9 means uniforms are correct too).
- Status: parent-driven.

## Test 9: Validator at debugMode=0 (parent-driven, carried from v17)

- Goal: confirm 3/3 validator pass on the freshest dump group at mode 0.
- Command: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- Expected: 3/3 status. Mode 0 is unchanged by v18.
- Status: parent-driven.

## Test 10: stderr capture (parent-driven, carried from v17)

- Goal: confirm v12 cerr default-ON writes still fire after v18 patch.
- Command: same as Test 3 but with `2>stderr.log`. Expected: 8 `[RGI] Render() entry:` lines + 8 `[RGI] FGIPass::DispatchRays() entry:` lines per `HLVM_RGI_ACCUM=8` run.
- Status: parent-driven.

## Test 11: vision analysis of display + gi_raw (parent-driven, carried from v17, expanded to modes 8/9/10/11)

- Goal: visually inspect display_frame8.png and gi_raw (modes 1, 6, 7, 8, 9, 10, 11) for recognizable non-uniform Sponza geometry.
- Command: open dumps with vision tools (or run pixel-statistics script).
- Expected: display shows Sponza geometry with sane exposure. Mode 1 shows white-ish Sponza materials. Mode 6 shows per-pixel gradient. Mode 7 shows Sponza × 1.5. Mode 8 shows hit/miss pattern (green where ray hit, red where missed). Mode 9 shows Sponza × 1.5 (should match mode 7). Mode 10 shows uniform (0.04, 0, 0). Mode 11 shows uniform gray (FrameIndex / 256).
- Status: parent-driven (vision tools unavailable in cron).

## Test priority ordering

Tests 4 (mode 8), 5 (mode 9), 6 (mode 10), and 7 (mode 11) are the decisive new probes. Run them first to determine which branch the bug is on:
- All four modes produce expected output → bug is in payload/result merge (v19 stages investigate accumulate/ReSTIR/denoise passes)
- Mode 8 crashes → bug is in TraceRay setup
- Mode 8 produces NaN/garbage → bug is in TraceRay's interaction with payload structure
- Mode 9 = 0 → GBufferMaterial SRV binding is broken
- Mode 10 = 0 → GI cbuffer not bound/updated
- Mode 11 = 0 → View cbuffer not bound
- Modes 6/9 work but mode 7 fails → bug is in AmbientColor/AmbientScale uniforms
- Modes 6/7/8/9 all 0 → bug is in dispatch body / slangc dead-strip (mode 10 is the decisive probe)

Then run Test 3 (mode 0) and Test 9 (validator) to confirm the default path is unchanged.

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: PASS — no imports, no Python code.
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A — no test files.
- [x] No source-incomplete-relative-to-test: PASS — source patch is complete; no test code references incomplete source.
- [x] No missing test isolation fixture: N/A — no test files.
- [x] No AsyncMock on sync function (or vice versa): N/A — no Python tests.

## Honest scope

Tests 1-11 are all parent-driven because the cron's terminal is blocked by tirith. The only test that can be run in cron is Test 1 (file-only diff check), and even that requires shell access to invoke `diff`. The cron is structurally unable to verify the v18 patch's runtime effect without parent's interactive session.

This is consistent with all prior cycles' test staging (v1-v17). The patch is mechanically sound (verified by static inspection), the identifiers are correct (verified against the existing main-loop patterns at lines 502-533), and the parent-driven tests cover the runtime verification path comprehensively.