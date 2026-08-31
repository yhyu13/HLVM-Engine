# Pending Tests v8
- task: comment-only documentation drift cleanup — no new tests needed
- plan: docs/PENDING_PLAN_v8.md
- commit: docs/PENDING_COMMIT_v8.md
- timestamp: 2026-07-27T07:26:30Z (estimated cron tick wall clock)
- tester: tester (single-head autonomous cron — software-development-practices §"Full auto" anti-pattern #7 caveat applies)

## Tests produced
None. The v8 patch is comment-only (lines 1685-1693 of TestReSTIR_GI_Temporal.cpp, +6/-5 lines net +1). It does not change:
- The DumpRGBA32FTexture function logic
- The HLVM_LOG output content (the format string at line 1694 is unchanged)
- The validator's 3 structural checks (mean luma, spatial std, cell-variance)
- The v5/v6/v7 acceptance criteria

## What validates the patch then
The patch is verifiable mechanically via the patch tool diff (already done) and via read_file at offset 1680-1705 (done above). Both confirm:
1. The stale "v4b candidate fix" reference to the HLVM-bypass is gone.
2. The replacement points at v3 ENTER/EXIT/binding-set logs and v5 NOTE near line 1521, both of which exist in the same file.
3. No code, binding, or shader change.

## Test surface unchanged
- `validate_restir_gi.py` — unchanged, still 3 checks.
- `HLVM_DUMP_RGI=1` dump schedule — unchanged.
- `HLVM_RGI_ACCUM=8` accumulation — unchanged.
- v5/v6/v7 acceptance criteria — unchanged.

## Cron's terminal status (unchanged)
Tirith continues to block every terminal probe. The cron's mechanically actionable work is restricted to file-based patches. The parent-driven verification (build + run + log capture + validator + vision-check) is still required for the v5 code patch to be confirmed working. v8's documentation cleanup is independent of that verification.

## Honest caveat
If parent runs the test after this patch lands, the renderer output will be IDENTICAL to what they would have seen with v5/v6/v7 alone (the comment is non-executing text). v8 is documentation drift cleanup; it does not advance v6 audit SOME_RELAX. The pipeline still requires parent verification of v5's actual code patch.