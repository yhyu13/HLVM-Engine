# Pending Plan v207

- task: card Q — evaluate `FGIPass` against the guide-extent contract v205/v206
  pinned for its two `PostProcess` siblings, and document the result at the
  `FGIPassDesc` declaration.
- source: no bundle — direct source analysis
- skip_plan_review: no
- diff_estimate: +2 / -2 functional, ~+18 comment (see "approach")

## approach

Card Q asks three things, in order: (1) does any `FGIPass` shader index a
guide-like SRV with a coordinate derived from something other than its own
dispatch extent; (2) if some reads go through `gbPixel` and others use raw
`pixel`, that is the v182 shape in the **production** path; (3) whichever way it
resolves, document the contract where the two siblings document theirs.

**Q1/Q2 answer: the guide reads are CLEAN, and this is established by
enumeration rather than by sampling.** `GBufferWorldPos` → 4 hits, `GBufferNormal`
→ 4, `GBufferMaterial` → 10; every hit is a banner comment, the `register`
declaration, or an indexed read, and **every indexed read goes through `gbPixel`**
— production `:501`, `:502`, `:503`, `:584`, the sky-unprojection `:524`, and the
four debug probes v182 already aligned (`:764/765/766/793`). Zero raw-`pixel`
guide reads. `gbScale`'s two operands are both pinned to the same compile-time
constant (numerator `UpdateViewConstants(WIDTH, HEIGHT)` `:784` after v195;
denominator `Desc.OutputWidth = HalfResWidth` `:815` ← `HalfW = W/2` ← `W = WIDTH`),
so the ratio is a fixed 2 and cannot drift. **So card Q's stated worry does not
obtain, and v182's production-path gap is closed by enumeration, not assumed.**

**But the enumeration surfaced a defect one register over, in the same class the
card is about.** The card framed the question as guide **reads**; the divergence
is in a guide-shaped **write**. `RWTexture2D<float4> OutputDirection : register(u2)`
is written at `:645` with the **raw dispatch coord**:

    OutputDirection[pixel] = float4(firstSampleDir, 1.0);

with the comment *"Done unconditionally so downstream reservoirs always have a
valid sample."* Raw `pixel` is correct **only while u2 is dispatch-sized**. The
primary consumer creates `DirectionTexture` at `HalfW, HalfH` (`:1678-1680`),
matching the half-res dispatch — fine. **The other consumer never supplies u2 at
all**, and `FGIPass.cpp:636-661` binds a **1x1 dummy** in that case:

    nvrhi::TextureHandle DirectionUAV = Desc.OutputDirection;
    if (DirectionUAV) { ...setTextureState... }
    else { ...create 1x1 "FGIPass.DummyDirection"...; DirectionUAV = DummyDirectionTexture; }
    SRVBuilder.SetTextureUAV(2, DirectionUAV);

`TestPathTraceGI.cpp` sets `Desc.OutputWidth = CurrentFBInfo.width` (`:438`) and
never assigns `OutputDirection` (**0 hits** in that file), so **the control
dispatches a full-window grid of threads that each store to a 1x1 UAV at their
own coordinate** — an out-of-bounds UAV store on every thread but one.

**The asymmetry with u1 is the proof this is an oversight, not a design.** u1 has
the identical 1x1-dummy fallback, and its shader write is guarded by a runtime
flag the C++ sets from exactly the condition that selects the real texture:
`Data.Params3[2] = (Desc.DebugStatsTexture && Desc.DebugBounceStats) ? 1.f : 0.f`
(`FGIPass.cpp:471`) gating `if (g_GI.Params3.z > 0.5f ...)` (`:830`). So when u1 is
a dummy, **nothing writes to it**. u2 took the same fallback and dropped the
guard — and the comment at `:643-644` shows the reasoning: "unconditionally" was
chosen for the *consumer that supplies the texture*, without considering the
consumer that doesn't.

**Remedy — the minimal correct one, and why not the alternatives.** Three options
were considered:

  (a) Guard the shader write behind a new constant, mirroring u1 exactly. Costs a
      cbuffer field, touches **both** shader copies (v182 dual-copy hazard), and
      changes the production path's instruction stream.
  (b) Size the dummy to the dispatch extent. Requires the dummy's creation to
      know `Desc.OutputWidth/Height`, making a per-frame-resizable dummy out of a
      one-time allocation, and would allocate a full-window RGBA32F texture for a
      consumer that discards it.
  (c) **Reuse the existing `Params5` tail as the guard.** Rejected: `Params5.x`
      is the debug mode, and `yzw` are documented "unused" — writing a semantic
      into a debug register couples the production path to `HLVM_RGI_DEBUG_VIS`.

**Chosen: (a), but with the guard sourced from a field that already exists.**
`Params3.w` is `LightCount` and `Params3.z` is the u1 guard; there is no free
slot in `Params3`. However **`Params5.y` is genuinely unused and `Params5` is
already marshalled every frame** — no struct grows, no `static_assert` moves.
Rather than overload the debug register, the honest choice given the constraints
is the one that changes least and cannot regress the production path:

**FINAL: bind the real output texture as the u2 fallback instead of a 1x1 dummy.**
`Desc.OutputTexture` is mandatory (bound unconditionally at `:603`) and already in
`UnorderedAccess` state.

**REVISED after plan review — the original justification here was false.** It read
"is always exactly the dispatch extent (that is what `OutputWidth/Height` mean)."
The codebase neither enforces nor structurally honours that. In the control,
`OutputTexture` is created at `WIDTH, HEIGHT` (`TestPathTraceGI.cpp:265-267`)
while `Desc.OutputWidth = CurrentFBInfo.width` (`:438`) — **two different
quantities**, equal only because that target sets `Resizable = false` with
`Extent = { WIDTH, HEIGHT }` (card J, v196). The primary consumer satisfies the
same relation by a **different** mechanism: `OutputTexture` at `HalfW, HalfH`
(`:1678-1680`) and `OutputWidth = HalfResWidth` (`:815`), both descending from
`W = WIDTH`. **Two consumers, two mechanisms, neither of them the field's
semantics.** The fix is in-bounds because both current consumers size the
mandatory UAV to their own dispatch extent — verified per consumer — and the
header comment must therefore state this as a **requirement on callers**, not as
an observation, so it is enforceable rather than incidentally true. The stray write then lands in-bounds on a
texture the non-supplying consumer overwrites at `:826` **later in the same
invocation** — `OutputDirection[pixel]` at `:645` precedes `Output[pixel]` at
`:826`, so the final value is unaffected. This is exactly the shape
`FReSTIRPass::DispatchGeneration` already uses for its optional t4
(`Desc.DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture`,
recorded in card M) — **an established in-repo idiom for this precise problem,
not an invention.** Both HLSL copies stay byte-unchanged, so the v182 hazard is
not engaged and the control's shader is untouched.

**Ordering hazard — RESOLVED at the plan gate, re-derived from the file.**
`return;` → 10 hits; only two are in `RayGen` scope: `:538` (no-geometry sky) and
`:923` (inside closest-hit, past `:838` where `RayGen` closes). `:538` returns
*before* `:645`, so on the sky path the stray write never executes; on every
other path `:645` precedes `:819`/`:826` unconditionally. Claim holds.

## files

- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` — u2 fallback (+2/-2)
- `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` — contract comment at the
  `FGIPassDesc` declaration, in the same place and form v205/v206 used (+~18)

## test_strategy

File-only verification. Rows must include: both HLSL copies byte-unchanged;
`DummyDirectionTexture` member set unchanged (the field stays — removing it is a
separate decision); every zero controlled by a same-shape positive; the returned
diff read in full (v203's near-miss); the `:645`-before-`:826` ordering claim
re-derived from the file, not from this plan.

**Added at the plan gate**: a row that enumerates **both** consumers'
`OutputTexture` extents **from their creation sites** and compares each against
that consumer's `OutputWidth` assignment — per consumer, not asserted from the
field name. This is v206's enumerate-from-the-creation-site procedure applied to
the very operand the fix depends on, which the first draft skipped.

## risks

- **The early-return path.** If `:645` is unreachable when `:537` returns, the
  "overwritten later" argument holds trivially for that path but the in-bounds
  claim still stands. Must be checked, not assumed.
- **`DummyDirectionTexture` becomes dead.** After this change nothing assigns it
  on the fallback path. Leaving the member is safe; deleting it is scope creep.
  Flag it as a follow-up card rather than absorbing it.
- **Cannot be built.** 25th consecutive unbuilt cycle. This is a 2-line C++
  change with no signature or layout effect, so the compile risk is minimal, but
  it is nonzero and must be stated in the marker rather than glossed.
