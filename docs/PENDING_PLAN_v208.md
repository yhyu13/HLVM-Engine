# Pending Plan v208

- task: Pre-build compile-risk audit of the **v201-v207 delta** — the cycles that
  landed after v200's audit and have never been audited by anything.
- source: no bundle — direct source audit
- skip_plan_review: no
- diff_estimate: +0 / -0 source lines (audit only)

## approach

v200 (tick-546) performed the lineage's only pre-build compile-risk audit, and it
covered **v183-v199**. Seven cycles have landed since — v201 (no source change),
v202, v203, v204, v205, v206, v207 — and **no cycle has audited that delta.** The
audit that was supposed to de-risk the operator's first build is now itself seven
cycles stale, and card L / M / N are all precondition-gated on exactly that build.

This is the same scope error v202 caught in v200/v201 and v204 caught in v203:
an audit is performed, a rule is adopted, and the *next* cycles are not swept by
it. Here the un-swept thing is the audit itself.

**Why this and not a thirteenth defect.** v199 and v201 both concluded the extent
seam is swept; v207 found its twelfth instance only by enumerating a register the
card was not about. Hunting a thirteenth is `§Anti-patterns §6` drift. The
binding constraint on the entire queue is the unbuilt chain, and the delta
v200 never saw is the part of it nobody has checked.

## Audit domain — the two failure classes the lineage has demonstrated

(a) **Arity/signature breakage** — loud, fails at compile.
(b) **C++/HLSL constant-buffer layout desync** — SILENT: compiles clean, renders
    wrong. This is the v183/v184 class and the one that matters, because a build
    that succeeds does not clear it.

v204 added a cbuffer **field** (`GuideScale`) and v206/v207/v205 wrote contracts
into three shared headers. Class (b) is therefore live in this delta in a way it
was not in v200's.

## Per-cycle risk surface to check

| Cycle | Change | Class | Check |
|-------|--------|-------|-------|
| v202 | `FReSTIRPass` t4 ternary | binding | layout↔consumer agreement |
| v203 | `FReSTIRPass` comment + near-miss restore | **deletion** | `SpatialLayout` item set intact |
| v204 | `FBilateralDenoise` **new cbuffer field** | **(b) silent** | 4-way: C++ array index, buffer byteSize, both HLSL copies |
| v205 | `GuideScale` source + header contract | logic | mandatory operand |
| v206 | `FReBLURPass.h` comment only | none | byte-check functional lines |
| v207 | `FGIPass` u2 ternary + header | binding | fallback operand non-null |

## test_strategy

Every negative paired with a same-shape positive control (v205 row). Every
absence closed by reading the scope, not by a symbol count (v198/v202 rows).
Cbuffer claims must be checked in **all four** expressions per v200's rule
(C++ struct, marshaller, both HLSL copies) — not two.

## risks

- The lineage's own query tooling has produced false zeros in both polarities
  (tick-526 false pass, v192 false failure). Any "clean" verdict here is cheap to
  fabricate accidentally. **Determine the actual mechanism before trusting any
  zero in this cycle** — no prior tick has done that, they have only catalogued
  instances and written avoidance rules.
- v203's near-miss (a `patch` whose `old_string` anchored on a comment and
  deleted three live binding items) landed inside this delta and was caught by
  the impler reading its own returned diff. If it had not been, the damage is in
  the tree right now. Re-verify that item set independently.
