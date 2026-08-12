# CRITIC REVIEW — Phase 7a Implementation

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer  
**Verdict**: REVISE (Minor improvements needed)

---

## Concerns

### [MAJOR] Unused Sampler Bound to Compute Shader

**Dimension**: Correctness / Efficiency  
**Summary**: `FReSTIRPass.cpp` creates a `PointSampler` and binds it to the compute pipeline, but `ReSTIR_Generate_cs.hlsl` uses `.Load()` for all texture reads, which does not use a sampler. This wastes a binding slot and may trigger SPIR-V validation warnings about unused bindings.  
**Suggestion**: Remove `SamplerState gPointSampler` from HLSL and remove `PointSampler` creation/binding from C++.  
**Verdict**: Fix.

### [MAJOR] Unnecessary Debug UAV Write Every Pixel

**Dimension**: Efficiency  
**Summary**: The shader writes `float4(0,0,0,0)` to `gDebugOutput` for every pixel when `DebugVis <= 0.5`. This is wasted bandwidth — ~2.4 MB/frame written for no reason at 800x600 RGBA16F.  
**Suggestion**: Remove the `else` branch. Only write to `gDebugOutput` when `DebugVis > 0.5`.  
**Verdict**: Fix.

### [MINOR] `ReadBinaryFile` Duplicated Across 3 Files

**Dimension**: Maintainability  
**Summary**: `FReSTIRPass.cpp`, `FReBLURPass.cpp`, and `FBilateralDenoisePass.cpp` all contain an identical `ReadBinaryFile` helper.  
**Suggestion**: Extract to a common utility (e.g., `Engine/Source/Runtime/Private/Renderer/ShaderMake/ShaderBlob.cpp` already has blob loading — use that).  
**Verdict**: Acknowledge. Out of scope for this phase.

### [MINOR] DebugVis is a Static Bool, Not a CVAR

**Dimension**: Observability  
**Summary**: `g_ReSTIRDebugVis` is a `static bool` that cannot be toggled at runtime.  
**Suggestion**: For a test executable this is acceptable, but document the limitation. A proper CVAR would require `AUTO_CVAR_BOOL` which doesn't link cleanly in test targets.  
**Verdict**: Accept with documentation.

### [NIT] Constant Buffer is 256 bytes but struct is 32 bytes

**Dimension**: Efficiency  
**Summary**: We allocate 256 bytes but only use ~32 bytes. This is harmless and actually required by some drivers for constant buffer alignment.  
**Suggestion**: No change needed. 256-byte CB is standard practice.  
**Verdict**: Accept.

---

## Required Changes

1. Remove unused sampler from HLSL and C++
2. Remove unnecessary else-branch UAV write in HLSL

---

**Confidence Score**: 8/10 after fixes
