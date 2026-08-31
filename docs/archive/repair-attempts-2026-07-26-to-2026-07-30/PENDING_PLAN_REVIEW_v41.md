# Pending Plan Review v41 — fix FImageDump::DumpToPNG to preserve source alpha channel

## Verdict: KEEP

## Design soundness
- The plan correctly identifies the root cause: `FImageDump::DumpToPNG` at `Engine/Source/Runtime/Private/Image/FImageDump.cpp:19` hardcodes `pixels[idx + 3] = 255;` regardless of the source `rgbaData[i*4+3]` value.
- The v28 alpha sentinel at `GIPathTracing.hlsl:694` writes `Output[pixel].w = max(Output[pixel].w, 0.99994f)`, intending to produce ~254/255 alpha in the PNG dump.
- The hardcoded alpha-255 in the encoder means the v28 sentinel is dropped on the way to disk. v37's `check_alpha_sentinel` and v40's `classify_alpha_sentinel` both inspect the PNG's alpha channel — which is always 255 — and report "saturated PASS" on every frame of every run, including pre-v28 binaries.
- The fix is to change line 19 from `pixels[idx + 3] = 255;` to `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i*4 + 3] * 255.0f, 0.0f, 255.0f));` — same pattern as the R/G/B lines above.
- The patch is mechanical, fully reversible, and scoped to a single file. No caller signature changes. All 13+ call sites get the fix transitively.

## Plan completeness
- The plan enumerates:
  - 1 source file modified (`FImageDump.cpp`); 1 source file unchanged (`FImageDump.h` — signature preserved).
  - 2 post-rebuild evidence shapes (alpha=saturated + rest correct = PIPELINE_GOAL_DONE; alpha=low/zero/mixed = bug elsewhere).
  - All 13+ call sites of `DumpToPNG` listed (TestRTReflections, TestCornellBoxGI, TestPathTraceGI, TestRTShadowsGBuffer, TestReSTIR_GI_Temporal, FRenderPassDumper).
  - Behavior change scope: every PNG dump in the project gets a different alpha.
  - NaN/inf fallback: std::clamp at the byte cast gives deterministic behavior.
- The plan does NOT cover:
  - What happens if a caller passes a `nullptr` for `rgbaData` (existing pre-v41 behavior also crashes — not a regression).
  - What happens if `rgbaData[3]` is negative (existing std::clamp produces 0; pre-v41 it produced 255). The pre-v41 behavior was actually safer for invalid data but useless for valid data. The post-v41 behavior is correct for valid data and slightly worse for invalid data — acceptable trade-off.

## Feedback for planner (FIX only)
- (none — plan is well-scoped; the patch shape is mechanical and the rationale is grounded in actual code inspection)

## Risks acknowledged
- **Single-head host caveat**: same model writes all 6 roles. Verdicts are self-checks. Patch is mechanical so verdicts are reproducible.
- **Behavior change**: every PNG dump's alpha now reflects the source buffer. Mitigation: API signature unchanged; RGB-only consumers unaffected; alpha consumers (none in source tree today) gain a real signal.
- **NaN handling**: source alpha NaN → clamp gives 0 (per C++ std::clamp semantics for NaN). Documented in plan.
- **Cross-test impact**: 5 test files + 1 utility call `DumpToPNG`. All 6 use it for diagnostic visualization only. None of them consume the alpha channel today. The change is therefore low-impact for current callers but high-impact for the v28 sentinel's diagnostic value.
- **FRenderPassDumper.cpp**: gets the fix transitively. Same data-flow shape.
- **FImageDump::DumpTestPattern**: not modified (hardcodes alpha=255 for the test pattern; that pattern's purpose is to verify stb_image_write works, not to test alpha).

## Verdict: KEEP

## Architectural note
This v41 is structurally identical to the v22 binding-layout-split in spirit: both fix a boundary bug (nvrhi-deferred-barrier-ordering at the SRV/UAV boundary; FImageDump at the GPU-output-to-PNG boundary) that no amount of post-hoc dump inspection can work around. v22 was the binding-layout boundary fix; v41 is the dump-encoder boundary fix. Both are required to make the diagnostic surface reliable.

The cron's "renderer is BROKEN until parent rebuilds" gate (per v40 audit) was technically correct about the binary not being rebuilt, but missed that the diagnostic surface itself was broken at the encoder layer. v41 closes the encoder-layer gap.