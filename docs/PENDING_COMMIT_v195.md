# Pending Commit v195

- plan: docs/PENDING_PLAN_v195.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (no branch — no commit performed, per job instruction)
- task: card H — source the camera/view constants and the GBuffer raster
  viewport from the fixed render-target extent, not the swapchain
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
  then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: no
- notes: **+2 / -2 functional, +39 comment.** No shader edited — neither copy of
  `GIPathTracing.hlsl` was touched, so the v182 dual-copy hazard is not engaged.
  No-op at the default 800x600, so the unbuilt v183-v194 chain is unperturbed.

## Sites changed

| Site | Was | Now |
|---|---|---|
| `Render()` | `UpdateViewConstants(FB.width, FB.height)` | `UpdateViewConstants(WIDTH, HEIGHT)` |
| `RenderGBuffer` raster viewport | `Vp(0, float(LastWidth), 0, float(LastHeight), ...)` | `Vp(0, float(WIDTH), 0, float(HEIGHT), ...)` |

## Complete candidate-set enumeration

`FB.width` → 13 hits, every one classified:

| Hits | Classification |
|---|---|
| 764, 886, 1047, 1052, 1108, 1136, 1205, 1277, 2352 | comments (v189/v191/v193/v194/v195 rationale) |
| 754, 756 | **resize detection** — must stay swapchain-derived (see below) |
| 796 | `RenderGBuffer(FB.width, FB.height)` — inert; the callee's signature is `RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)`, both parameter names commented out, so the call transmits nothing |
| 1327 | the **blit** — its destination genuinely is the swapchain; correct as-is |

**0 functional swapchain-extent uses remain outside the two legitimate ones.**

`LastWidth` / `LastHeight` → 5 hits each: the resize comparison (754), the two
assignments (756-757), the log field (2385→2408), and the declarations (2953-4).
The viewport was the fifth and is now gone.

## Why `LastWidth` itself was NOT substituted

This is the one place where the mechanical substitution the last five cycles
applied would have **introduced** a bug. `LastWidth` has two roles: it feeds the
viewport (wrong) and it is the resize-detection state at `:754` gating
`BindingCache.Clear()` (right). Substituting the variable would turn that
comparison into `WIDTH != WIDTH` — never true — leaving the binding cache stale
across a resize. So the *use* was changed, not the variable. Card I should note
this: an `HLVM_ENSURE(FB.width == WIDTH)` invariant would have been **wrong**
here too, since it would fire on a legitimate resize that the cache-clear is
designed to handle.

## Shadowing check — a stronger form than v194's

v194 cleared shadowing by positional ordering (declarations after the patch
site cannot be in scope before it). **That argument does not apply here** — the
three `const uint32_t W = WIDTH, H = HEIGHT;` declarations at 1617, 1743 and
1836 all precede `RenderGBuffer` at 2165. The correct grep-checkable argument
is *containment*, not order: the enclosing ranges are `Render()` (opens 746,
patch at 786) and `RenderGBuffer` (opens 2165, patch at 2354), and **no `W`/`H`
declaration falls inside either range** — the four `const uint32_t W = WIDTH`
hits are at 1617, 1743, 1836 and 2344 (the last being this cycle's own comment).
`RenderGBuffer`'s own parameters are commented out and so declare nothing.
`WIDTH`/`HEIGHT` have exactly one definition each (`:106`, `:107`).

Recording the correction because v194 promoted positional ordering as the
carry-forward rule and it is **not general** — it happened to hold there.
Containment-within-the-enclosing-range is the rule that holds in both cases.

## Plan Deviations

None. Both sites are as planned; nothing else was touched.
