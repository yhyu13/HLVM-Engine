# Pending Impl Review v185

- plan: docs/PENDING_PLAN_v185.md
- commit: docs/PENDING_COMMIT_v185.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-532)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly: 8 assignment lines across the two named
blocks, `FB.width/height` → `HalfResWidth/HalfResHeight`, one file, no shader
edits. **No `## Plan Deviations` section is present, and I confirm none was
required** — nothing in the plan proved infeasible.

The plan-criticer issued one binding constraint: do not touch spatial
(`:1042-1046`) or `TestCornellBoxGI.cpp`. **Both honoured, verified by direct
read rather than by trusting the commit marker.** Spatial reads
`float(HalfResWidth)` and carries its original `// Phase D` comment, not a
v185 one — it was not rewritten. Cornell: 0 edits.

## Correctness re-derivation (not inherited)

I re-checked the post-patch state at the actual shifted line numbers, since
the patch moved everything below `:874` by ~23 lines and a stale reference
would be easy to carry forward.

**Three-way consistency now holds.** `pattern="HalfResHeight"` → 14 hits.
The three constant blocks each pair a `Desc.OutputHeight = HalfResHeight`
with a matching `OutputSize[1] = float(HalfResHeight)`:

| pass | dispatch grid | constant | agree? |
|---|---|---|---|
| generation | `:872` | `:883` / `:885` | yes |
| temporal | `:959` | `:984` / `:986` | yes |
| spatial | `:1040` | `:1044` / `:1046` | yes (pre-existing) |

Before the patch, rows 1 and 2 disagreed with their own dispatch grids.

**The most important negative check: `GBufferScale` was not disturbed.**
This is where a careless "replace FB.width with HalfResWidth" sweep would
have done real damage. `:1005` and `:1051` still read
`FB.width / std::max(HalfResWidth, 1u)` — the numerator there is *supposed*
to be full-res, because the quantity is the ratio between the full-res
GBuffer and the half-res grid. Had the impler globally substituted, the ratio
would have collapsed to 1 and silently re-inerted v183's fix, reproducing the
exact failure v184 had just repaired. Both sites intact; the impler correctly
changed only the four-line constant blocks.

**Surviving `FB.width` uses are all correct**: `:754-757` resize detection,
`:764` `UpdateViewConstants(FB.width, FB.height)` (the raster/view path is
genuinely full-res), and the two `GBufferScale` numerators.

## Security scan

- [x] No hardcoded secrets — numeric/identifier substitution only
- [x] No shell injection — no process invocation added
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] **Validation:** `HalfResWidth/Height` are set at `:1562-1563` inside
      texture creation, which runs before the render loop; the values are
      also the source of every reservoir texture's dimensions
      (`:1575-1594`), so the constants and the textures they index cannot
      diverge. Division uses `float(...)` on a non-zero uint (`W/2`).
- [x] **Error handling:** unchanged. `FReSTIRPass::DispatchTemporal:418-422`
      still guards zero dimensions and warns.
- [x] **Tests:** `produces_test_files: no` is accurate — no path under a test
      *fixture* directory was added; the edited file is an existing engine
      test source. Per HARD INVARIANT #2 I ran anyway (`skip_impl_review:
      no`), which is the correct setting for a production-path change.

## Where I disagree with the commit marker's emphasis

The marker calls the generation change "inert today." I verified that and it
is true (`ReSTIR_Generate_cs.hlsl` uses `OutputSize` only for the `:58-60`
early-out; `RcpOutputSize` is never read) — but it should be stated more
sharply: **only the temporal block can change a rendered pixel.** If the
operator run shows no change in `M mean`, nobody should be able to point at
the generation edit as a confound. It cannot be one.

## Load-bearing caveat

Not compiled, not run. A C++ change this small is unlikely to fail to build,
but "unlikely" is not "verified", and I am the same model that wrote the
patch — this KEEP is a self-check, not fresh eyes
(`six-role-pipeline §Anti-patterns §7`). The claim that history acceptance
improves is a **prediction**, not a result.

## Feedback for impler

None. KEEP.
