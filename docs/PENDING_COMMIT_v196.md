# Pending Commit v196

- plan: docs/PENDING_PLAN_v196.md
- files: **none — no source file modified**
- source: no bundle
- target: no branch (no commit made; job instruction forbids committing)
- task: Card J determination — `TestPathTraceGI.cpp` is NOT a tenth instance
- verify: `search_files path=Engine/Source/Runtime/Test pattern="CurrentFBInfo"` → 12 hits, all classified below; `Resizable` in `TestPathTraceGI.cpp` → `false`
- skip_impl_review: no
- produces_test_files: no
- notes: **This is the first cycle in the lineage to close a card with zero source change.** That is the deliverable, not a shortfall — see the plan review's ruling on the known-good control. The impler's substantive work was the enumeration below.

## What was implemented

Nothing in source. Per `PENDING_PLAN_REVIEW_v196.md §Feedback for planner` note
1: *"If the impler finds itself editing `TestPathTraceGI.cpp`, the cycle has
drifted."* I did not. `TestPathTraceGI.cpp` and every other source file in the
tree are **byte-unchanged this cycle**.

## Card J — determination, with the complete candidate set

`CurrentFBInfo` in `TestPathTraceGI.cpp` — **12 hits, every one classified.**
The set is closed; there is no thirteenth site.

| Site (symbol) | Consumer | Verdict |
|---|---|---|
| resize-detect compare | `FB != LastWidth/LastHeight` | correct — detection must use the live extent |
| `LastWidth`/`LastHeight` assign | resize state | correct — same reason (v195 precedent) |
| `UpdateViewConstants(...)` | aspect, `RenderTargetSize`, viewport | **benign here** — see ratio analysis |
| `FillGBufferTextures(...)` | CPU GBuffer staging | benign — double-guarded, below |
| `Desc.OutputWidth`/`OutputHeight` | RT dispatch extent | **benign — this is the denominator** |
| `AccumConstantsData.Width`/`Height` | accumulate shader guard | benign — matches its own grid |
| accumulate `dispatchX`/`dispatchY` | accumulate grid | benign — matches its own guard |
| blit destination extent | `BlitTexture(...)` | **correct** — genuinely should follow the window |

**The ratio.** `gbScale = RenderTargetSize / DispatchRaysDimensions()`.
Numerator ← `UpdateViewConstants` ← `CurrentFBInfo`. Denominator ←
`Desc.OutputWidth` ← `CurrentFBInfo`, passed verbatim through
`FGIPass::DispatchRays` into `RTPipeline.DispatchRays`. **Both operands are the
same quantity, so the ratio is identically 1 at every window size.** Card J's
first branch. This is the structural inverse of v195, whose denominator was
pinned to `HalfW = W / 2` and could not follow.

**The reachability.** `WindowProps.Resizable = false` with `WindowProps.Extent =
{ WIDTH, HEIGHT }` (contrast: the sibling sets `Resizable = true`). The
swapchain extent is pinned at creation and equals the fixed constants by
construction, so every swapchain-derived quantity above is a compile-time-equal
alias.

**Second, independent guard**, which the plan review found and I confirm:
`FillGBufferTextures` early-returns on `bGBufferFilled`, so the `W * sizeof(float)
* 4` row-pitch `writeTexture` executes exactly once and can never re-run at a
different extent even hypothetically.

**Verdict: card J is not a defect. No patch. Card closed.**

## Plan Deviations

**One deviation, and it produced the cycle's only net-new finding.**

The plan scoped this cycle to `TestPathTraceGI.cpp` alone. Because the
determination yielded no patch, I spent the remaining budget on re-scoped
**card I**'s actual question — whether the `FB.width` enumeration in the *primary*
target is now closed — rather than ending the cycle early. This is a scope
addition, not a substitution; nothing the plan asked for was skipped.

**Finding: `RenderGBuffer(FB.width, FB.height)` is a live call passing
swapchain-derived arguments into a function that no longer uses them.** v195
substituted the viewport construction inside the callee to `WIDTH`/`HEIGHT` and
in doing so left the signature as `void RenderGBuffer(uint32_t /*W*/, uint32_t
/*H*/)` — **both parameters commented out, i.e. unused.** So the call site is
harmless *today*, but it is a loaded gun: the argument list still reads as
though the raster extent follows the swapchain, and any future edit that
un-comments a parameter silently reintroduces the v195 defect in the pass v195
just fixed.

**I did not patch it.** Two reasons: (a) it is not a defect, so it does not meet
the bar for a functional change, and the honest fix is a signature change
(dropping the parameters entirely) which is a real edit to the acceptance-path
file with the v183-v195 chain still unbuilt; (b) bundling it would make this
cycle's "zero source files modified" row — the row that proves the known-good
control was not perturbed — unverifiable. **Carded as K.**

This also gives card I a partial answer: the `FB.width` enumeration in the
primary target is **not** clean, but its one surviving live site is inert. The
enumeration's remaining hits are the deliberate exclusions (resize detect,
`LastWidth` assign, blit destination) plus v191-v195 comment prose.
