# Pending Commit v218

- plan: docs/PENDING_PLAN_v218.md
- plan_review: docs/PENDING_PLAN_REVIEW_v218.md (KEEP)
- files: **none — zero source files modified** (audit cycle)
- source: no bundle
- target: (uncommitted working tree — this pipeline does not commit)
- task: Pre-build compile-risk + tri-copy audit of cycles v200-v217
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: Two findings, both **carded rather than patched**, per v196/v208 — a patch inside an audit
  cycle makes the audit's own "zero source modified" row unverifiable. No `.cpp`, `.h`, `.hlsl`, `.py`,
  `.sh` or governance file touched.

## Plan Deviations

None. The plan gate's addition (card S) and correction (slot-position not name-equality) were both
folded in as instructed.

## RESULT: the v200-v217 delta is compile-coherent in both silent classes

### Check 1 — arity / symbol coherence (v209 deletion, v214 move)

| Query (scope `Engine/Source/Runtime`) | Count | Reading |
|---|---|---|
| `DummyDirectionTexture` | **0** | v209's deletion is complete — no dangling reference |
| `DummyDebugStatsTexture` | **5** | live sibling intact — **the control that makes the 0 meaningful** |
| `MaterialPlaceholderTexture` | 40 (ctx) | v214's move: created once at `FGIPass.cpp:190`, nulled `:218`, consumed `:684` |
| `RenderGBuffer(` shape | def `:2173` + call `:795`, **both zero-arg** | v197's arity change is coherent |

`FGIPass.cpp` `executeCommandList` → **2 hits, `:196` (Initialize) and `:440` (Shutdown)**; `waitForIdle`
→ `:197` (Initialize) and `:441` (Shutdown), plus a comment at `:177`. **Neither is in `DispatchRays`**,
so v214's functional intent — remove the per-frame stall — holds.

### Check 2 — ReSTIR cbuffer layout, four-way (v210 touched this header)

`FReSTIRSpatialConstants` agrees field-for-field and in order across all four expressions:
`FReSTIRPass.h:64-72`, the marshaller (`FReSTIRPass.cpp:634-637`, appending `GBufferScale` explicitly),
`TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl:16-27`, and
`TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:33-35`. **v184's rule holds** — the tail is plain scalars
(`GBufferScale`, `Pad`), no array. Temporal likewise: `FReSTIRPass.h:42-59` tail is
`NearPlane / FarPlane / GBufferScale` as three scalars, matching the marshaller's `:544-546`
(which names the constraint in-line) and the shader's `:24-30`. v210's header edit did not perturb
either tail.

### Check 3 — the `GuideScale` slot, three copies, by SLOT POSITION not name (plan-gate correction)

C++ writes it at `ConstantsData[5]` (`FBilateralDenoisePass.cpp:215`), i.e. **float slot 5**, after
`TexelSize`(0,1), `DepthSigma`(2), `NormalSigma`(3), `SpatialSigma`(4). All three HLSL copies place a
`float` at slot 5 with two `float` pads after:

| Copy | Slot-5 declaration | Consumes it? |
|---|---|---|
| `Shader/` (shared) | `:21 float GuideScale` | yes — `GB()` `:43-47` |
| `TestReSTIR_GI_Temporal_Data/` | `:21 float GuideScale` | yes — `GB()` `:35-39` |
| `TestCornellBoxGI_Data/` | `:26 float GuideScale_Unused` | **no, deliberately** — `:71`/`:102` index raw |

**A name-equality sweep would flag the third copy and be wrong** — that is precisely why the plan gate
required slot-position. The Cornell target dispatches at its guides' resolution, so its scale is
identically 1 and the identity map is correct; the field is named-but-unconsumed so the wire layout
stays legible. **Layout agrees in all three; behaviour differs by design, and the design is stated in
each file.**

### Check 4 — tri-copy divergence

`BilateralDenoise_cs.hlsl` → 3 copies. Each is compiled by its own `ShaderMake.cfg`
(`Shader/ShaderMake.cfg:5`, `TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:3`, and the Cornell equivalent),
and each consumer loads by the `DataDir` it passes to `Initialize` (`FBilateralDenoisePass.cpp:44`,
`:47-48`). The v182 hazard (edit landed only in the non-compiled copy) is **not present**: v204's `GB()`
work is in both copies that need it, and v205's `DepthTexture`-sourced derivation is C++-side only.

## Finding 1 — card S: the shared copy documents a mechanism with zero call sites

`Shader/BilateralDenoise_cs.hlsl:26-32` tells the reader `FCommonRenderPasses` selects this copy
"unless a consumer overrides the directory via `SetShaderDataDir()`". **`SetShaderDataDir` → 4 hits,
none of them a call**: definition `FCommonRenderPasses.cpp:290`, declaration `FCommonRenderPasses.h:97`,
and two comments (that shader's own, and `ShaderMakeBuild.py:453`). Copy selection actually happens
through the `DataDir` argument each consumer passes to `Initialize`. Not a defect — paths resolve
correctly by the other route — but the comment will be read exactly when someone is choosing which
copy to edit. **Carded, not patched.**

## Finding 2 — card T: v214's own `verify:` command is falsified by v214's own patch

`PENDING_COMMIT_v214.md:10` states the verification as: `waitForIdle` in `FGIPass.cpp` →
*"exactly 1 hit at line 415 (Shutdown). Pre-patch: 2 hits (415, 671)."*

Actual post-patch state: **2 code hits, `:197` and `:441`.** The move did not delete a `waitForIdle`; it
**relocated** one from the per-frame path into `Initialize`, which is the correct outcome and exactly
what the patch intended — but the marker's expected count was written as though the call had been
removed. The cited line numbers are also stale (`415` → `441`), consistent with v217's finding that
counts were invariant across cycles while line numbers were not.

**Why this matters more than a stale number:** an operator running v214's verify command on a correct
tree gets a mismatch and reads it as *the patch is not present*. That is a **false failure**, which
v192 established is worse than a false pass — a false pass wastes a cycle, a false failure sends
someone to re-patch code that is already right, in the one file whose per-frame path this lineage has
already broken and repaired once. **Carded, not patched**, because editing a closed cycle's marker
would rewrite the audit trail this pipeline runs on.

## What this cycle did NOT establish

That anything compiles, links, runs, renders or validates. `terminal` was probed this tick and refused
(`pending_approval` / `tirith:unknown` / `exit_code -1`), including the degenerate command `true`, so
the block is command-independent. The v183-v218 chain remains unbuilt. This audit **lowers the
probability** that the operator's first build fails in the two silent classes; it does not replace the
build.
