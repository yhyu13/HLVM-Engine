# Pending Impl Review v183

- plan: docs/PENDING_PLAN_v183.md
- commit: docs/PENDING_COMMIT_v183.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-530)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the revised plan exactly. All four plan-review items are
present in the code, verified first-hand rather than from the commit summary:

- **Zero layout delta.** `FReSTIRPass.h:44-45` is `Pad[2]` + `GBufferScale`
  (3 floats, was `Pad[3]`); `:57-58` is `GBufferScale` + `Pad` (2 floats, was
  `Pad[2]`). Neither struct changed size.
- **Guard present in both copies.** `ReSTIR_Temporal_cs.hlsl:74` and
  `ReSTIR_Spatial_cs.hlsl:54` both read
  `int s = max(int(gConstants.GBufferScale), 1);`.
- **Convention match.** Both helpers return `p * s + (s >> 1)`, which at s=2
  is `p*2 + 1` — identical to `Resolve_cs.hlsl:60`.
- No `## Plan Deviations` content to assess; none were declared and none were
  found.

## Field-order audit (the one residual risk the impler flagged)

A zero-size rename can still break if field ORDER diverges. Checked
field-for-field:

| # | `FReSTIRPass.h` (C++) | `ReSTIR_Temporal_cs.hlsl` (HLSL) |
|---|---|---|
| … | `PrevSceneYaw` `:43` | `PrevSceneYaw` `:34` |
| … | `Pad[2]` `:44` | `Pad[2]` `:35` |
| last | `GBufferScale` `:45` | `GBufferScale` `:36` |

Spatial: C++ `DebugVis :56` → `GBufferScale :57` → `Pad :58`; HLSL
`DebugVis :24` → `GBufferScale :25` → `Pad :26`. Order matches on both.
`float2 Pad` → two scalars is the correct HLSL expansion; no implicit
16-byte-boundary padding is introduced, because in both structs the new
scalar sits in the same slot the old array element occupied.

## Conversion-partition audit (the correctness core)

Exhaustive `Load` enumeration — 8 per shader, and the split is exactly right:

**Temporal** — wrapped (full-res GBuffer MRTs): `:129` `gDepth`,
`:172` `gPrevDepth`, `:173` `gPrevNormals`, `:174` `gNormals`.
Unwrapped (genuinely half-res): `:117`/`:118` `gCurrReservoir0/1`,
`:195`/`:196` `gHistReservoir0/1`.

**Spatial** — wrapped: `:80` `gNormals`, `:81` `gDepth`, `:116` `gNormals`,
`:117` `gDepth`. Unwrapped: `:83`/`:84` and `:128`/`:129` `gReservoir0/1`.

4/4 and 4/4 in both files. No half-res read was wrongly converted — that
would have been the damaging failure mode, since it would have corrupted
reprojection while looking like a fix.

Also confirmed untouched, as the plan required: `:130` `uv` (from
`RcpOutputSize`), `:164` `prevPixel`, `:170` and spatial `:113` bounds checks
against `outputSize`. All remain in half-res space.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no shell surface in this change)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] **Validation:** `std::max(HalfResWidth, 1u)` guards the C++ divide;
      `max(...,1)` guards the shader. `HalfResWidth` is set at
      `TestReSTIR_GI_Temporal.cpp:1531` inside `CreateGBufferTextures`, which
      runs before any dispatch, so the guard is defence-in-depth rather than
      load-bearing. `std::max` with a `1u` literal already appears at `:432`,
      so no include change is needed.
- [x] **Error handling:** the failure mode is now graceful — an unset constant
      reproduces the pre-v183 behaviour instead of collapsing every sample to
      texel (0,0).
- [x] **Tests:** no test files produced; `skip_impl_review: no` was correctly
      set, so this review ran (HARD INVARIANT #2 satisfied).
- [x] **Scope:** `TestCornellBoxGI_Data/ReSTIR_{Temporal,Spatial}_cs.hlsl`
      confirmed untouched. `GBufferScale` appears in exactly 4 places, all
      under `TestReSTIR_GI_Temporal_Data/`.

## Risk note carried forward (NOT a FIX trigger, but must not be lost)

This patch changes the **production** render path — unlike v182, which was
confined to `#ifdef HLVM_RGI_DEBUG_VIS`. It therefore *can* change the
display output and *can* move the validator result in either direction. The
reasoning that it should improve temporal accumulation is sound but remains
a prediction: no build and no run were possible in this runspace (terminal
denied by tirith down to `pwd`; no vision tool per tick-528).

The correct falsifiable expectation for the operator: `ReSTIR summary: M
mean` should rise substantially above the current `2.93` toward `MaxM=30`.
If M does **not** move after this patch, the half-res/full-res mismatch was
not the dominant rejection cause and the hypothesis is wrong — that outcome
must be reported as such, not explained away.

## Post-report deviation audit (tick-530, added after the ad-hoc verification pass)

The commit now declares a `## Plan Deviations` section that did not exist at
first review. Per the deviation policy this reviewer must assess it
explicitly. **Verdict unchanged: KEEP** — the deviation is justified, and
catching it materially improved the cycle.

**What was wrong.** `FReSTIRPass.cpp` marshals both cbuffers field-by-field
into `float ConstantsData[64]`, not via `memcpy` of the struct. Verified
first-hand at `:424-444` (temporal) and `:518-532` (spatial). Declaring
`GBufferScale` in the header was therefore insufficient — the value never
reached the GPU. The original v183 patch was **inert**.

**Why this was nearly invisible.** The `max(scale,1)` guard mandated by
plan-review item 3 made the inert patch reproduce the OLD behaviour exactly.
Without that guard the scale would have been 0 and every read would have
collapsed to texel (0,0) — a loud, obvious failure. With it, the failure was
silent: "patch applied, nothing changed." The guard that made the patch safe
also made its own inertness hard to see. Worth recording as a general lesson:
a defensive clamp can mask a plumbing bug.

**Pre-existing bug surfaced (not introduced here).** Temporal marshalling
stopped at `PrevSceneYaw`, so `Pad[0]`/`Pad[1]` — near/far, written at
`TestReSTIR_GI_Temporal.cpp:976-977`, read at `ReSTIR_Temporal_cs.hlsl:140-141`
— never reached the GPU. `ndcZ` was computed with `near = far = 0`. The
shader's own comment (`:136-138`) states that an inexact `ndcZ` makes the
reprojected pixel drift so that "the depth/normal validation rejects every
history sample (M never accumulates)." That is a **second, independent cause
of `M mean=2.93`**, orthogonal to the half-res/full-res mismatch, and it is
now fixed by the same three added lines.

**Sibling-test safety re-verified.** `TestCornellBoxGI` shares
`FReSTIRPass.cpp`. Its shader copies declare `float Pad[3]`
(`ReSTIR_Temporal_cs.hlsl:15`) and `float2 Pad`
(`ReSTIR_Spatial_cs.hlsl:19`) at exactly the offsets the three new writes
target, and never read them — the values land in declared padding. Capacity
is fine: `ConstantsData[64]` = 256 B, temporal high-water mark 46 floats,
cbuffer allocated at 256 B (`:317`).

**Assessment:** justified deviation, correctly scoped, correctly documented,
and it converts the cycle from inert to actually load-bearing. No FIX.

## Feedback for impler

None. KEEP.
