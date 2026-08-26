# CRITIC REVIEW — Phase 7b Implementation

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer  
**Verdict**: APPROVE with minor notes

---

## Concerns

### [MINOR] PrevViewProj Updated in Two Places

**Dimension**: Architecture  
**Summary**: `PrevViewProj = currViewProj` exists in both ReSTIR and ReBLUR blocks. Redundant but harmless since both compute the same `currViewProj`.  
**Suggestion**: Could hoist the update to after both blocks, but not worth the refactoring risk.  
**Verdict**: Accept.

### [MINOR] No Motion Vectors for Reprojection

**Dimension**: Correctness  
**Summary**: Reprojection uses depth + camera matrices only. Moving objects will cause ghosting because their motion isn't accounted for.  
**Suggestion**: Document as known limitation for Phase 7b. Motion vectors require GBuffer PS modification.  
**Verdict**: Accept with documentation.

### [MINOR] Depth Validation Threshold is Hardcoded

**Dimension**: Maintainability  
**Summary**: `depthDiff > 0.05` is a magic number. Different scenes may need different thresholds.  
**Suggestion**: Make it a constant or pass it via the constant buffer.  
**Verdict**: Nit. Defer to Phase 8.

### [NIT] First Frame Uses Identity as PrevViewProj

**Dimension**: Correctness  
**Summary**: On frame 1, `PrevViewProj` is identity. Reprojection yields invalid UVs, so `historyValid = false`. This is actually correct behavior (no history on first frame).  
**Verdict**: Accept. Works as intended.

---

## Verified Checklist

- [x] Compiles without errors
- [x] No RAW hazards (temporal writes to separate `Merged` texture)
- [x] Proper texture state transitions
- [x] Ping-pong swap logic verified
- [x] Test passes (9.07s / 8.96s)
- [x] Sky/invalid pixels handled (M=0)
- [x] M cap applied (MAX_M = 30)
- [x] Random hash uses frame index for decorrelation

---

**Confidence Score**: 9/10
