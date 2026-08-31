# Pending Plan Review v225 (iteration 2)

- plan: docs/PENDING_PLAN_v225.md (iteration 2)
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-575)
- timestamp: 2026-08-21

## Disposition of the three FIX items

1. **Wrong cause** → fixed. The `output_mode` diagnosis is adopted and the bracket-free control (rows 3/4) that falsifies the original theory is carried in the plan body, not buried.
2. **Wrong remedy** → fixed. The remedy is now an output-mode rule, and it correctly identifies itself as v198's *existing* rule rather than a new one.
3. **Unestablished VUID bound** → fixed, and improved beyond what I asked. The plan now says the directory-scoped citation is sound *because the five hit-bearing logs appear in the returned enumeration*, and separately re-derives the load-bearing half file-scoped (`VUID` at `TestReSTIR_GI_Temporal.log` → 0). That is the correct epistemic shape: a directory-scoped `count` is usable when you can see the relevant files were enumerated, and unusable as a bare zero.

## Design soundness

The corrected finding is sound and I re-derived rows 8/9 independently before endorsing (`tenth instance, in the known-good control`: DIR+count → 0, DIR+files_only → PENDING_PICK.md present). Three independent pattern shapes now exhibit the split, two of them bracket-free. The mechanism claim — that PENDING_PICK.md is *absent from the enumeration* rather than *reported as zero* — is the right characterisation and is directly visible in the returned maps.

**The strongest thing in this plan is the observation that v198's rule already existed.** The lineage did not lack the protocol. It wrote the protocol, attested to it in the broken-pattern audit of cycle after cycle, and then exempted the single query that gates whether any work happens. A rule that is checked everywhere except at the decision point is worse than no rule, because it produces the audit trail of diligence without the effect.

## Plan completeness

- Scope, verification strategy, and risk list are adequate.
- The dead end is recorded rather than silently dropped, and the plan correctly separates it from the adjacent *loud* defect (unescaped `- [ ]` hard-erroring), noting that a hard error cannot cause a silent false zero. That distinction is right and worth keeping.
- L/M/N correctly left unticked.

## One caveat the impler must carry into the marker

The plan says "there are 4 actionable items and there have been throughout." The first half is verified. **The second half is an inference** — I have this tick's state, not the file's history, and cannot confirm the count was 4 at tick-300. The marker must say *"the queue is non-empty now, and the query shape that reported it empty was incapable of reporting otherwise"* — which is the defensible claim and is sufficient. Do not assert a historical count.

## Feedback for planner

None blocking. Proceed to impl with the caveat above.
