# Pending Plan v210

- task: Card S — `FReSTIRPass`'s guide-extent contract is the THIRD distinct
  contract among three sibling denoise/reuse classes, and it is the only one
  undocumented. Re-derived from source (Rule 9 with no unblocked card).
- source: no bundle — direct edit
- approach: v205 documented `FBilateralDenoisePass`'s contract (guides need
  NOT match the dispatch extent; the class derives a `GuideScale` for you).
  v206 documented `FReBLURPass`'s **inverse** contract (guides MUST match the
  dispatch extent; the class indexes raw) precisely because the documented one
  actively misleads about the undocumented one. `FReSTIRPass` holds a **third**
  contract that neither header describes and that no cycle has written down:
  guides need not match the dispatch extent, **but the caller must compute and
  supply the ratio itself** via `FReSTIRTemporalConstants::GBufferScale` /
  `FReSTIRSpatialConstants::GBufferScale`. Document it at the two `FDesc`
  declarations that carry the guides, in the same place and form v205/v206 used.
- diff_estimate: +0 functional / +~22 comment, 1 file
- skip_plan_review: no
- test_strategy: file-only. Verify the three contracts are as stated by reading
  each class's guide read-site; verify the two existing headers' comments; verify
  the new comment's every factual claim against the marshaller and the shader.
- risks:
  1. **The natural wrong move is a functional patch.** Two of the three
     classes derive the scale inside `Dispatch` from `getDesc().width`.
     "Making FReSTIRPass consistent" by doing the same there is a real
     temptation and would be wrong — see the plan-criticer question below.
  2. Comment-only diffs have exactly one realistic failure mode, and v203
     produced it: an `old_string` anchored on a comment adjacent to a braced
     initialiser matched INTO the list and deleted three live binding items.
     **Anchor on a statement/declaration boundary, and read the returned diff.**
  3. `FReSTIRPass.h` is 169 lines. Per tick-555's post-cycle correction, read
     it END-TO-END before adding a comment — redundancy is a whole-file
     property and a paginated read cannot judge it.

## Evidence, re-derived this cycle (not quoted from any marker)

| Class | Guide read | Scale source | Contract |
|---|---|---|---|
| `FBilateralDenoisePass` | scaled | **callee** derives it: `Desc.DepthTexture->getDesc().width / outputW` (`.cpp:185-187`) | guides free |
| `FReBLURPass` | raw (`gDepth.Load(dispatchThreadID.xy)`) | none — no scale field exists | guides MUST match |
| `FReSTIRPass` | scaled via `GB()` (`ReSTIR_Temporal_cs.hlsl:78-82`) | **CALLER** supplies `GBufferScale` | guides free, caller computes |

- `getDesc` in `FReSTIRPass.cpp` → **3 hits**, all `OutReservoir0`/`OutRadiance`
  (the dispatch grid), **none on a guide**. So the class cannot derive the
  ratio and does not try.
- `GBufferScale` in the primary consumer → `TC.GBufferScale` `:1061`,
  `SC.GBufferScale` `:1109`, both `WIDTH / max(HalfResWidth,1u)`.
- `GBufferScale` in the control → `1.0f` at `:1592` and `:1645`, correct
  because that target is not half-res.
- Marshaller: `:546` and `:637` append it field-by-field, each with a comment
  saying it "stays zero" otherwise.

## The question this plan puts to the plan-criticer

**Should the remedy be a comment, or should `FReSTIRPass::Dispatch*` derive
`GBufferScale` itself from the guide, the way `FBilateralDenoisePass` does?**

I propose **comment only**, and I want the gate to rule on it rather than
accept it, because "make the three siblings consistent" is the more natural
instinct and I may be rationalising the cheaper option. My reason: the
bilateral pass takes ONE guide extent and one output extent, so the ratio is
recoverable inside the callee. `FReSTIRPass` is handed six-plus textures per
dispatch spanning **both** extents (half-res reservoirs AND full-res GBuffer
guides in the same `FDesc`), so a callee-derived ratio would have to pick a
guide to trust — reintroducing exactly the "derived from the optional operand"
defect v205 removed. **Verify that claim against `FTemporalDesc`/`FSpatialDesc`
before ruling.**
