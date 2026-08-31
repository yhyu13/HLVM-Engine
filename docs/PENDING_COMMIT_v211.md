# Pending Commit v211

- plan: docs/PENDING_PLAN_v211.md
- files: Engine/Source/Runtime/Shader/BilateralDenoise_cs.hlsl
- source: no bundle
- target: no branch — working tree only (no commit per job instruction)
- task: Card T — bring the THIRD, STALE, COMPILED copy of
  `BilateralDenoise_cs.hlsl` into agreement with the primary (v204/v205 shape).
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
- skip_impl_review: no
- produces_test_files: no
- notes: **+6 / -6 functional, +23 comment**, 1 file. Two other copies
  byte-unchanged. No C++, no cbuffer size change, no binding, no signature.

## Change

Four `patch` calls, each anchored on **declaration or statement boundaries**,
never on a comment adjacent to a braced initialiser — the v203 near-miss
hazard, which is live here because the cbuffer *is* a braced list.

1. `float Pad0` → `float GuideScale` at slot 5 (in-place rename).
2. `GB()` helper added, byte-identical in body to the primary's, with
   `max(int(GuideScale), 1)` so an unfilled constant is the identity map.
3-4. Four guide reads routed through `GB()`, and converted from operator[] to
   `.Load(int3(...))` to match the primary's form.

## Verification performed (file-only)

1. **Every returned diff read in full**, not the line count asserted (v203).
   All four are the intended hunks and nothing else; no `-` line outside the
   six intended.
2. **Cbuffer integrity by SET, not count** (v191): `float` → 30 hits
   enumerated; slots read in place as `float2 TexelSize, DepthSigma,
   NormalSigma, SpatialSigma, GuideScale, Pad1, Pad2` — **8 floats before and
   after, same order, tail intact.** The v184/v200 rule (never displace the
   tail) is not engaged.
3. **`GB(` → 10 hits, enumerated as a set**: declaration `:43`, two doc
   references `:21`/`:40`, comment `:89`, and **exactly four call sites**
   `:90`, `:91`, `:121`, `:126`. Matches the primary's site set one-for-one.
4. **The over-reach risk closed by a controlled query, not by intent.**
   `pixelCoord\]` → **1 hit**, `:139 u_Output[pixelCoord]` — the output store,
   correctly on the raw coord. And `:133 t_Input[uint2(neighborPixel)]` is the
   neighbour input read, also correctly raw. **Neither dispatch-res resource
   was routed through `GB()`**, which was the one edit in scope capable of
   turning a latent defect into a live one.
5. **The two already-correct copies proven byte-unchanged**, not assumed:
   `Test/TestReSTIR_GI_Temporal_Data/` 131 lines / 4,872 bytes and
   `Test/TestCornellBoxGI_Data/` 120 lines / 4,246 bytes — identical to the
   pre-edit reads in this same tick.
6. **Every zero controlled by a same-shape positive** (v205): the pre-edit
   `GuideScale` → 0 in this file was controlled by `cbuffer` → 1 same file
   and by 3/2 hits in the sibling copies under the same query.

## Plan Deviations

**None.** The plan proposed the rename + helper + four call sites; the plan
gate upheld it and supplied two strengthenings rather than changes. Both are
carried below, as instructed.

## Carried from the plan review

**(a) The evidence is stronger than the plan argued.** The plan reasoned from
`ShaderMake.cfg:5` + `build.ninja:2372`, i.e. *intent to build*. The review
found `Engine/Source/Runtime/Shader/BilateralDenoise_cs.sblob` **on disk**
next to `HBAO_cs.sblob`, `JointBilateralUpsample_cs.sblob`, `BlitVS.sblob`,
`BlitPS.sblob` — evidence of a **completed** build. A stale-shape shader had
already been compiled to SPIR-V and was sitting in the tree as a loadable
default.

**(b) The framing correction is adopted, and it is the cycle's real lesson.**
The plan billed this as a *"seventh false-instrument mechanism."* It is not.
`search_files target=files` on the filename finds it immediately, and that
query is neither new nor exotic. The mechanism is **"nobody asked"**: v182
observed *two* copies of a *different* shader, and eleven subsequent cycles
(v192, v193, v195, v197, v207 among them) repeated "both copies" as an
inherited fact about the dual-copy hazard in general, without ever
re-deriving the cardinality for the file in front of them.

**Standing rule, as an extension of v195's**: *a marker's description of code
is evidence about its author, not about the code* — **and this applies to
cardinality claims inherited across cycles, including ones every subsequent
cycle repeated. Eleven repetitions of "both copies" did not make it two
copies.**

Corroboration that the three-copy state is an accident rather than a
convention: the sibling entries of the very same cfg are singletons —
`JointBilateralUpsample_cs.hlsl` → 1, `HBAO_cs.hlsl` → 1. This is **the only
duplicated file in that directory**, which defeats the natural objection that
shared shaders are routinely mirrored.

## Severity — stated without inflation

**Latent, not live. This cycle moves no pixel and clears no acceptance gate.**
`FBilateralDenoisePass::Initialize` loads the blob from its `InShaderDataDir`
argument (`:44`, `:48`) and both consumers pass their own data dir
(`TestReSTIR_GI_Temporal.cpp:537`, `TestCornellBoxGI.cpp:901`), so nothing
binds this blob today.

What the cycle removes is a defect with an unusually bad shape: it sits in the
**default** copy, in the **pre-v204** form, and its failure is silent by
construction — wrong guide texels give wrong bilateral weights, no VUID, no
error. A future consumer that simply omits `SetShaderDataDir` reintroduces
v204's defect **without changing a line of C++**, and no query shape in the
lineage's audit list would flag it, because a `GuideScale` sweep of the
*consumers* comes back clean — the consumers are clean.

The patch is an **identity transform at `GuideScale == 1`**, the only value any
consumer can produce today, so it cannot perturb the v183-v210 chain awaiting
its first build.

## What was NOT done

Not built, not compiled, not run, not validated, not viewed — `terminal` was
probed first-hand this tick and refused at the tool boundary. No commit, no
push, no governance file touched. `GIAccumulate_cs.hlsl`'s two copies are in
the same domain and were deliberately left for a separate cycle, so this
cycle's own three-copy enumeration stays verifiable.
