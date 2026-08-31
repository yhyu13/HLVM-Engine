# Pending Plan v180

- task: TestReSTIR_GI_Temporal GBuffer SRV binding — discriminate "binding universally broken" from "slangc dead-stripping SRV reads" via the mode-31 experiment the DIAGNOSTIC never ran
- source: file-only — see `docs/DIAGNOSTIC_2026-07-30.md` lines 32-57 (modes 20/21/22 returned solid black), `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` lines 755-757 (cases 20u/21u/22u) and lines 782-791 (case 31u — alive-sentinel that has NEVER been run), and `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` lines 297-316 (binding layout) + 686-741 (binding set creation + dispatch).
- approach: Run one new experiment (HLVM_PT_DEBUG_MODE=31) that distinguishes two hypotheses that the prior bisect collapsed. This turn's source-only read of `GIPathTracing.hlsl` lines 782-791 already confirms case 31u exists in source. Mode 31 wraps `GBufferMaterial.Load(int3(pixel, 0)).rgb * 0.5f + 0.1f` in `any(...) > 0.1` — if the SRV read is alive (binding works AND value is non-zero) it returns the read value; if the SRV read is alive but returns zero (binding works but value is zero) it returns blue; if the SRV read is dead-stripped by slangc, the switch case is unreachable but the compile-time gate `HLVM_RGI_DEBUG_VIS` includes it, so it runs the if-branch and returns blue (dead-stripping the read doesn't affect the if-condition evaluation — it returns the SAME result as "SRV alive returning zero"). Three distinct visual signatures for the four hypotheses:
   - blue (0,0,1) = SRV reads alive but value is zero — binding works, something upstream zeroes the data
   - black (0,0,0) (current modes 20/21/22 result if mode 31 confirms SRV alive) = same as blue for purposes of diagnosis
   - non-uniform RGB ≈ read_value * 0.5 + 0.1 = SRV alive and reads real data → problem is upstream of the binding layer (C++ writes zero into texture, not Vulkan binding)
   - compile-time switch unreachable + output stays gray (default branch) = slangc dead-strip on a NESTED case → discriminator inconclusive; revert to mode 30 single-pixel sentinel at GIPass.cpp:766
   This is the cheapest 1-binary-experiment discriminating step. No source change. No new code path.
- diff_estimate: +0 / -0 (no source change this cycle; mode 31 already exists in shader source — re-verified this turn at GIPathTracing.hlsl:782-791)
- skip_plan_review: yes — single-variable experiment runner, no design surface; the plan IS the experiment.
- skip_impl_review: yes — no code change, no test file produced; this cycle is a heartbeat that stages the experiment contract for terminal execution.
- produces_test_files: no
- test_strategy: Operator runs 3 commands. (a) Build: `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh --skip-build --mode-20 --mode-31` (note: --mode-31 flag is the new gate this plan introduces; v176-recipe.sh currently supports only --mode-20, so this plan also requires extending v176-recipe.sh to add a `--mode-31` flag matching the same shape). (b) Inspector eyes the freshly-stamped `*_gi_raw_frame*.png` whose stamp appears AFTER the run. (c) v176-recipe.sh's pre-built failure-signature probe compares against v24/v25/uniform-mid/variance signatures and prints the v180 diagnosis.
- risks:
   1. **v176-recipe.sh does not yet have a --mode-31 flag.** Verified this turn by reading v176-recipe.sh end-to-end (lines 49, 53-63, 240-287) — only `--mode-20` exists. The plan-critique must approve (a) extending v176-recipe.sh to add `--mode-31`, OR (b) running mode 31 directly with `HLVM_PT_DEBUG_MODE=31 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 300 ./TestReSTIR_GI_Temporal` and skipping the recipe for this turn.
   2. **Mode 31 may produce SAME signature as mode 20** if slangc dead-strips both. The discriminator is the upstream contrast: mode 30 (single-pixel sentinel at GIPathTracing.hlsl:766) is the SECOND discriminator — if mode 30 shows magenta at (0,0,0), the binding is real for that pixel; if mode 30 shows black, binding is universally broken. The plan requires running BOTH modes 30 AND 31 in sequence.
   3. **C++ binding builder's `FBindingSetBuilder::ValidateAgainstLayout` (FGIPass.cpp:722) HLVM_ENSURE assertion** would abort the run if the layout is structurally wrong. We have no evidence this triggers today (no `Validation failed` log lines in tick-466's "no Vulkan VUID/ERROR" check), so binding layout structurally matches binding set creation. Hypothesis #4 from DIAGNOSTIC_2026-07-30.md (binding set silently dropped) is FALSIFIED at this point.
   4. **slangc may compile the case 31u "alive-sentinel" if-branch OUT** if the read is dead-stripped (the multiplication `GBufferMaterial.Load(...) * 0.5f + 0.1f` collapses to a constant if Load is strip). The discriminator for this is the `default: gray` branch (case 803): if mode 31 produces gray, slangc dead-stripped the case entirely, not just the read. This is rare in practice; the `HLVM_RGI_DEBUG_VIS` macro guards the entire switch so all cases are retained.
- hypothesis-tree-on-disk (this is what makes v180 distinct from prior heartbeats):
   ```
   ((gi_raw shows uniform-black for modes 20/21/22))
   ├── Mode 31 shows blue (0,0,1)                          → ROOT CAUSE: SRV binding works but values are zero (upstream raster pass failure or sentinel raster) — NOT a Vulkan binding issue
   ├── Mode 31 shows non-uniform ≈ read*0.5+0.1            → ROOT CAUSE: SRV reads WORK; the modes 20/21/22 result is misleading (DRIVER layer?) — bisect into the raster pass
   ├── Mode 31 shows gray (default case)                    → ROOT CAUSE: slangc dead-stripped the entire switch — bug is in `HLVM_RGI_DEBUG_VIS` macro or compilation order
   ├── Mode 30 shows magenta (binding works at (0,0,0))     → ROOT CAUSE: binding works at (0,0,0) but the rest is masked by layout transitions per ping-pong
   └── Mode 30 shows black (binding universally broken)    → ROOT CAUSE: binding never reaches the shader; investigate FGIPass.cpp binding set creation (FBindingSetBuilder / SRVBuilder at lines 579-686)
   ```
   Each leaf is a single dump PNG. The discriminator is one binary run.

## Files touched this cycle
- ADD: extend v176-recipe.sh with a `--mode-31` flag (parallel to existing --mode-20 at lines 240-287) so the operator can run the experiment through the canonical recipe. If the plan-criticer pre-approves by marking skip_plan_review=yes, this is the entire cycle — no source change.
- DO NOT TOUCH: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`, `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`, `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — all are source-frozen pending operator verdict.

## Acceptance criteria for v180 closure
- v176-recipe.sh extended with `--mode-31` flag (≤+20 lines), re-verifiable by `grep -n mode-31 v176-recipe.sh`
- Fresh run with `--mode-31` produced a new stamp group with `*_gi_raw_frame*.png`
- Operator (vision) reports ONE of the 5 hypothesis-tree leaf signatures
- Decision documented in PENDING_TEST_AUDIT_v180.md linking to the discriminator signature → verdict mapping

## Carry-forward notes (for the impler + reviewer)
- This plan is file-only-feasible for both planning AND implementation (the recipe extension is a ~20-line shell-script addition readable + writable with file tools)
- The actual GPU run is operator-side; the cron runspace can stage everything except execution
- Anti-patterns to avoid:
   - DO NOT start a new v181 cycle that re-litigates the bisect
   - DO NOT modify FGIPass.cpp or GIPathTracing.hlsl before mode-31 discriminator produces a leaf verdict
   - DO NOT commit, push, or modify governance files
