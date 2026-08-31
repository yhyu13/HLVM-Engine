# Pending Plan Review v194

- plan: docs/PENDING_PLAN_v194.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-540)
- timestamp: 2026-08-30

## Design soundness

The plan solves the stated problem and, unusually for this lineage, it closes
the card's open design question rather than deferring it. I re-derived the
central claim independently rather than accepting it:

`Desc.OutputWidth` is read at exactly two places in `FReBLURPass::Dispatch` —
the local `outputW` initialiser and, through it, `dispatchX = (outputW + 7) / 8`.
`outputH` likewise, four hits total. The `OutputTexture->getDesc()` fallback
assigns only those two locals. Separately, `Constants.OutputSize` appears four
times in that file: two are the verbatim marshalling into `ConstantsData`, and
two are in the *other* `Dispatch` overload. **Nothing in the fallback path writes
`Constants`.** So the plan is right that taking the fallback would fix the grid
and leave `OutputSize`/`RcpOutputSize` swapchain-derived — an internally
inconsistent dispatch, worse than either pure option. Card G's "genuine design
choice" is correctly dissolved rather than answered.

**Corroboration the plan did not use, and should have.** The convenience overload
`Dispatch(CmdList, Input, Output, W, H)` sets all six quantities from the same
`W`/`H` pair — `OutputWidth`/`OutputHeight` *and* `OutputSize[0..1]` *and*
`RcpOutputSize[0..1]`. That is the API's own worked example of the intended
calling convention, and it passes the extent explicitly rather than relying on
the fallback. The plan's chosen shape is the one the class already demonstrates
on itself. This strengthens the plan; it is not a defect.

## Plan completeness

All six sites are enumerated with their consumers. The four fixed-size resources
are each traced to a creation site, and I confirm the plan's precision about
`ReBLURHistoryTexture`: it is created from `Desc.width = WIDTH` directly, not via
the `W`/`H` locals, which is a different code path from the other three and was
worth distinguishing.

The three failure modes are correct. I verified the shader has no extent guard:
`return;` appears exactly three times in `ReBLUR_cs.hlsl` — inside the
`depth == 0.0` sky branch, inside the all-zero-radiance branch, and the trailing
one at end of `main`. All three are *content* tests, none is an extent test, and
each of the two early-outs still stores through `gOutput[dispatchThreadID.xy]`
before returning. So the out-of-bounds store on a widened window occurs on **every**
path including the early-outs, not merely the fall-through. That is marginally
worse than the plan states and I record it here rather than sending the plan back.

Failure mode 3 is the substantive new contribution and it checks out.
`pixelUv` has exactly two occurrences: its definition from `texelSize`, and
`IsHistoryValid(pixelUv)`, which is `all(uv > 0) && all(uv < 1)`. With
`historyValid` false, `TemporalAccumulation` returns `current` unblended. The
characterisation — this instance corrupts a *weight* where every prior instance
corrupted an *index* — is accurate and is the reason this is not a mechanical
repeat of v191-v193.

**Severity re-rating endorsed.** `AccumInput = DenoisedTexture` at the end of the
branch, and `bBypass` is set only from `std::getenv("HLVM_RGI_BYPASS")`. Under
the acceptance recipe the branch is taken and this pass feeds the accumulate pass
that writes `DisplayTexture`. Card G's "lower severity than card F" rested on the
`denoised` dump being unread, which is true but not the whole output path. The
plan is right to retract it.

## The one thing I would have rejected

Had the plan proposed the fallback route — passing nothing and letting
`getDesc()` supply the extent — that would have been a FIX, because it fixes the
visible half and leaves the half the shader actually reads. The plan avoided it
for the right reason. Noting the counterfactual so the reasoning is on record.

## Scope check

Six functional lines in one block of one file. Both `ReBLUR_cs.hlsl` copies and
`FReBLURPass.cpp` untouched, so the v182 dual-copy hazard stays inert. No-op at
800x600, so the v183-v193 chain awaiting an operator run cannot be perturbed.
The blit-adjacency risk is named with a concrete post-check. Proportionate.

## Feedback for planner

None. Proceed to impler. Two additions for the impler to fold into its own
verification rather than a re-plan:

1. The out-of-bounds store on a widened window happens on all three shader
   paths, including both early-outs — not just the fall-through.
2. Cite the `Dispatch(CmdList, Input, Output, W, H)` overload as precedent for
   passing the extent explicitly at all six sites.
