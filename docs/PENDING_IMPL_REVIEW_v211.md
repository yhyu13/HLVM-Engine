# Pending Impl Review v211

- plan: docs/PENDING_PLAN_v211.md
- commit: docs/PENDING_COMMIT_v211.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-557)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl is exactly the plan's three-part remedy: slot-5 rename, `GB()` helper,
four guide call sites. The commit declares **no deviations**, and I confirmed
that independently — there is nothing in the file the plan did not authorise,
and nothing the plan authorised that is missing. Both strengthenings the plan
gate asked to be carried forward (the `.sblob` evidence; the framing
correction from "false instrument" to "nobody asked") are present and stated
accurately, not softened.

## Independent re-derivation

**Declaration-order hazard — checked, and it is the one thing that could have
made this file fail to compile.** `GB()` at `:43` references `GuideScale`,
which HLSL requires to be already declared. The `cbuffer` closes at `:24`, so
the ordering is correct. Worth stating because the impler placed the helper
*after* the cbuffer and *before* the texture declarations, which is the
primary's layout too — had it been hoisted above the cbuffer to sit with the
other free functions, this would be a compile error, and the chain is unbuilt,
so it would have surfaced 17 cycles downstream and been blamed on v183-v211.

**Binding surface unchanged — the row that matters for a shared shader.**
`register` → 6 hits: `b0`, `t0`, `t1`, `t2`, `s0`, `u0`. Identical to the
pre-edit set. This is the check that makes the edit safe against
`FBilateralDenoisePass`'s binding layout, which the impler did not have to
touch and did not touch. **A shader-only edit that silently changed a register
would pass every row in the commit marker and still break the pass** — v202's
layout-vs-consumer invariant applied to this cycle's own patch.

**The over-reach row re-run with a different query shape.** The commit closed
it with `pixelCoord\]` → 1. I ran the inverse: enumerated all four `t_Input`
reads and the single `u_Output` store. `:92` `t_Input.Load(int3(pixelCoord,0))`,
`:133` `t_Input[uint2(neighborPixel)]`, `:139` `u_Output[pixelCoord]` — **all
on raw dispatch coords, none through `GB()`.** Correct: these are dispatch-res
resources. The one edit capable of converting a latent defect into a live one
was not made.

**Cbuffer tail verified in place, not from the diff.** Read `:17-23`:
`float2 TexelSize`, `DepthSigma`, `NormalSigma`, `SpatialSigma`, `GuideScale`,
`Pad1`, `Pad2`. Eight floats, `GuideScale` at slot 5 — which is where
`FBilateralDenoisePass::Dispatch` writes it (`ConstantsData[5] = GuideScale`,
`:189`). **The C++ marshaller and this shader now agree on slot 5 for the
first time.** Previously the marshaller wrote a real scale into a slot the
shader called `Pad0` and ignored.

## One observation, deliberately NOT carded

The impl converts three guide/input reads to `.Load(int3(...))` but leaves
`:133 t_Input[uint2(neighborPixel)]` as `operator[]`, where the primary copy
uses `.Load(int3(neighborPixel, 0))` at the corresponding site. This is a
**form divergence with no semantic content** — `operator[]` on a `Texture2D`
is defined as `Load` at mip 0 — and the coordinate is already bounds-checked
at `:103`.

I am **not** carding it. After 211 cycles in a thinning seam the gradient is
toward carding any asymmetry, and v210's audit made exactly this call in the
opposite direction and was upheld: **not carding a difference that is provably
inert is the harder and the right judgement.** Recording it here so a future
cycle that notices it does not re-litigate it as a finding.

## TDD evidence

N/A — no test files produced (`produces_test_files: no`). The verification
regime for a shader edit in a file-only runspace is the tester's row set, not
a red-phase commit.

## Security scan

- [x] No hardcoded secrets — HLSL compute shader, no strings
- [x] No shell injection — no shell surface
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] **Validation**: `max(int(GuideScale), 1)` guards the unfilled-constant
      case. Verified in place at `:45`. A zero constant yields the identity
      map, not a collapsed index — the v184 silent-zero lesson, which is the
      specific way this class of fix has previously been *undone*.
- [x] **Error handling**: no new failure mode. The patch is an identity
      transform at `GuideScale == 1`, the only value any current consumer
      produces, so it cannot perturb the unbuilt v183-v210 chain.
- [x] **Tests**: file-only rows only; see `PENDING_TESTS_v211.md`.

## What this cycle established, and what it did not

**Established (file-only, sound):** a compiled, on-disk-blob-backed default
copy of a shared shader carried the pre-v204 defect and now does not; the
three-copy cardinality is now a derived fact rather than an inherited one; and
the inherited-cardinality failure mode is named.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates. **0 of 7 acceptance gates verified against the patched
tree**; `terminal` was probed first-hand this tick and refused at the tool
boundary.

**Severity, without inflation:** latent. This cycle moves no pixel.

## Feedback for impler

None. KEEP.
