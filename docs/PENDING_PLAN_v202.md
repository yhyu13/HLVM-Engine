# Pending Plan v202

- task: Audit the SHARED-RUNTIME seam of the ReSTIR pass — the one seam v200/v201
  did not cover — and fix what it turns up.
- source: no bundle — direct source analysis
- approach: v200 audited the chain's compile risk; v201 audited the primary
  target's runtime-extent risk and returned structural immunity. Both audits
  scoped themselves to a **single .cpp plus "both HLSL copies"**. But
  `FReSTIRPass` is a **shared runtime class** consumed by two targets, and its
  generation shader has **two** copies whose *resource declarations differ*.
  Nothing in the lineage has ever audited a shared-runtime object against **all**
  of its consumers simultaneously. That is the seam this cycle takes.
- diff_estimate: to be determined by findings; expect small or zero
- skip_plan_review: no
- test_strategy: file-only structural verification; every claim re-derived by
  the tester from source with a controlled positive for each zero.
- risks:
  - The v182 dual-copy trap generalises: this seam has **four** ReSTIR HLSL
    copies (2 targets x {generate, temporal, spatial}), not two.
  - `TestCornellBoxGI.cpp` is the known-good control (`software-development-
    practices §Path-Tracing §rule 4`) and card L's precondition says do NOT
    modify it while the v183-v199 chain is unbuilt. If the finding lands in the
    control, the correct output is a CARD, not a patch.
  - Per v195: a card's/plan's description of code is evidence about its author.
    The plan-criticer must re-read every callee named here.

## Specific questions this cycle must answer

1. `FReSTIRConstants` (generation) is the only one of the three constant structs
   with **no** `GBufferScale`. Temporal and spatial both got one at v183 for the
   half-res-dispatch/full-res-GBuffer ratio. Does generation have the same
   mismatch, and if not, why not?
2. `GenerationLayoutSRV` declares `Texture_SRV(4)`. The two generation shader
   copies **disagree** on whether a t4 exists. What happens to the consumer that
   does not declare it?
3. `SpatialLayout` is the only ReSTIR layout still mixing SRVs and a UAV in one
   set. Generation and temporal were both split at v151 under bug-075. Is
   spatial's non-split a defect or is it safe, and on what grounds?
