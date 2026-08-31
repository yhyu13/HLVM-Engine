# Pending Plan v218

- task: Pre-build compile-risk + dual-copy audit of cycles **v200-v217** — the 18 cycles that landed
  AFTER the lineage's only compile-risk audit (v200, which covered v183-v199).
- source: no bundle — direct source derivation
- approach: v200 de-risked the operator's first build for v183-v199 and closed at ALL_KEEP. Eighteen
  cycles have landed since, including exactly the change shapes v200 was built to catch: a **cbuffer
  field** (v204/v205 `GuideScale`), a **header/struct edit** (v206, v210, v213), a **signature/arity
  change** (v209 member deletion), a **lifecycle move** (v214), and a **shader edit in a file with three
  copies** (v204, v211). None of these has been compile-risk audited, and two of the classes
  (cbuffer layout, dual-copy divergence) are the two this lineage has proven are **silent** — they
  compile clean and render wrong. Audit the v200 checks against the v200-v217 delta.
- diff_estimate: +0 / -0 source lines expected (audit); any finding gets carded, not bundled.
- skip_plan_review: no
- test_strategy: role #5 re-derives every load-bearing row independently, each zero paired with a
  same-shape same-scope positive control (v217 rule).
- risks:
  - The v217 rule is mandatory: `search_files` returns false zeros (timeout, `file_glob`) AND false
    partial results on wide walks. Every query must be scoped at `Engine/Source/Runtime` or a single
    file, never the project root, and every zero needs a positive control.
  - Line numbers are NOT invariant across 39 cycles (v217 finding). Cite symbols, verify counts.
  - The audit must not become an excuse to patch. If a finding appears, card it — a patch inside an
    audit cycle makes the audit's own "zero source modified" row unverifiable (v196/v208 precedent).

## Checks to run (derived from v200's four, extended by the v200-v217 delta)

1. **Arity/symbol coherence** for v209's deletion and v214's move — deleted symbols at 0, live
   siblings unchanged (the control), moved symbols present exactly once at the new site.
2. **Cbuffer layout, four-way** for the ReSTIR structs v210 touched: C++ struct, the field-by-field
   marshaller, and BOTH HLSL copies. v184's rule (never an array in the tail) must still hold.
3. **The `GuideScale` slot introduced by v204/v205** — same four-way check, across **three** shader
   copies rather than two. This is the field no audit has ever seen; v200 predates it.
4. **Dual/tri-copy divergence** for `BilateralDenoise_cs.hlsl`, which exists in three directories.
   v182 established that editing one copy and not the one ShaderMake actually compiles is a real,
   already-committed failure mode in this repo.
