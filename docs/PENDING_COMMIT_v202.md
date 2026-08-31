# Pending Commit v202

- plan: docs/PENDING_PLAN_v202.md
- files: Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp
- source: no bundle — direct source analysis
- target: (no branch — no commit made, per job instruction)
- task: Audit the shared-runtime seam of FReSTIRPass against BOTH of its
  consumers; document two structural immunities and card the one real defect.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: **Comment-only change. +46 lines, 0 functional lines, one file.**
  Zero `.hlsl` touched, so the v182 dual-copy hazard is not engaged. The
  known-good control is byte-unchanged, so v196's control-protection row holds.
  (Initially +66; tightened to +46 in a follow-up editing pass with no loss of
  load-bearing content — each block still states the finding, the evidence, and
  the condition under which its reasoning would lapse.)

## What changed

Three comment blocks in the SHARED runtime file — deliberately not in either
consumer:

| Site | Subject | Verdict recorded |
|---|---|---|
| `GenerationLayoutSRV` bindings | shared layout vs divergent shader copies | **DEFECT — carded M, not patched** |
| `DispatchGeneration` binding set | why generation needs no `GBufferScale` | safe, structural |
| `SpatialLayout` bindings | why the SRV/UAV split is absent here | safe, structural |

## Findings

**1. The real defect: a shared binding layout against divergent shaders.**
`GenerationLayoutSRV` declares `Texture_SRV(4)` unconditionally, but only the
primary target's `ReSTIR_Generate_cs.hlsl` declares a `t4` (`gDirection`, `:37`,
read at `:71`). The `TestCornellBoxGI_Data` copy declares `t0..t3` only —
`gDirection` → 0 hits there. The control's pipeline is thus built from a layout
advertising a binding its SPIR-V lacks. Latent rather than fatal only because
`:382`'s ternary substitutes `RadianceTexture`; the control never sets
`DirectionTexture` (0 hits; its `GenDesc` block assigns six fields).

This is the **general form** of the v182 dual-copy hazard, and the generalisation
is why nothing caught it: v182 was "one shader, two places, identical content —
don't edit just one." Here the copies are *supposed* to differ and the binder is
shared, so any check phrased as "are the copies identical?" returns clean. The
correct invariant: **for a shared binding layout, every consumer's shader must
declare every binding the layout declares.**

**2. Generation is immune to the v183 half-res class, structurally.** It reads no
GBuffer texture at all: `gWorldPos`/`gNormals`/`gDepth` return exactly 1 hit each
(the declaration) in **both** copies, with no `.Load`/`.Sample` anywhere; `main`
(`:62-78`, read in full) touches only `gRadiance` and `gDirection`. Both live
inputs are half-res (`:1675-1680`) and the dispatch is half-res (`:911-912`), so
extents already agree. **No `GBufferScale` should be added here.**

**3. Spatial's unsplit layout is safe, structurally.** bug-075 needs a texture
reachable as both SRV and UAV in one dispatch. At `:1083-1096` the five SRVs are
`OutputTexture`, a parity-selected `TemporalReservoir` pair, `GBufferNormal` and
`LinearDepthTexture`; the lone UAV is `SpatialRadiance`, in no SRV slot. No alias
is possible. `00344` → 0 hits across all retained logs.

## Plan Deviations

**One, and it is the plan gate's instruction rather than my own choice.** The
plan's question 1 was framed as "does generation need a `GBufferScale`?", which
invites adding one. The plan-criticer established it must not be added and that
the immunity should be *documented* instead. I followed the review, not the plan.

Second, the plan left open whether to split `SpatialLayout`; the review resolved
it to "safe, and record why." No split was made.

Third — and this is the one a reviewer should scrutinise — **I did not fix the
defect I found.** It lives in `TestCornellBoxGI_Data/`, the known-good control.
Card L's precondition and v196's precedent both say do not modify the control
while the v183-v199 chain is unbuilt. Carded as M.

## LSP note — read before treating it as a regression

Both `patch` calls reported `ERROR [N:10] Use of undeclared identifier
'FReSTIRPass'`, at line 331 then 361. **It is a stale-index artifact, not a
regression.** The cited line tracks `DispatchGeneration`'s definition as my
comment insertions push it down (331 → 361 → now 381); the token under it never
changed. Verified after the edits: `namespace ReSTIR` opens `:39-40` and closes
`:658`; all five member definitions are present and qualified (`Initialize` :41,
`DispatchGeneration` :381, `DispatchTemporal` :470, `DispatchSpatial` :578,
`Shutdown` :639). A comment-only diff cannot undeclare an identifier.

**This is unverified by compilation** — see the verification status in the audit.
