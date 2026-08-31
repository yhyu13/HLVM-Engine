# Pending Commit v41 — fix FImageDump::DumpToPNG to preserve source alpha channel

## Plan
- docs/PENDING_PLAN_v41.md

## Files
- Engine/Source/Runtime/Private/Image/FImageDump.cpp (modified)

## Source
- No source bundle; the patch modifies an existing file in the same project tree.

## Target
- working tree (no commit, no push — per cron's "do not commit/push/rewrite history" rule)

## Task
Fix `FImageDump::DumpToPNG` to write the source `rgbaData[i*4+3]` alpha channel value instead of hardcoding 255. This restores the v28 alpha-channel sentinel's diagnostic value: v37's `validate_restir_gi.py::check_alpha_sentinel` and v40's `dump_pixelstats.py::classify_alpha_sentinel` were both inspecting PNG alpha values that the encoder had set to 255 unconditionally, making every "PASS" verdict meaningless.

## Verify
- Static: read_file the modified file at lines 11-27; expect line 19 to be `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i*4 + 3] * 255.0f, 0.0f, 255.0f));`
- Static: search_files for "idx + 3] = 255" → expect 0 hits in FImageDump.cpp (DumpToPNG line)
- Static: search_files for "pixels[idx + 3] =" → expect 1 hit (DumpToPNG, post-patch) + 1 hit (DumpTestPattern, unchanged at line 72)
- Runtime (parent-driven): parent rebuilds, runs HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8, expects dumped PNGs to have alpha reflecting source buffer; v37/v40 alpha verdicts become meaningful for the first time.

## skip_impl_review: no
- Behavior change to a shared encoder used by 13+ call sites. Full audit trail invoked.

## produces_test_files: no
- Modifying an existing encoder, not creating a test file. HARD INVARIANT #2 does NOT fire.

## Diff estimate
- +1 line / -1 line (replace the hardcoded `255` with a std::clamp expression matching the R/G/B pattern at lines 16-18)

## Notes
- API signature unchanged. No caller updates needed.
- The patch is the same pattern as the existing R/G/B conversion at lines 16-18 — symmetric with surrounding code.
- After this patch, every PNG dump in the project (including non-GI tests like TestCornellBoxGI, TestRTReflections, TestRTShadowsGBuffer, TestPathTraceGI) gets a meaningful alpha value instead of the hardcoded 255. None of those tests consume the alpha channel today, so their visual output is unchanged.
- The v28 alpha sentinel at GIPathTracing.hlsl:694 (both HLSL copies) was already in source; v41 makes its diagnostic value observable at the PNG layer.

## Plan Deviations
None — patch matches plan exactly.