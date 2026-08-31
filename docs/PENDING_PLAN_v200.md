# Pending Plan v200

- task: Pre-build compile-risk audit of the unbuilt v183-v199 chain
- source: no bundle — direct source audit
- approach: Seventeen cycles (v183-v199) have been written without a single
  compile, and `PIPELINE_HEALTH_2026-08-30_six-role-tick-545.md` names the
  operator's first build as the point where "errors from any of seventeen
  unbuilt cycles" surface. No cycle has ever audited that chain for
  build-breaking or silently-wrong-at-runtime defects. This cycle does that,
  file-only, and produces a ranked risk list the operator can use. The audit
  targets the two failure classes the lineage has already demonstrated:
  (a) arity/signature breakage (v197 changed a function's arity), and
  (b) C++/HLSL constant-buffer layout desync (the v183/v184 class, which is
  SILENT — it compiles and renders wrong).
- diff_estimate: +0 / -0 source lines (audit only)
- skip_plan_review: no
- test_strategy: tester re-derives each load-bearing claim with an independent
  query, each zero controlled by a same-shape positive
- risks: the false-zero mechanisms on record (tick-526 alternation, v199
  path-at-a-directory in count mode) make a "clean" verdict cheap to fabricate
  accidentally. Every negative in this cycle must be paired with a positive
  control of the same query shape.

## Why this and not card L

Card L's remaining half is explicitly precondition-gated: *do not action while
the v183-v199 chain is unbuilt.* That precondition is now the binding
constraint on the entire queue. The highest-value file-only work is therefore
not to find an eighteenth instance of the extent class — v199 showed that seam
is swept and yielding nothing — but to **de-risk the build that unblocks
everything**, which no cycle has done.
