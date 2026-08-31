# Pending Impl Review v194

- plan: docs/PENDING_PLAN_v194.md
- commit: docs/PENDING_COMMIT_v194.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-540)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly: six substitutions, the six sites the plan
enumerated, no seventh. One deviation is declared, and it is a *scope*
declaration rather than a departure — the impler found a candidate ninth
instance while executing the plan's row-4 enumeration and carded it instead of
absorbing it. That is the correct disposition and matches how v192 and v193 each
handled the card they opened. The plan-review's two folded-in additions (the
all-three-paths OOB detail, the convenience-overload precedent) both appear in
the source comment.

## Independent re-derivation

I re-ran the load-bearing rows rather than reading them.

**Rows 1-3.** All six sites confirmed at their new values, and `OutputTexture`
confirmed *not* substituted — the `ReBLURDesc.Output` query returns three hits
and exactly two of them changed.

**Row 6 — shadowing.** `const uint32_t W = WIDTH, H = HEIGHT;` returns exactly
three hits, at 1607, 1733 and 1826. The patched block sits at 1182-1233, inside
`Render()` which opens at 746. **All three shadowing declarations are
numerically after the patch site**, so none of them is in scope there — a
stronger result than "they are in other functions," and it does not depend on my
reading the brace structure correctly. `WIDTH`/`HEIGHT` resolve to the file-scope
`static const uint32_t WIDTH = 800; HEIGHT = 600;`. Confirmed.

**Row 8 — types.** Checked the header rather than inferring from the `.cpp`:
`FDesc::OutputWidth` and `OutputHeight` are `uint32_t` with `= 0` default
initialisers. `WIDTH`/`HEIGHT` are `uint32_t`. Exact match. The `= 0` defaults
are also what makes row 9's reasoning about the fallback correct.

**Row 12 — no-op at default.** `WindowProps.Extent = { WIDTH, HEIGHT }`, so the
six expressions are value-identical at startup. The v183-v193 chain cannot be
perturbed by this patch.

**Row 4.** Re-ran `FB.width` → 12 hits and walked the impler's partition. It
accounts for every hit and I agree with each classification. The
`RenderGBuffer(FB.width, FB.height)` call being inert is independently confirmed:
the definition is `RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)` with both
parameter names commented out, and the body logs `LastWidth`/`LastHeight`. Worth
noting that this is a *latent* trap of a different kind — a function that ignores
its arguments — but it is out of scope and harmless today.

## The finding that raises this cycle's stakes above its predecessors

Every cycle from v191 onward has carried a reachability caveat of the form "the
defect manifests only on a resize, which the standard recipe does not perform."
v193's audit stated it plainly and treated the cycle's correctness as resting on
source argument alone.

While checking row 12 I read the two lines after it:

    WindowProps.Extent   = { WIDTH, HEIGHT };
    WindowProps.Resizable = true;

**The window is resizable.** So this class of defect is not reachable only via a
hypothetical code change — it is reachable by a user dragging the window edge
during the run. That does not make it reachable under the *automated* recipe,
which is what the acceptance gates run, so I am not upgrading any gate. But the
standing caveat should be stated more precisely from here on: *not exercised by
the recipe*, rather than *not reachable*. I record it here because it is the
kind of premise that silently hardens into "unreachable, therefore theoretical"
over a long lineage.

## Severity assessment — I endorse the plan's retraction of card G's rating

Verified the propagation chain link by link: the ReBLUR branch ends with
`AccumInput = DenoisedTexture`; the accumulate pass consumes `AccumInput` as its
SRV and writes `DisplayTexture`; `DisplayTexture` is the `display` dump and the
artifact `validate_restir_gi.py` checks. And `bBypass` is set only from
`std::getenv("HLVM_RGI_BYPASS")`, which the recipe does not set. So under the
acceptance run this pass executes and feeds the acceptance artifact. Card G's
"lower severity than card F, because no gate reads the denoised dump" reasoned
from the dump alone and missed the SRV hand-off. Correctly retracted.

The `RcpOutputSize`-as-scale mode is the genuinely new variant. Confirmed
`pixelUv` has exactly two occurrences and its only consumer is
`IsHistoryValid`. A corrupted *weight* rather than a corrupted *index* is a
failure mode with no out-of-bounds access and no VUID — nothing in gates 3 or 4
could ever surface it.

## Security scan

- [x] No hardcoded secrets — six numeric-constant substitutions
- [x] No shell injection — no process execution
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: types checked against the header, not inferred; constants
      confirmed non-zero so the fallback stays disengaged
- [x] Error handling: unchanged. The `outputW == 0` guard in `Dispatch` is
      untouched and now provably unreachable from this call site
- [x] Tests: none produced; `produces_test_files: no` is accurate, so the
      HARD INVARIANT #2 condition for honouring a skip does not arise — and it
      was not skipped anyway
- [x] Anchor convention: the comment names symbols (`ReBLUR_cs.hlsl main`,
      `IsHistoryValid`, `FReBLURPass::Dispatch`, `CreateGBufferTextures`) with
      no `:NNNN` references, per the v190 ban. Every symbol named exists in a
      file this cycle does not delete, so no anchor dangles — the v193 lesson
      applied and, this time, applied correctly on the first attempt

## Feedback for impler

None. Proceed to tester.

## Standing stop-condition (carried forward from v190, restated)

The v190 review set the condition: stop if a cycle produces no functional
change. This cycle changes six functional lines and opens one card, so the
condition is not met. **But I want to sharpen it for whoever runs v195**: the
extent class has now yielded eight instances found by the same query shape, and
card H is a *design* question rather than a substitution. If v195 finds itself
substituting a seventh identical `FB.width`, that is evidence the sweep should be
done in one pass with a single audit rather than one card per cycle.
