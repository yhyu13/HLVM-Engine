# Pending Plan v140
- task: v140 — Expose AmbientColor in FGIPassDesc so the test can override the hardcoded ambient color
- source: docs/DIAGNOSTIC_2026-08-01-v25.md (authoritative current-state) + docs/DIAGNOSTIC_2026-07-30.md (legacy v24) + Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h + Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp + Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- approach: (1) Add `float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` to `FGIPassDesc` in FGIPass.h, defaulting to the existing hardcoded value (backward-compatible for TestPathTraceGI). (2) Replace the hardcoded `const float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` at FGIPass.cpp:447 with `std::memcpy` from `Desc.AmbientColor` (no wire-format change; `FGIConstantsData.AmbientColor[4]` at line 35 is `float[4]`). (3) Set `Desc.AmbientColor = { 1.0f, 1.0f, 1.0f, 0.0f };` in TestReSTIR_GI_Temporal.cpp at line 441 to match the test author's documented intent. Net diff: ~6 lines added across 3 files. All three files are in the engine source tree, no test data dir changes required, no shader changes required (GIPathTracing.hlsl reads AmbientColor via cbuffer b0 already).
- diff_estimate: +6 / -1 lines (3 files)
- skip_plan_review: no (default for non-trivial change with intent match to diagnostic; though surgical, the patch is the file-only ceiling)
- test_strategy: file-only patch integrity verification via read_file + search_files (the tester role checks that all 3 sites changed and the FGIPassDesc struct field order is preserved — see PENDING_TESTS_v140.md)
- risks:
  1. **Backward compatibility for TestPathTraceGI**: it uses `GI::FGIPassDesc Desc{};` (default-init). Adding a field with the SAME default value as the existing hardcoded preserves byte-exact behavior. Risk: low.
  2. **Wire format alignment**: `FGIConstantsData.AmbientColor[4]` at FGIPass.cpp:35 is `float[4]` and is uploaded as a constant-buffer slice. `std::memcpy(Data.AmbientColor, Desc.AmbientColor, sizeof(Desc.AmbientColor))` preserves the layout. Risk: low.
  3. **Struct layout**: adding a field to `FGIPassDesc` does not change its size-relevant order if placed at the end (after `AmbientScale`). Risk: low.
  4. **Default value preservation**: the default `{ 0.6f, 0.6f, 0.65f, 0.0f }` MUST match the existing hardcoded value exactly (3 sig-figs). A typo would silently change TestPathTraceGI's behavior. Risk: low but verify by grep.
  5. **Single-profile self-check**: all 6 roles on this host run as the same head, so the plan-criticer / reviewer / testing-verifier verdicts are self-checks (per `Anti-pattern #7`). The patch is small enough that this is acceptable; we are explicitly in file-only mode.

## Files modified (expected)

| File | Change | Approx lines |
|------|--------|--------------|
| `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` | +1 line: add `float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` to `FGIPassDesc` struct (after `AmbientScale` member, line 56) | +1 |
| `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` | -1 / +1 lines: replace `const float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` (line 447) with no local; the `std::memcpy(Data.AmbientColor, ...)` at line 461 reads from `Desc.AmbientColor` instead | -1/+1 |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | +1 line: `Desc.AmbientColor = { 1.0f, 1.0f, 1.0f, 0.0f };` after line 441 (after `Desc.AmbientScale = 1.5f;`) | +1 |

Net: +3 / -1 lines across 3 files.

## Verification (parent runspace only)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# Expected: SUCCESS (no link errors; only FGIPass.h public-API additive change)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
# Expected: log line like "DumpRGBA32FTexture: gi_raw normalized per-channel — R[1.500,1.500] G[1.500,1.500] B[1.500,1.500]"
# (was R[1.000,1.000]... before v140; the change is 1.0/0.6 * 0.9 = 1.5)
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py --data-dir Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
# Expected: gi_raw per-channel mean ≈ 1.5, std still ≈ 0 (uniform color)
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expected: still fails variance check (uniform color) but per-pixel values match the new ambient math
HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
# Expected: non-zero per-pixel GBufferMaterial (already worked per v24 diagnostic; v140 doesn't affect this path)
```

## Acceptance for v140 itself (file-only verifiable)

1. FGIPass.h contains `float AmbientColor[4]` in `FGIPassDesc` struct (1 match)
2. FGIPass.cpp:447 NO LONGER contains `const float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` (0 matches)
3. FGIPass.cpp:461 reads from `Desc.AmbientColor` (1 match in std::memcpy call)
4. TestReSTIR_GI_Temporal.cpp:441-442 sets `Desc.AmbientColor = { 1.0f, 1.0f, 1.0f, 0.0f };` (1 match)
5. No other call sites broken (TestPathTraceGI still compiles because default `{ 0.6, 0.6, 0.65, 0.0 }` matches old hardcoded)
6. CMakeLists.txt UNCHANGED (no new source files; FGIPass.cpp / FGIPass.h / TestReSTIR_GI_Temporal.cpp already in their respective targets)

This is the minimum-scope, file-only patchable item from the diagnostic. Verification of the actual rendered image requires terminal+vision+numpy in a parent runspace — out of scope for this cycle.