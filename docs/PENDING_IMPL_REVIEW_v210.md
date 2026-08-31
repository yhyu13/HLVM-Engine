# Pending Impl Review v210

- plan: docs/PENDING_PLAN_v210.md
- commit: docs/PENDING_COMMIT_v210.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-556)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly and the commit declares **no deviations**,
correctly — the plan proposed comment-only, the plan gate ruled on that
question explicitly rather than accepting it, and comment-only is what landed.
+22 comment / 0 functional, one file, matching the diff and the 169→191 line
delta independently.

## Verification of the comment's factual content (v190 rule)

I re-derived every claim rather than judging whether the comment reads well.
All five hold; four are re-derivations of the impler's, one is new:

| Claim in the comment | Independent check | Result |
|---|---|---|
| `getDesc()` only on `OutReservoir0`, never a guide | `getDesc` → 3 in `.cpp` | holds |
| one scale, four guides | `GB(` → 5 in primary shader (def + 4 sites) | holds |
| `max(int(s),1)` → identity map at 0 | `:80` read in place | holds |
| `FReBLURPass` has no scale field | `GuideScale` → **0** in `.cpp`, controlled by **1** in its `.h` | holds |
| `FBilateralDenoisePass` callee-derives | `.cpp:185-187` read in place | holds |

## NET-NEW FINDING — the two shader copies diverge, and the divergence is CORRECT

The impler did not check the dual-copy axis, which is the standing hazard in
this tree (v182). I did, and it produces the sharpest corroboration of the
comment yet:

- `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl`: declares
  `GBufferScale` (`:42`) **and** defines `GB()` (`:78`), applied at 4 guide
  reads.
- `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl`: declares `GBufferScale`
  (`:40`) but **`GB(` → 0 hits** — no helper, guides indexed raw.

The zero is controlled: `Load` → **9 hits** in that same file under the same
query shape, so the file is readable and the pattern works; `GB()` genuinely
is not there. Per audit rows 18/v205/v209 I also confirmed no `search_timeout`
and no `error` field on that zero.

**This is NOT a card M-style layout/consumer divergence and must not be
carded as one.** The control is not half-res: it sets `GBufferScale = 1.0f`
(`TestCornellBoxGI.cpp:1592`, `:1645`), for which `GB()` is the identity map.
Declaring the field keeps the cbuffer tails aligned across copies — the v184
/ v186 / v200 requirement — while omitting the helper it does not need. Both
copies are correctly different, which is exactly the case v202 established
that no *sameness* check can adjudicate.

**Why this strengthens rather than threatens the patch**: the comment says the
caller must supply the ratio and that leaving it unset is silent. The control
demonstrates the benign end of that contract (supplies 1.0f explicitly, having
been bitten by the `GBufferScale == 0` case at v184, per its own comment at
`:1574`). The primary demonstrates the load-bearing end. **Two consumers, two
different correct values, both explicit — which is precisely the argument for
documenting the contract at the `FDesc` rather than deriving it in the callee.**

## Security scan

- [x] No hardcoded secrets — comment-only
- [x] No shell injection — no code
- [x] No eval/exec — no code
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: no code path added; nothing to validate
- [x] Error handling: unchanged; the comment documents an existing silent
      failure mode rather than introducing or suppressing one
- [x] Tests: `produces_test_files: no` is correct — no path under a test dir
      is in `files:`, so HARD INVARIANT #2 is satisfied and the reviewer
      (this gate) ran anyway because `skip_impl_review: no`

## Severity assessment — the commit's own statement is accurate

The commit says the cycle "moves no pixel and clears no acceptance gate" and
that there is "no defect today." Both verified: primary `:1061`/`:1109`,
control `:1592`/`:1645`, all four assignments present and correct. **The
marker does not inflate its finding**, which is the failure mode this lineage
has to watch for after 210 cycles of diminishing seams.

## Feedback for impler

None. Proceed to tester.
