# Pending Commit v204

- plan: docs/PENDING_PLAN_v204.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl, Engine/Source/Runtime/Private/Renderer/PostProcess/FBilateralDenoisePass.cpp, Engine/Source/Runtime/Test/TestCornellBoxGI_Data/BilateralDenoise_cs.hlsl
- source: no bundle
- target: working tree (NOT committed — job instruction forbids commit/push)
- task: ELEVENTH instance of the Phase-D extent class — the bilateral denoise pass samples full-res GBuffer guides with half-res dispatch coordinates
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` then `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: no
- notes: shared pass class, touches the known-good control's compile path

## The sweep

Shared pass classes with two consumers, derived by query (not recall) over
`Engine/Source/Runtime/Test`:

| Class | Consumers | v202 invariant | v183 invariant |
|---|---|---|---|
| `FReSTIRPass` | primary + control | swept by v202/v203 (3 clean, 3 defective — cards M, N) | prior cycles |
| `FReBLURPass` | primary + control | **CLEAN** | **CLEAN** |
| `FBilateralDenoisePass` | primary + control | **CLEAN** | **DEFECTIVE** ← this cycle |
| `FGIPass` | primary only | N/A (not shared) | prior cycles |

`FReBLURPass` verified clean on both: the 8-item layout (b0, t0-t3, s0, s1, u0)
is declared identically in both consumers' `ReBLUR_cs.hlsl`, and its cbuffer
tail agrees between the C++ struct, the marshaller and both shader copies.
Recorded because a clean row on a class that *looks* like the defective one is
what makes the defective row informative.

## Root cause

`FBilateralDenoisePass` passes v202's invariant — both consumers declare b0,
t0, t1, t2, s0, u0, matching the layout exactly. The defect is invisible to
that check, because the divergence is not in *which* bindings are declared but
in *what extent* the bound textures have.

At `TestReSTIR_GI_Temporal.cpp:880-897` the primary consumer builds a descriptor
whose three inputs do not share one extent:

- `Bd.InputTexture = OutputTexture` — **half-res** (400x300), created at `:1675`
  from `HalfW = W / 2`
- `Bd.DepthTexture = LinearDepthTexture` — **full-res** (800x600) GBuffer MRT
- `Bd.NormalTexture = GBufferNormal` — **full-res** (800x600) GBuffer MRT
- `Bd.OutputWidth/Height = HalfResWidth/HalfResHeight` — **half-res**

`FBilateralDenoisePass::Dispatch` derives both the dispatch grid (`:248-249`)
and `TexelSize` (`:158-159`) from `OutputWidth/Height`, and the shader recovers
its bounds by inverting `TexelSize` (`:60`). So the kernel runs over a 400x300
grid — correct for the input and the output — and then indexed the two full-res
guides with that same raw `pixelCoord` (`:66-67`, `:97`, `:102`), sampling the
**top-left quadrant of the GBuffer at half stride**.

Consequence: for every one of the 25 taps in the 5x5 kernel, `wDepth` and
`wNormal` were computed from a surface geometrically unrelated to the radiance
sample being weighted. The bilateral filter did not degrade gracefully — it
became an edge-preserving filter preserving *the wrong edges*, blurring across
true geometric discontinuities and preserving imaginary ones.

## Why this instance is different from the ten before it

**It is the first HALF-FIX rather than an omission.** Every prior instance was
a site nobody revisited when Phase D landed. This one *was* revisited: the
comment at `:885-889` records v189 fixing this exact call site, changing
`OutputWidth/Height` from `FB.width/height` to `HalfResWidth/HalfResHeight` so
the dispatch would match the half-res *input*.

That fix was correct and incomplete. v189 reconciled the dispatch with the
input and the output — three of the four operands — and did not notice the
remaining two were full-res guides. The half-fix then **camouflaged the
defect**: the call site now carries an explicit, correct-sounding comment about
Phase-D resolution matching, which reads as though the extent question at this
site has already been settled.

This is the same camouflage mechanism v193 found in the accumulate pass, where
a guard keyed to the wrong extent survived an audit sweep by being present. Here
it is a *comment* keyed to a partial fix. **A site that documents a resolution
fix is not thereby cleared of resolution defects** — arguably it deserves more
scrutiny, not less, since the fix proves the extents differ.

## Fix applied

**1. `TestReSTIR_GI_Temporal_Data/BilateralDenoise_cs.hlsl`** — added a `GB()`
helper matching the centre-of-footprint form `Resolve_cs.hlsl:60` uses for these
same two textures, and routed **all four** guide loads through it. Verified by
query: `t_Depth.Load` → 2 hits, both `GB(...)`; `t_Normal.Load` → 2 hits, both
`GB(...)`.

**`t_Input.Load` deliberately NOT routed through `GB()`** — 2 hits, both left on
the raw coord. The input is half-res like the dispatch; remapping it would
reintroduce exactly the out-of-bounds read v189 fixed. This is the single most
important line of this patch to get right, and it is the one a mechanical
"route every Load through the helper" edit would have broken.

**2. `FBilateralDenoisePass.cpp`** — the scale is derived **inside the shared
Dispatch from the guide texture's own desc**, not added as a caller-supplied
`FDesc` field. Three reasons, and the choice is the substantive one in this
cycle:

- The control is **byte-unaffected by construction**: its guides and dispatch
  are the same extent, so `GuideW / outputW == 1` and `GB()` is the identity
  map. It is not "probably fine" — it is arithmetically pinned.
- A future consumer **cannot forget to set it**. Card G's ruling applies:
  a fallback that the caller must opt into is a fallback that will be missed.
- No header change, so no other consumer's compile is perturbed.

Written to `ConstantsData[5]`, the slot previously commented "Pad[5] remains
zero". Buffer is `float[64]` (256 bytes), so index 5 is far in bounds — the
v184 overflow class is not engaged. `ConstantsData[5]` → 1 hit in this file.

**3. `TestCornellBoxGI_Data/BilateralDenoise_cs.hlsl`** — the v182 dual-copy
check, run rather than assumed: this shader **does** exist in both directories.
The control's copy was already binary-compatible (its `Pad0` occupies the same
float-5 slot the shared C++ now writes), so no layout drift was possible. I
renamed the field `GuideScale_Unused` with a comment, **and deliberately did NOT
add `GB()` to it**: the control does not need the remap, and adding an unused
helper to the known-good control would modify its generated SPIR-V for no
behavioural reason.

## Plan Deviations

None. The plan said "sweep, then patch what it finds," and the plan-criticer's
required addition (sweep both invariants, derive the domain by query) is what
surfaced this defect — v202's invariant alone reports this pass clean.
