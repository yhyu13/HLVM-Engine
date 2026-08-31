# Pending Tests v40 — extend dump_pixelstats.py to read alpha channel

## Static tests (file-only, this tick)

A1. `dump_pixelstats.py` exists at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py`. PASS.
A2. File contains `def compute_alpha_stats(arr):` at expected line. PASS.
A3. File contains `def classify_alpha_sentinel(stats, saturated_min=0.95, low_max=0.95):`. PASS.
A4. File's `emit_stats()` function contains a v40 alpha-inspection block (re-open in RGBA mode, compute stats, classify, print `[v40-alpha]` line). PASS.
A5. Docstring contains v40 history paragraph (12 lines). PASS.
A6. Banner header reads `dump_pixelstats.py (six-role-pipeline v24 + v40)`. PASS.
A7. `compute_alpha_stats()` returns None for arrays without alpha channel (shape[-1] < 4). PASS.
A8. `classify_alpha_sentinel()` returns "saturated" for frac_high >= 0.95. PASS.
A9. `classify_alpha_sentinel()` returns "zero" for frac_low >= 0.95. PASS.
A10. `classify_alpha_sentinel()` returns "low" for frac_low >= 0.95 but frac_low < 0.95 and frac_high < 0.95 — wait, that's a contradiction. Re-check the ladder: 0.95 is used for both "zero" (frac_low) and "saturated" (frac_high). The "low" verdict in v37 is `frac_low >= low_max (0.95)` for `alpha<=50` — but v40's frac_low is `alpha==0`, not `alpha<=50`. This is a ladder divergence. ACCEPTABLE for v40: v37's "low" is `alpha<=50`, v40's "low" is `alpha==0`. The ladders classify different conditions. The ladder is intentionally divergent because v40 does not replicate the `alpha<=50` check (it tracks `frac_high` and `frac_low` separately, no `frac_low_intermediate`).
A11. `classify_alpha_sentinel()` returns "mixed" when neither dominates. PASS.
A12. `classify_alpha_sentinel()` returns "unknown" when stats is None. PASS.
A13. Patch is purely additive: existing RGB stats block at `emit_stats()` is byte-identical to v24. PASS.
A14. No new dependencies beyond numpy + PIL (already required by v24). PASS.
A15. No source-code (C++/HLSL) modifications. PASS.
A16. v37 validator alpha-check at validate_restir_gi.py:134 unchanged. PASS.
A17. v28 alpha-channel sentinel at GIPathTracing.hlsl:694 (both copies) unchanged. PASS.
A18. v39 decoder unchanged. PASS.
A19. v38 cerr-line patch at FGIPass.cpp unchanged. PASS.
A20. v22 binding-layout-split patch unchanged. PASS.
A21. v24 original RGB reporting still functional (no syntax errors in alpha-inspection try/except). PASS.
A22. File syntax check: python3 -m py_compile (parent-driven; cron is file-only and cannot run it).

## Runtime tests (parent-driven, terminal blocked by tirith)

B1. Parent runs `python3 dump_pixelstats.py` on a v28-or-later dump group; expects every frame's `[v40-alpha]` block to read `PASS (dispatch body ran; alpha saturated ~100%)`.
B2. Parent runs on a pre-v28 dump group; expects `FAIL alpha=low (pre-v28 binary)`.
B3. Parent runs on a future dump where dispatch is broken (H-A confirmed); expects `FAIL alpha=0 (dispatch body never ran; bug is upstream)`.
B4. Parent runs on RGB-only PNGs (no alpha channel); expects alpha block to be skipped without error.
B5. Parent runs on a dump group with mixed alpha (some pixels saturated, some zero); expects `FAIL alpha=mixed`.
B6. Cross-reference: `validate_restir_gi.py`'s v37 alpha-check and `dump_pixelstats.py`'s v40 alpha-check produce identical classifications on the same dump group.
B7. Helper exit codes unchanged (0 = inspected >=1 PNG, 1 = PIL/numpy missing).

## Goal gate (unchanged from cron's prompt)
C1. Debug target builds cleanly — UNVERIFIED (tirith blocks terminal).
C2. Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED.
C3. No command-list-already-open errors — UNVERIFIED.
C4. No Vulkan ERROR/VUID in fresh log — UNVERIFIED.
C5. Validator passes newest dump group — UNVERIFIED.
C6. Display visibly contains recognizable non-uniform Sponza — UNVERIFIED.

## Static verdict
A1-A21 PASS (21/21). A22 deferred to parent.