# CRITIC REVIEW — Phase 9 Plan

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer  
**Verdict**: REVISE (1 issue)

---

## Concerns

### [MAJOR] Procedural Sky Could Overexpose Image

**Dimension**: Correctness  
**Summary**: Adding sky radiance to the miss shader will increase overall image brightness. If the sky intensity is too high, the image could blow out. The current direct lighting uses `g_GI.LightDir.w` as intensity — the sky should use a similar scale.  
**Suggestion**: Add a sky intensity parameter to `GIConstants` (or reuse `AmbientColor.w`). Start with low intensity (0.1-0.3) and scale up.  
**Verdict**: Add sky intensity control.

### [MINOR] Poisson Disk Pattern Needs Verification

**Dimension**: Correctness  
**Summary**: The 12-point Poisson disk pattern in the plan is from memory. We should use a well-known pattern to ensure good distribution.  
**Suggestion**: Use the standard 12-point Poisson disk from GPU Gems or similar reference.  
**Verdict**: Verify pattern.

### [MINOR] SpatialRadius Should Be In Pixels

**Dimension**: Clarity  
**Summary**: `SpatialRadius = 3.0` means 3 pixels. This is reasonable. But it should be documented clearly.  
**Suggestion**: Add comment: `// Pixel radius for neighbor search`.  
**Verdict**: Nit.

### [NIT] Environment Sampling Could Be Phase 10

**Dimension**: Scope  
**Summary**: Phase 9 already has two features. Environment sampling is a bigger visual change than Poisson disk.  
**Suggestion**: Keep both — they're independent and both simple.  
**Verdict**: Accept scope.

---

## Required Changes

1. Add `SkyIntensity` parameter to `GIConstants` in `FewBounceGI.hlsl`
2. Use well-verified 12-point Poisson disk pattern
3. Set default `SpatialRadius = 3.0` pixels

**Confidence Score**: 9/10 after revision
