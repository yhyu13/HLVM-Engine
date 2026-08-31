# Pending Plan Review v192

- plan: docs/PENDING_PLAN_v192.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-538)
- timestamp: 2026-08-30

## The plan's most important move is a NEGATIVE, and it is correct

Card E said the shader's hardcoded `int2 fp = hp * 2 + 1;` was "a genuine design
decision" whose treatment was an open question. The plan read the shader and
concluded it must **not** be touched. I verified that independently by reading
`Resolve_cs.hlsl` in full (74 lines):

`fp` is used only at `:61-62` to fetch the *representative* full-res depth/normal
for a half-res texel `hp` — the centre of its 2x2 footprint. The relationship it
encodes is between `OutputTexture` (400x300) and the GBuffer MRTs (800x600), both
descended from the same `W` in `CreateGBufferTextures`. **The swapchain is not
one of its operands.** Parameterising it would introduce a constant that can only
hold 2.

This is the right call and it is the opposite of what the card implied. It also
makes the cycle shader-free, which sidesteps the v182 dual-copy trap entirely —
worth stating because `Resolve_cs.hlsl` exists in only one data directory
(`search_files target=files pattern="Resolve_cs.hlsl"` — the Cornell tree has
none), so a shader edit here would *not* have hit that trap anyway. The plan
gets the right answer for the right reason regardless.

## Shadowing — `HEIGHT` checked, which v191 never did

The plan correctly flagged that v191 only cleared `WIDTH`.
`search_files pattern="HEIGHT"` → 8 hits: sole declaration `:107`
`static const uint32_t HEIGHT = 600`; uses at `:564`, `:2931`, `:2946`; locals
`const uint32_t W = WIDTH, H = HEIGHT` at `:1541`, `:1667`, `:1760` — all in
functions other than `Render()`; and one comment at `:2582`. **No shadowing at
the resolve block. Both substitutions bind to `:106`/`:107`.**

## Premise verified: all four bindings are fixed-size

I checked each binding of the resolve dispatch rather than accepting the plan:

- **Outputs.** `DispatchResolve` is called exactly twice, `:1141` with
  `FullResGIRaw` and `:1142` with `FullResSpatial`. Both are created at
  `:1633-1638` via `CreateTexture2D(NvrhiDevice, W, H, ...)` inside
  `CreateGBufferTextures`, where `W = WIDTH`. **Fixed.** Neither call site passes
  a swapchain-sized texture, so the substitution is safe for both.
- **Guides.** t1/t2 are `LinearDepthTexture` and `GBufferNormal` (`:1129-1130`),
  the fixed GBuffer MRTs.
- **Input.** t0 is the half-res texture, and `HalfSize` is fed from
  `HalfResWidth`/`HalfResHeight` (`:1105-1106`) — already correct, untouched.

So `FB.width` at `:1109`/`:1138` is the only swapchain-derived quantity in a pass
where nothing else is swapchain-derived. Premise holds.

## The severity analysis is sound, and one part of it is stronger than v189/v191

I checked the plan's claim that the widened case is an out-of-bounds **write**,
because that is a materially different hazard from the out-of-bounds `Load`s in
the previous cycles. Confirmed: the shader writes `FullResOutput[tid.xy]` at
`:34` and `:73` and there is **no bounds test on `tid.xy` anywhere in the
kernel** — the only early-out (`:32`) is on `centerDepth <= 0.0`, which is a
*data* condition, not an extent condition. An out-of-range UAV store is
undefined behaviour, not the harmless zero that an out-of-range `Load` returns.

The narrowed case is also correctly analysed: the `:49`
`clamp(base + int2(dx,dy), 0, int2(HalfSize) - 1)` pins over-range threads to the
input's last column (smear), and the short grid leaves the remaining columns of
an 800-wide output never written this frame (stale). Both silent, no VUID.

## Plan completeness

Strong on the points this lineage keeps failing:

- **Disclaims causation explicitly.** The resolve pass runs after ReSTIR reuse and
  cannot affect reservoir accumulation, so it cannot explain `M mean=2.93`. Stated
  plainly. This is the third cycle running to get that right.
- **Predicts a no-op at 800x600** and says so as a falsifiable prediction rather
  than a reassurance.
- **Bans line-number references in the comment up front**, having watched v191
  nearly regress v190's zero-stale-refs result. Correct pre-emption.
- **Requires positive controls for every zero-hit test row**, now that three
  false-zero mechanisms are documented.

One gap, minor, folded into the impl rather than a FIX: the plan does not say
what the comment should assert about the *shader*. Since this cycle's central
finding is that `hp * 2 + 1` is correct-and-invariant, the comment should record
that — otherwise the next reader re-opens card E's question.

## Feedback for planner (FIX only)

None — KEEP. One instruction folded into the impl: the comment must state that
the shader's `hp * 2 + 1` is deliberately left alone because it encodes the
fixed half-to-full relationship, not a swapchain ratio.
