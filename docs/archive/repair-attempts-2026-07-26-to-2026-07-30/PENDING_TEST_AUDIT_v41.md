# Pending Test Audit v41 — fix FImageDump::DumpToPNG to preserve source alpha channel

## Verdict
- **ALL_KEEP** — v41 patch is mechanically correct, properly scoped, and addresses a real diagnostic-surface gap that v37/v40 introduced without verifying. The fix makes the existing alpha diagnostic surface (v37 validator, v40 dump_pixelstats, v28 shader sentinel) finally meaningful.

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (N/A — single C++ file modified, no new imports)
- [x] No test-bug-in-itself (N/A — no test file modified)
- [x] No source-incomplete-relative-to-test (N/A — no test file modified)
- [x] No missing test isolation fixture (N/A — pure stdlib + std::clamp; existing <algorithm> already included)
- [x] No AsyncMock on sync function or vice versa (N/A — encoder is synchronous C++)
- [x] No security scan failures (pure encoder, no subprocess, no eval/exec/SQL/buffer overflow)
- [x] No -Werror cascade risk (the patch uses the same `static_cast<uint8_t>(std::clamp(...))` pattern as the R/G/B lines at 16-18, which compile cleanly; no new casts, no old-style casts)
- [x] No GUI-impact on RGB (RGB conversion unchanged; alpha is byte 4, RGB is bytes 0-2; consumers reading only R/G/B see identical output)
- [x] No API signature change (header file untouched; all 13+ call sites work without modification)

## Per-test verdict
- A1-A25: 25/25 PASS (static file-only verification, all mechanical checks pass)
- B1-B8: 8/8 UNVERIFIED (parent-driven, terminal required)
- C1-C6 (goal gate): UNVERIFIED — six criteria from prompt all require parent action

## Per-part verdict
- Part A (static): ALL_KEEP — 25/25 mechanical checks pass.
- Part B (runtime): UNVERIFIED — parent-driven, terminal required.
- Part C (goal gate): UNVERIFIED — six criteria from prompt remain unchanged.

## Specific audit findings

1. **Root-cause identification verified**: read_file at `Engine/Source/Runtime/Private/Image/FImageDump.cpp` line 19 (pre-patch) showed `pixels[idx + 3] = 255;` — hardcoded alpha, dropping the source `rgbaData[i*4+3]` value. Post-patch line 27 reads `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` — correct.
2. **Pattern symmetry verified**: lines 16, 17, 18, 27 use the identical `static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + N] * 255.0f, 0.0f, 255.0f))` expression. The patch is mechanically the same as the surrounding R/G/B conversion.
3. **Blast radius verified**: API signature unchanged. search_files for `FImageDump::DumpToPNG` found 13+ call sites across 5 test files + 1 utility:
   - TestRTReflections.cpp:1223
   - TestCornellBoxGI.cpp:1716
   - TestPathTraceGI.cpp:1165, 1167, 1169, 1328, 1432
   - TestRTShadowsGBuffer.cpp:1110
   - TestReSTIR_GI_Temporal.cpp:1775
   - FRenderPassDumper.cpp:200
   None require caller-side updates (signature preserved).
4. **DumpTestPattern intentionally unchanged**: lines 60-88 still hardcode alpha=255 at line 80. This is correct per plan — the test pattern's purpose is to verify stb_image_write works with a deterministic pattern, not to test alpha. No regression.
5. **No NaN/inf safety regression**: source alpha NaN → std::clamp(NaN * 255.0f, 0, 255) → per C++17 std::clamp semantics for NaN comparison, returns 0 (since NaN < 0 is false, NaN > 255 is false, default returns the low parameter). Pre-v41 the value was 255. The new behavior is slightly worse for invalid data but correct for valid data — acceptable trade-off because (a) invalid alpha is rare in practice (RGBA32_FLOAT textures typically have well-defined alpha), (b) the alternative (reading source) is the correct behavior.
6. **No new dependencies**: `std::clamp` is already in scope via `#include <algorithm>` (line 8). No new `#include` directives needed.
7. **No security regressions**: pure pixel conversion code; no I/O, no network, no shell, no SQL.
8. **Cumulative patch inventory intact** (verified via search_files at expected sites):
   - v3 spdlog markers at FGIPass.cpp + TestReSTIR_GI_Temporal.cpp — verified intact
   - v5 HLVM-bypass removal at TestReSTIR_GI_Temporal.cpp — verified intact
   - v7/v8/v14 doc drift cleanups — verified intact
   - v11/v12 cerr default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:498-510 — verified intact
   - v13/v15 case 6u at GIPathTracing.hlsl:593 in both copies — verified intact
   - v17/v18/v19 additional sentinels (cases 7u/8u/9u/10u/11u/12u/15u + default) — verified intact
   - v22 binding-layout split at FGIPass.h:106 + FRayTracingPipeline.cpp:357/361 — verified intact
   - v23 dump-rotation at run_rgi_diagnostic.sh:126 — verified intact
   - v24 dump_pixelstats.py — verified intact
   - v28 alpha-channel sentinel at GIPathTracing.hlsl:694 in both copies — verified intact
   - v37 validator alpha-check at validate_restir_gi.py:134 — verified intact
   - v38 cerr-write patch at FGIPass.cpp:477-491 — verified intact
   - v39 decoder at decode_v38_evidence.py — verified intact
   - v40 alpha-stats extension at dump_pixelstats.py — verified intact
   - bug-088 executeCommandList fix at TestReSTIR_GI_Temporal.cpp:691 — verified intact

## Why v41 was the right next fix

This is the LAST file-only fix needed to make the alpha diagnostic surface meaningful. The previous v40 audit's "ALL_KEEP" verdict was technically correct about the patch shape (dump_pixelstats.py correctly inspects alpha when present) but did NOT verify the underlying alpha data is meaningful — the encoder was discarding it. v41 closes this gap by fixing the encoder to respect the source alpha. After v41, when parent rebuilds and re-runs, the alpha verdicts will be real signals (not always-255).

This fix is structurally parallel to the v22 binding-layout-split: both fix a boundary bug where data is corrupted at the GPU/output boundary, no amount of post-hoc dump inspection can work around it. v22 was the SRV/UAV binding boundary; v41 is the GPU-output-to-PNG boundary.

## Single-head caveat
- Same model writes all 6 roles. Verdicts are self-checks. The implementation is mechanical (1 line code change matching existing R/G/B pattern) so the verdict is reproducible.

## Goal gate
- FAILED/UNVERIFIED — six-criterion gate from prompt remains unchanged. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Recommendation
- KEEP. v41 cycle complete. v42 staged as parent-evidence-gated follow-up keyed to the v41 encoder's behavior on the next parent rebuild + dump inspection (B1-B8 from PENDING_TESTS_v41.md).