# CRITIC REVIEW — Phase 7b Plan

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer  
**Verdict**: REVISE (2 issues)

---

## Concerns

### [CRITICAL] Read-After-Write Hazard in Ping-Pong

**Dimension**: Correctness  
**Summary**: The plan proposes:
1. Generation writes to `Reservoir0`
2. Temporal reads `Reservoir0` + `Reservoir0History`
3. Temporal writes merged back to `Reservoir0`

Step 3 writes to the same texture that was read in Step 2 **on the same command list without a barrier**. This is a RAW (read-after-write) hazard in NVRHI/Vulkan.  
**Suggestion**: The temporal pass must write to a **separate output texture** (e.g., `Reservoir0Merged`), then swap `Reservoir0Merged` with `Reservoir0History` after the pass.  

Correct flow:
1. Generation writes to `Reservoir0/1`
2. Temporal reads `Reservoir0/1` (current) + `Reservoir0/1History` (history)
3. Temporal writes to `Reservoir0/1Merged`
4. Swap `Reservoir0/1Merged` <-> `Reservoir0/1History`

For next frame:
- `Reservoir0/1` = fresh generation output
- `Reservoir0/1History` = merged result from previous frame

**Verdict**: Must fix.

### [MAJOR] Random Number Quality for Reservoir Selection

**Dimension**: Correctness  
**Summary**: The plan uses `hash(pixel + frame)` for random selection. If the hash has poor distribution or correlation between frames, the reservoir selection will be biased.  
**Suggestion**: Use a better hash:
```hlsl
float hash(uint2 p, uint frame) {
    uint n = p.x * 15843u + p.y * 23457u + frame * 14325u;
    n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU;
    return float(n) / float(0xFFFFFFFFU);
}
```
**Verdict**: Fix.

### [MINOR] No Visibility Check for History Samples

**Dimension**: Correctness  
**Summary**: The plan accepts history samples without checking if the reprojected sample is still visible. If an object moves behind another object, the history sample becomes invalid but we still merge it.  
**Suggestion**: For Phase 7b, document this as a known limitation. Add a simple depth difference check:
```hlsl
float histDepth = gDepth.SampleLevel(gPointSampler, historyUv, 0);
float depthDiff = abs(depth - histDepth);
bool historyValid = depthDiff < 0.01;
```
This is cheap and catches major occlusions.

**Verdict**: Add to plan.

### [MINOR] M Visualization May Be Too Subtle

**Dimension**: Testing  
**Summary**: M ranges from 1 to 30. Log-scaled visualization might be hard to read. Linear `M / MAX_M` is clearer.  
**Suggestion**: Use `debugGray = M_merged / MAX_M` for M visualization.  
**Verdict**: Fix.

### [NIT] `DispatchTemporal` vs Separate Class

**Dimension**: Architecture  
**Summary**: Extending `FReSTIRPass` with `DispatchTemporal` is fine, but if the temporal shader grows complex (spatial reuse in Phase 8), the class becomes bloated.  
**Suggestion**: Keep as `DispatchTemporal` for now. If Phase 8 adds spatial reuse, consider renaming to `DispatchSpatialTemporal` or splitting into `FReSTIRTemporalPass`.  
**Verdict**: Accept.

---

## Revised Plan Summary

1. **Add `Reservoir0Merged` and `Reservoir1Merged` textures** (separate from current/history)
2. **Temporal pass writes to `Merged`, reads from `Current` + `History`**
3. **After temporal pass, swap `Merged` <-> `History`**
4. **Add depth-difference validation** for history samples
5. **Use improved hash for random selection**
6. **Debug M vis**: linear `M / MAX_M`

**Confidence Score**: 9/10 after revision
