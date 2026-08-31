# Pending Commit v205

- plan: docs/PENDING_PLAN_v205.md
- files: Engine/Source/Runtime/Private/Renderer/PostProcess/FBilateralDenoisePass.cpp, Engine/Source/Runtime/Public/Renderer/PostProcess/FBilateralDenoisePass.h
- source: no bundle
- target: working tree (NOT committed — job instruction forbids commit/push)
- task: card O — and the card described the wrong defect
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` then `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: no
- notes: 2 functional lines; +21 comment. Header is included by the known-good control.

## The card was right that an invariant was missing, and wrong about where the risk was

Card O described a **latent** invariant: `GuideScale` derived from one guide and
applied to both, safe today because both consumers' guides share an extent, and
dangerous only if some future consumer passes mismatched guides. That is true.
It is also the less serious half.

The scale was derived from `Desc.NormalTexture`, and **that guide is optional**:

- `FBilateralDenoisePass.h:27` — "Normals for edge detection (optional)"
- `FBilateralDenoisePass.cpp:90` — layout comment, "t2 -> 2 (Normal guide - optional)"
- `Dispatch` guarded it with `if (Desc.NormalTexture)`; `Desc.DepthTexture` has
  **no** such guard anywhere in the class

So no future consumer had to do anything unusual to break it. A consumer that
simply declines the optional guide — which the API invites — falls to the
`else`, gets `GuideScale = 1.0f`, and its **depth** guide is then indexed with
the identity map while still being full-res. That is v204's defect restored in
full, by the branch that appears to be handling the null case. Same camouflage
mechanism as v193's tautological guard and v204's own reassuring comment: the
code that looks like the safety measure is the failure.

The optional-guide path is a live affordance in this codebase, not a
hypothetical: `FReBLURPass.cpp:290-291` manufactures dummy depth and normal
textures via `EnsureDummyTexture` for exactly that reason.

## Fix applied

**1. `FBilateralDenoisePass.cpp`** — source the scale from `Desc.DepthTexture`,
the guide that is always present. **+2/-2 functional**, +8 comment.

Post-patch, re-derived by query rather than asserted:

- `Desc.NormalTexture` → **1** hit (`:199`), the binding-set item alone; the
  extent-deriving read is gone. Before the patch it was 3.
- `Desc.DepthTexture` → **3** hits: the guard `:183`, the extent read `:185`,
  the binding-set item `:198`.
- `ConstantsData\[5\]` → 1 hit, `:189`, into a `float[64]`. Slot unchanged, so
  no cbuffer layout drift and **neither HLSL copy needs editing** — the v182
  dual-copy hazard is not engaged and both shader files are byte-unchanged.

**2. `FBilateralDenoisePass.h`** — the invariant documented at the `FDesc`
declaration, which was card O's own preferred remedy, now stated truthfully
because the operand it depends on is the mandatory one. Comment-only, +11.
The comment also records what the invariant does **not** require: the guides
need not match `OutputWidth/Height`, since half-res-over-full-res is precisely
what `GuideScale` exists for. Without that sentence a future reader could
"fix" the invariant by forcing the guides to the dispatch extent and undo v204.

Card O's second option — a per-guide scale — was **not** taken. It costs a
second cbuffer float and a second `GB()` variant for a case no consumer has,
and it cannot be verified by any consumer in the tree today. Documenting the
invariant while pinning the derivation to the mandatory guide removes the
realistic failure and leaves the unrealistic one stated.

## No-op for the known-good control, by derivation

Per v204's new row 17, assignment sets closed rather than names compared:

- control: `GBufferDepthTexture` ← `:1199`, `GBufferNormalsTexture` ← `:1187`,
  both from the same `Desc` block; both recreated together at resize
- primary: `LinearDepthTexture` ← `:1655`/`:1664` from `W, H`;
  `GBufferNormal` ← `:1630` from `WpDesc` (`:1620`, `W, H`)

Both consumers' two guides are equal in width by derivation at every extent, so
`GuideW` is the same number whichever guide it is read from and the swap cannot
change a single dispatched value at any current call site. `TestCornellBoxGI`
is unperturbed; both `BilateralDenoise_cs.hlsl` copies are byte-unchanged.

## Plan Deviations

None. The plan instructed the impler to test the card's framing before
accepting it, and the framing did not survive.
