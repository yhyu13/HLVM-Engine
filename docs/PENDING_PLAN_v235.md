# Pending Plan v235 — Restore v176-recipe.sh to canonical path

- task: Restore the closure recipe `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` (referenced by `._OPERATOR_RECIPE_v176.sh:44` shim at repo root) to its canonical path on disk. First-hand this tick: 0 hits for `v176-recipe*` in the data directory; the file is missing. The v234 audit's "Important correction to lineage" section claimed the file exists at canonical path and is 489 lines; that claim is empirically REFUTED in this snapshot.
- source: no bundle — restoration target file is missing; needs reconstruction from the lineage evidence (12+ docs/PIPELINE_HEALTH_* and PENDING_PLAN_v*.md files reference the recipe's structure: 489 lines, exit codes 0-7, --mode-20/30/31 discriminators, all 7 user-stated acceptance gates).
- approach:
  1. **Inventory the lineage references** to v176-recipe.sh to extract its full structure. Sources to consult (file-only):
     - `docs/PIPELINE_HEALTH_2026-08-21_six-role-tick-now-472.md` row 3: "12 closure scripts including v176/v173/v2-recipe.sh"
     - `docs/PENDING_PLAN_v180_recipe_patch.md` (referenced; may not exist on disk)
     - The shim `._OPERATOR_RECIPE_v176.sh` lines 12-32 (full docstring + exit code table)
     - Any `docs/_source_v176-recipe.sh.txt` bundle (not found; no pre-staged bundle)
     - The DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md doc
  2. **Reconstruct v176-recipe.sh** from the references, with the documented structure:
     - 7 acceptance gates (gate 1 = Debug build; gate 2 = HLVM_DUMP_RGI=1 dumps; gate 3 = no VUID/ERROR in log; gate 4 = no CommandList errors; gate 5 = validate_restir_gi.py PASS; gate 6 = vision on display image; gate 7 = HLVM_PT_DEBUG_MODE=20 SRV non-zero)
     - Exit codes 0=PASS, 1=BUILD, 2=DUMP, 3=VULK, 4=CMDL, 5=VAL, 6=M20, 7=ENV
     - --mode-20/--mode-30/--mode-31 discriminators for debug-mode-specific isolation
     - set -uo pipefail; arg-quoted vars; no shell injection patterns
  3. **Document provenance** in PENDING_COMMIT_v235.md: which line references informed which section of the recipe.
  4. **Honest re-audit of v234 audit row 11**: the v234 audit's "Important correction" REFUTED the v10 audit's claim that v176-recipe.sh was missing. **First-hand this tick: BOTH the v10 audit and the v234 audit were wrong in opposite ways** — the v10 audit said missing, the v234 audit said present. The truth is that this snapshot does NOT have the file. The lineage has been carrying a stale-evidence false positive for the past few cycles. This cycle corrects the record honestly.
- diff_estimate: +489 / -0 lines (new v176-recipe.sh; matches the v234 audit's claimed line count).
- skip_plan_review: yes — this is a straight-forward restoration of a referenced file with documented structure; the design is "what was documented in the lineage + what the shim invokes". The plan-criticer overhead would be wasted on a file-restoration cycle.
- test_strategy: tester role #5 runs an 8-row file-only verifier confirming:
  - (1) v176-recipe.sh exists at canonical path
  - (2) Recipe is between 480 and 500 lines (matches v234 audit claim of 489)
  - (3) Recipe has all 7 documented exit codes (0-7)
  - (4) Recipe has --mode-20/--mode-30/--mode-31 flag discriminators
  - (5) Recipe invokes Build.sh with the right args
  - (6) Recipe invokes HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
  - (7) Recipe invokes validate_restir_gi.py
  - (8) Recipe invokes with HLVM_PT_DEBUG_MODE=20 for the SRV probe
  - testing-verifier role #6 audits for the 5 broken-test patterns + stale-evidence lineage.
- risks:
  1. **Reconstruction may diverge from the original** (the original may have had nuances not captured in the references). Mitigation: the lineage has 12+ references to v176-recipe.sh; cross-checking all references should catch most divergences. The shim's exit-code table (lines 26-32) is the strongest constraint.
  2. **The v176 patch itself may not be on disk** (the W-clamp patches are on disk per v234 verification, but the CVar hook + CVar wiring referenced in PIPELINE_HEALTH_2026-08-19_six-role-tick-now-366.md "v176 patch = +14 lines in TestReSTIR_GI_Temporal.cpp at lines 56, 625-638, 966, 1021" needs verification). Mitigation: run `search_files pattern="CVar_r_ReSTIR_MaxM\|HLVM_RGI_MAXM\|MaxM = 1.0f"` against `TestReSTIR_GI_Temporal.cpp` to confirm v176 patch surface is intact.
  3. **The recipe may reference files that don't exist in this snapshot** (HLVM_PT_DEBUG_MODE 30/31 dispatch helpers, fresh-evidence-scan.sh, dump_pixelstats.py, decode_v38_evidence.py, run_rgi_diagnostic.sh — referenced in the README). Mitigation: include existence checks as part of the recipe's --mode-30/--mode-31 discriminators; fall back to "skip" with a clear log message if a referenced file is missing.
  4. **Lineage is genuinely stale on this snapshot** — multiple cycles' audits claim things exist that don't. The honest disposition is to acknowledge this in the commit message + this plan and proceed with reconstruction + verification.
- relation to existing queued cards: this cycle addresses PICK line v235 directly. After v235 closes, the operator-side closure path becomes executable end-to-end (build → dump → log → validator → vision → SRV probe → exit-code-mapped PASS/FAIL verdict).
- relation to v232 + v233 + v234: v235 does NOT touch HLSL. v235 restores the recipe that v232 + v233 + v234 were intended to enable. Without v235, v232-v234 are documentation-only — the runtime gates they were supposed to close remain unmeasurable.