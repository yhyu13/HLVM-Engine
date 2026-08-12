# CRITIC REVIEW — Phase 8 Plan

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer  
**Verdict**: REVISE (2 issues)

---

## Concerns

### [CRITICAL] Shading Approach is Under-Defined

**Dimension**: Correctness  
**Summary**: The plan proposes reading `DenoisedHDRTexture[winnerPixel].rgb` as the output radiance. But if the winner is a neighbor pixel, we're just copying the neighbor's radiance to the center pixel. This is **not** ReSTIR — it's just a fancy nearest-neighbor filter. The whole point of ReSTIR is that `W` encodes the weighted average contribution of ALL merged samples, not just the winner.

For a proper simplified ReSTIR with `p(y)=1`:
```
output = (1/M) * sum(sample_radiance_i * weight_i / p(y_i))
        = (1/M) * sum(sample_radiance_i * luminance_i)
```

But this requires storing radiance in the reservoir, which we don't.

**Alternative (practical for Phase 8)**: Treat the spatial merge as a **confidence-weighted blend**:
```hlsl
float3 output = centerRadiance;
// If spatial merge increased M significantly, the center radiance is "confirmed" by neighbors
// No color mixing — just use center color with confidence from W
```

This is less theoretically correct but avoids color bleeding and is easy to implement.

**Better Alternative**: Store `luminance` and a `dominant direction` or just accept that Phase 8 is about validating the spatial merge infrastructure, not perfect shading. Output `W_merged` as a scalar field and visualize it. The actual color output can remain `centerRadiance`.

**Verdict**: Revise. Phase 8 should focus on infrastructure validation, not color mixing. Output `W_merged` as confidence and keep `centerRadiance` as color.

### [MAJOR] Reading DenoisedHDRTexture at Neighbor Pixel Requires Sampler

**Dimension**: Correctness / Architecture  
**Summary**: The plan uses `DenoisedHDRTexture[neighborPixel].rgb` for winner evaluation. But `neighborPixel` is computed as an integer offset from center. Using `Load()` with integer coords is fine. However, the neighbor might be out of bounds at screen edges.

Also, using `Load()` means we don't need a sampler — but the plan shows `gRadiance` as `Texture2D`. We need to make sure we use `Load()` not `Sample()`.

**Suggestion**: Use `Load()` with bounds checking:
```hlsl
int2 neighborPixel = centerPixel + int2(dx, dy);
if (any(neighborPixel < 0) || any(neighborPixel >= outputSize)) continue;
float3 neighborRadiance = gRadiance.Load(int3(neighborPixel, 0)).rgb;
```

**Verdict**: Fix in implementation.

### [MINOR] Reservoir0History vs Reservoir0Merged Naming Confusion

**Dimension**: Clarity  
**Summary**: After temporal swap, `Reservoir0HistoryTexture` holds the merged result. But in the spatial pass, we call it `MergedReservoir0` in the descriptor. This is correct (after swap, history = merged), but it's confusing.

**Suggestion**: Add a comment in C++:
```cpp
// After temporal swap, History textures contain the merged result from this frame
SpatialDesc.MergedReservoir0 = Reservoir0HistoryTexture;
```

**Verdict**: Add comment.

### [MINOR] 3x3 Kernel is Small

**Dimension**: Quality  
**Summary**: A 3x3 kernel only gives 8 neighbors. Real ReSTIR uses larger neighborhoods (up to 20-30 pixels with strategic sampling).  
**Suggestion**: Start with 3x3 for stability. Phase 9 can expand to 5x5 or use Poisson disk sampling.  
**Verdict**: Accept.

### [NIT] Spatial Pass Should Run Even on First Frame

**Dimension**: Correctness  
**Summary**: On frame 1, temporal merge is a no-op (no history). Spatial merge still provides value by combining neighbors within the frame.  
**Suggestion**: Ensure spatial pass always runs.  
**Verdict**: Accept (already in plan).

---

## Revised Plan Summary

**Phase 8 Scope**: Spatial reservoir merge infrastructure + confidence visualization

1. **Spatial compute pass** merges center pixel reservoir with 3x3 neighbors using geometric weights
2. **Output**: 
   - `SpatialRadianceTexture.rgb = centerRadiance` (keep center color)
   - `SpatialRadianceTexture.a = W_merged` (merged confidence)
3. **Debug vis**: Show `M_merged` as grayscale, or show rejection mask
4. **ReBLUR input**: Use `SpatialRadianceTexture` instead of `DenoisedHDRTexture`
5. **No neighbor color copying** — avoids bleeding and theoretical incorrectness

This gives us:
- Validated spatial merge algorithm
- Confidence field in alpha channel for future use
- Clean image without color bleeding
- Foundation for Phase 9 (proper radiance storage + evaluation)

**Confidence Score**: 8/10 after revision
