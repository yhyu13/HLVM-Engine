# Pending Plan Review v191

- plan: docs/PENDING_PLAN_v191.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-538)
- timestamp: 2026-08-30

## Design soundness

The plan's core claim is that `GBufferScale` is a ratio between two fixed
quantities but is computed from a third, variable one. I verified both operands
independently rather than accepting the plan's citations:

- **Numerator.** `CreateGBufferTextures()` opens `:1523` `const uint32_t W = WIDTH,
  H = HEIGHT;` and every texture the `GB()` helper indexes derives from it:
  `GBufferNormal` via `NmDesc = WpDesc` (`:1535-1537`, `WpDesc.width = W` at
  `:1527`) and `LinearDepthTexture` via `CreateTexture2D(..., W, H, ...)`
  (`:1562-1563`). Neither reads the framebuffer. **Fixed at 800x600.**
- **Denominator.** `:1578-1581` `HalfW = W / 2` → `HalfResWidth = HalfW`, same
  `W`. **Fixed at 400.**

Nothing recreates these on resize: `BackBufferResizing()` (`:1292-1295`) is two
lines and clears only `BindingCache`. `search_files pattern="CreateGBufferTextures"`
→ the only call is `:514`, inside init. So the GBuffer genuinely never follows
the swapchain, and `FB.width` at `:1023`/`:1069` is the wrong operand. **The
plan's premise holds.**

## The shadowing risk — checked, cleared

The plan correctly flagged that substituting `WIDTH` is unsafe if the identifier
is shadowed at the patch site. `search_files pattern="WIDTH"` → 40 hits, and
every local binding is `const uint32_t W = WIDTH` **inside a different function**
(`:1523` `CreateGBufferTextures`, `:1649` `FillGBufferHardcoded`, `:1742`). There
is no declaration of `WIDTH` itself other than `:106`, and none inside `Render()`.
`:1023`/`:1069` sit in `Render()`, which binds neither `W` nor a local `WIDTH`.
**Substitution binds to `:106`. Safe.**

This is the check that distinguishes this cycle from v187/v188/v190, where the
card's remedy was accepted at the gate and turned out wrong. Here the remedy is
verified before implementation.

## Verifying the failure analysis, not just the premise

I checked the plan's claim about integer truncation against the shader, because
the plan's severity argument depends on it. `ReSTIR_Spatial_cs.hlsl:52-56`:

```hlsl
int2 GB(int2 p)
{
    int s = max(int(gConstants.GBufferScale), 1);
    return p * s + (s >> 1);
}
```

`ReSTIR_Temporal_cs.hlsl:80` is byte-identical in form. So:

- The C++ `uint32_t / uint32_t` truncates **before** the `static_cast<float>` —
  confirmed by reading the expression, the cast wraps the whole division.
- At a swapchain width of 600: `600 / 400 == 1` → `s = 1` → `GB(p) = p`. The
  half-res correction v183 introduced is **exactly** undone. The plan's parallel
  to v184 (where `GBufferScale == 0` silently disabled v183 through `max(...,1)`)
  is precise, not rhetorical — same helper, same clamp, same end state.
- At 1200: `s = 3`, `p.x` up to 399 → `GB` returns up to 1198 against an 800-wide
  texture. Out-of-bounds `Load` returns 0, so depth/normal validation fails and
  history is rejected silently. No VUID, no error, just a collapsed `M`.

Both directions are silent. That is the right severity reading.

## Plan completeness

Good on the points that have burned this lineage:

- **Explicitly disclaims causation.** The plan states this cannot explain the
  observed `M mean=2.93`, because at the startup extent the value is already 2.
  That disclaimer is correct and important — three prior cycles were tempted to
  present a latent defect as an explanation of the live symptom. Endorsed.
- **Scope discipline.** Not touching Cornell (its `1.0f` literals at `:1592`/
  `:1645` are derived from a full-res call site and remain right), not making the
  GBuffer resizable, not switching to float division. All three exclusions are
  justified in the plan and I agree with each.
- **Keeps the `std::max(HalfResWidth, 1u)` guard.** Necessary: `HalfResWidth` is
  a member initialized to `0` (`:2826`) and only set in `CreateGBufferTextures`.

One gap, minor, not a FIX: the plan does not say what happens if a future change
*does* make the GBuffer resizable — at which point `WIDTH` becomes the wrong
operand and `FB.width` becomes right again. The comment the impler writes should
state the invariant ("the GBuffer is fixed-size; if that changes, this must
change with it") rather than just asserting `WIDTH` is correct. Fold into the
impl.

## Why this passes the v190 standing stop-condition

`PENDING_IMPL_REVIEW_v190.md:92-94` said the pipeline should stop if the next
cycle produces no functional change. This one changes two functional lines and
the change is argued entirely from source with no runtime claim. The condition is
met on its terms.

## Feedback for planner (FIX only)

None — KEEP. One instruction folded into the impl: the comment must state the
*invariant* (GBuffer is fixed-size, independent of the swapchain), not merely the
substitution.
