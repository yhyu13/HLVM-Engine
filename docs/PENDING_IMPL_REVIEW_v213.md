# Pending Impl Review v213

- plan: docs/PENDING_PLAN_v213.md
- commit: docs/PENDING_COMMIT_v213.md
- verdict: **KEEP**
- reviewer: agent_4_reviewer (tick-559)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan as revised at the FIX gate. **No deviations declared,
and none found** — I re-derived each claim rather than accepting the marker.

Two deviations were *considered and rejected* by the impler and both rejections
are correct, so there is nothing for me to rule against:

- **Dummy normal rejected**: verified `FReBLURPass.cpp:289` does document its
  dummy as `(0,0,1)`, and this shader's `normalWeight` is `dot(n1,n2)`-driven,
  so a constant guide flattens every kernel weight. The two classes are not
  interchangeable and the header now says so.
- **Shader gating rejected**: correct. The guard makes the null case
  unreachable before any dispatch, so touching three shader copies would engage
  the v182 dual-copy hazard for zero behavioural gain.

## Independent re-derivation

**The guard's placement is the load-bearing detail and the impler did not argue
for it — I checked it.** It sits at `:171-175`, after the dimension check and
**before** `:177` `// Upload constants` / `:191` `writeBuffer`. So on the null
path no constant is uploaded, no binding set is built, no compute state is set.
Had it been placed after the upload, the pass would have written a constant
buffer it then never consumed — harmless but confusing in exactly the way this
class's history is confusing. It is where it should be.

**The contract sweep is closed, not sampled.** `optional` → 2 hits in the
`.cpp`, 1 in the `.h`; all three are the new explanatory prose. Zero stale
claims survive in either file. The `.h:32` premise ("because NormalTexture is
optional") was the subtle one — it is a *reason* resting on the false claim
rather than a restatement of it, and rewriting the field comment alone would
have left it standing two lines above the fix.

**v205's fix is undisturbed** — re-queried, not inherited: `if (Desc.DepthTexture)`,
`GuideScale = static_cast<float>(GuideW / outputW)`, `ConstantsData[5]` all
present at `:210-215`. This was the real risk of the cycle: the edit rewrites
the comment block *directly above* v205's branch, and v203's near-miss (an
`old_string` anchored on a comment matching into the adjacent initialiser and
deleting three live binding items) is exactly this shape. The returned diffs
were read and show comment lines only.

## The LSP argument — checked, and it holds

I re-examined it rather than accepting it, because "the linter is wrong" is the
most self-serving claim an impler can make (v207's row exists for this).

It holds, on the impler's second proof more than its first: **the error count
fell 14 → 11 across an edit that only added comment lines.** A regression cannot
reduce an error count. Combined with every error being an unresolved `nvrhi` /
`FString` / `uint32_t` — i.e. the file's own `:17-18` includes failing to
resolve in a standalone parse — and with stale line numbers (`[53:53] FString`
pointing at `float SpatialSigma`), these are parse-context artifacts.

**The controlled positive is what settles it**: the `.cpp` edits, including the
one that added a real `if` statement, returned **no diagnostics at all**. If the
tool were reporting genuine problems it would have had more to say about new
control flow than about new comments.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection — no shell surface
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] **Validation**: the added guard IS the validation; it is loud, names the
      field, and states the consequence.
- [x] **Error handling**: `err` not `warn`, correctly distinguishing a caller
      contract violation from the recoverable dimension case above it.
- [x] **Tests**: none required — `produces_test_files: no`, and no behaviour
      changes for either live consumer.

## One narrowing, forwarded rather than applied (per v195)

The header comment says "all three copies of BilateralDenoise_cs.hlsl". That is
true today (verified: 3 files, `t_Normal` → 3 hits each) but it is a **cardinality
written into the tree**, and v211's row 20 is precisely that counts decay. If a
fourth copy appears, the comment becomes false silently.

I am **not** requiring a change — the sentence's load is carried by "with no
gate", not by "three" — but I note it as the weakest sentence in the patch, and
the audit should consider whether asserting a count in a source comment is ever
worth it. My own view: state the property, not the census.

## Severity concurrence

**LATENT**, as the impler states. No pixel moves, no acceptance gate clears.
The marker does not inflate this and I have nothing to correct.
