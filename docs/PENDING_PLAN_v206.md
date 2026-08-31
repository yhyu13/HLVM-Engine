# Pending Plan v206

- task: Card P — does `FReBLURPass` have the guide-extent relation v205 pinned in its sibling `FBilateralDenoisePass`?
- source: no bundle — direct source read
- planner: agent_1_planner (tick-552)
- timestamp: 2026-08-30

## approach

Card P asks two questions in order, and the second is conditional on the first:

1. Does `FReBLURPass` derive any scale from a guide at all, or does it index
   guides with the raw dispatch coord as the bilateral pass did before v204?
2. If it indexes raw, are its guides full-res under Phase D in the primary
   consumer? That would make it a **twelfth instance** of the extent class, and
   the first since v194 to sit on the acceptance path (`DenoisedTexture` →
   `AccumInput` → `DisplayTexture` → the `display` dump).

**Q1 is answered: it indexes raw, and derives no scale.** `GB(` → **0** hits in
`TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl`, controlled by **22** hits in the
same directory (`BilateralDenoise_cs.hlsl` 6, `ReSTIR_Spatial_cs.hlsl` 5,
`ReSTIR_Temporal_cs.hlsl` 5, plus declarations) — a same-shape positive in the
same query, so the zero is real and not a v205-row-18 false zero. Both guide
reads use the bare dispatch coord: `gDepth.Load(int3(dispatchThreadID.xy, 0))`
(`:157`) and `gNormalRoughness.Load(int3(dispatchThreadID.xy, 0))` (`:178`), plus
a neighbour pair at `:115`/`:116` clamped to `OutputSize - 1` (`:112`).
`FReBLURConstants` (`FReBLURPass.h:31-52`) has **no** scale field at all.

**So the whole cycle turns on Q2, and Q2 must be settled by enumerating the
extent of every operand at both call sites — not by analogy with the bilateral
pass.** The plan's expectation, stated in advance so the impler can falsify it:
the bilateral pass is half-res *because Phase D put it there* (`:892-893`
`HalfResWidth/HalfResHeight`), whereas ReBLUR runs **after** the Phase-D resolve
(`:1117` "half-res → full-res resolve"), so its input is already full-res and no
scale is needed. If that holds, ReBLUR is CLEAN and the correct output is a
documentation change, not a patch.

**The finding this cycle is likely to produce is not a defect but a contract
divergence.** v205 wrote into `FBilateralDenoisePass.h` that its guides need
**not** match `OutputWidth`, because half-res-over-full-res is exactly what
`GuideScale` exists for. If ReBLUR indexes raw, then its guides **must** match
`OutputWidth` exactly — the opposite contract, in a sibling class, in the same
directory, stated nowhere. That is a trap of precisely the shape the lineage has
hit three times (v193's tautological guard, v204's reassuring comment, v205's
optional guide): a reader who has just read v205's header and moves to ReBLUR
will carry the wrong invariant across.

## files

- Read-only: `Engine/Source/Runtime/Private/Renderer/PostProcess/FReBLURPass.cpp`,
  `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`,
  `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp`,
  both `ReBLUR_cs.hlsl` copies
- Expected to modify: `Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h`
  (comment only), **only if** Q2 returns CLEAN
- If Q2 returns DEFECTIVE: `TestReSTIR_GI_Temporal.cpp` extent substitution, and
  the header comment defers to a later cycle

## diff_estimate

+0 functional / +12 comment if CLEAN. If DEFECTIVE, revise before implementing —
do not carry this estimate into a patch.

## skip_plan_review: no

The cycle's conclusion is a **negative** ("no defect here"), and this lineage has
one recorded near-miss (v203 deleted three live binding items under a
"comment-only" banner). A negative conclusion plus a header edit is exactly the
combination that needs a second reader.

## test_strategy

File-only structural verification. Every row must carry a same-shape positive
control on a file known to contain the token, per audit row 18. Specifically:
each guide-extent claim must trace to the `createTexture`/`CreateTexture2D` call
that sizes it, not to a variable name.

## risks

1. **Assuming instead of enumerating.** Cards E, G, H and F each asserted
   something about a callee that dissolved on reading it. This plan asserts an
   expectation about Q2 above; the impler must enumerate, and is expected to
   contradict the plan if the enumeration says otherwise.
2. **The control has a swapchain-derived `OutputWidth`.** `TestCornellBoxGI.cpp`
   sets `ReBLURDesc.OutputWidth = CurrentFBInfo.width`. That looks like the
   v194 shape and the natural move is to substitute it. **Do not** — determine
   first whether that target's guides and output are recreated on resize, since
   if they are, the swapchain extent is the *correct* source there and a
   substitution would introduce the defect rather than remove it.
3. **The known-good control must not be edited.** v196 established this and
   v205 preserved it. Any change to `TestCornellBoxGI.cpp` this cycle would
   forfeit the provenance that exonerates driver/nvrhi/slangc for 23 unbuilt
   cycles.
4. **Dual-copy hazard (v182).** `ReBLUR_cs.hlsl` exists in two directories,
   verified by query, not assumed. The plan expects **neither** to be edited.
5. **`patch` anchoring (v203).** Anchor on a statement boundary, never on a
   comment adjacent to a braced initialiser, and read the returned diff.
