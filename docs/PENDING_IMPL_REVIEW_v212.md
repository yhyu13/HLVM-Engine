# Pending Impl Review v212

- plan: docs/PENDING_PLAN_v212.md
- commit: docs/PENDING_COMMIT_v212.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-558)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan called for a determination and no patch. The commit produced exactly
that, and the `## Plan Deviations` section correctly reads **None** — with the
two deviations the impler *considered and rejected* written down rather than
left silent. That is the right shape: a deviation section that says "none"
without saying what was tempting is unfalsifiable.

Both rejections hold on review, and the first one is load-bearing enough that
I re-derived it:

- **Rejecting the four `GBufferSponzaPS.hlsl` "stale copy" patches** is
  correct. `MRT4 : SV_TARGET4` → 4 hits (RT family) and `Tangent : TEXCOORD3`
  → 5 hits (2 PS + 3 VS/instanced-VS), disjoint, summing to the group size.
  Forcing the 4-MRT PBR variant to the 5-MRT RT contract would have broken
  `TestSponzaDeferred` and `TestGPUInstancing` to "fix" a non-defect.
- **Rejecting the nine per-group "swept and clean" comments** is correct on
  v206's own terms: v206 warranted a comment because it recorded a *non-obvious
  invariant with an opposite-facing sibling* that a reader would otherwise get
  wrong. "These files agree" is the default expectation, and a comment
  asserting it decays silently the first time a copy changes. A stale
  `verified in agreement` comment is worse than none — it is the same
  camouflage mechanism as v193's tautological guard and v205's optional-guide
  comment, which this lineage has now been bitten by three times.

## The load-bearing claim is a NEGATIVE, and I verified it independently

A determination cycle hands the reviewer no diff. So the artifact to review is
the *absence* of one, and accepting the impler's word for it would make this
gate ceremonial.

**Re-derived, not accepted:** I re-queried the file v211 patched — the one
file in the tree most likely to have been disturbed by a cycle sweeping the
same domain — from both ends:

- `TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl` → `GuideScale` at
  `:21` (cbuffer slot 5), `:33` (the degradation comment), `:37`
  (`int s = max(int(GuideScale), 1);`)
- `Shader/BilateralDenoise_cs.hlsl` → the same three, at `:21`, `:41`, `:45`

v211's patch is **present and intact in both**, with the line offsets differing
only by the comment-block length the two files already had. Nothing in this
cycle perturbed it.

**Controlled positive accepted.** The impler's argument that the write path is
demonstrably functional (four successful `patch` calls against
`PENDING_PLAN_v212.md`, each returning a diff) is exactly v205's rule applied
to a negative, and it is the right control. Without it, "no source files were
written" is indistinguishable from "the write tool was silently failing."

## Security scan

- [x] No hardcoded secrets — no files written outside `docs/`
- [x] No shell injection — no shell executed (terminal refused at the boundary)
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A
- [x] No governance file touched — `AGENTS.md`, `.opencode/`, CMake, and
      `Build.sh` all unmodified; only `docs/PENDING_*_v212.md` written
- [x] No commit, no push — cron does not have and did not attempt either

## Self-review checklist

- [x] **Validation**: every claim in the commit is file-derived and each zero
      is controlled by a same-shape positive.
- [x] **Error handling**: the `GB\(` query returned
      `grep: Unmatched ( or \(` — an **error**, and the plan correctly did
      *not* read it as a zero. This is v205's rule working as intended, and
      it is worth noting it fired live this cycle rather than in retrospect.
- [x] **Tests**: file-only verifier; no test files produced, so HARD INVARIANT
      #2 is satisfied by `produces_test_files: no` being truthful (verified —
      `files: NONE`).

## Net-new at this gate

**The row-21 proposal should be narrowed before adoption, and I am carding the
narrowing rather than silently rewriting it.**

The plan proposes row 21 as: *re-derive the domain's membership, not only each
member's count.* As stated that is unbounded — "the domain" of an invariant
can always be widened one more level (this file → this directory → this class
→ this repository), and v204 already caught v200/v201 making a scope error one
level up. A rule to re-derive membership with no stopping condition licenses
infinite regress, which in a thinning seam is indistinguishable from drift.

The honest form is: *the domain must be derived from a query whose result set
is enumerable and enumerated — and the marker must state the query and the
partition, so a reader can check the domain rather than trust it.* That is
what this cycle actually did (205 files → first-party partition → 9 groups),
and it has a natural stopping condition: you stop when the enumeration is
exhausted, not when you feel done.

Forwarded to the audit as the recommended wording. **Not** silently applied —
per v195, a reviewer rewriting a planner's proposed rule without flagging it
is the same self-justifying loop the plan-criticer/reviewer split exists to
prevent.

## Feedback for impler

None. KEEP.

## Honest residue

This cycle **clears no acceptance gate**. 0 of 7 remain verified against the
current tree, and gate 6 remains structurally unreachable from this runspace
regardless of the terminal block (no vision/image tool exists here). The
commit marker states this plainly and does not inflate the finding, which is
the behaviour this gate should reward.
