# Phase 8: ReSTIR GI — Spatial Reuse + Shading Evaluation

> **Goal**: Merge reservoirs spatially from neighboring pixels, then evaluate final shaded output that feeds into ReBLUR.

## Why Phase 8?

Phase 7b gave us temporal stability (M accumulates over frames). Phase 8 adds:
- **Spatial stability**: Merge with 3x3 neighbors to reduce noise in a single frame
- **Visible quality improvement**: Output actual RGB radiance from merged reservoirs to replace raw GI
- **Production-ready pipeline**: Spatial + temporal reuse together is the core ReSTIR algorithm

## Architecture

### New Pipeline

```
GBuffer Pass
    ↓
GI Ray Trace (3 bounces, untouched)
    ↓
Bilateral Denoise (untouched)
    ↓
ReSTIR Generation (Phase 7a — writes Current reservoirs)
    ↓
ReSTIR Temporal Reuse (Phase 7b — reads Current+History, writes Merged)
    ↓
[NEW] ReSTIR Spatial Reuse (reads Merged reservoirs, writes SpatiallyStable radiance)
    ↓
ReBLUR (now uses SpatiallyStable radiance instead of DenoisedHDRTexture)
    ↓
Blit to screen
```

### Why Replace DenoisedHDRTexture?

The spatially reused output should be significantly less noisy than the bilateral-denoised raw GI. By feeding it into ReBLUR, we get the best of both worlds:
- ReSTIR spatial reuse reduces single-frame noise
- ReBLUR temporal denoising further stabilizes over time

If the spatial reuse output is worse, we can fall back to DenoisedHDRTexture via a toggle.

### Spatial Merge Algorithm

For each pixel center:
1. Read center merged reservoir `r_center`
2. For each neighbor in 3x3 kernel (excluding center):
   a. Read neighbor merged reservoir `r_neighbor`
   b. Compute geometric weights:
      - **Normal weight**: `exp(-acos(dot(centerNormal, neighborNormal)) * normalSigma * 10.0)`
      - **Plane weight**: `exp(-planeDist * planeSigma * 1000.0)`
      - **Depth weight**: `exp(-abs(centerDepth - neighborDepth) * depthSigma * 100.0)`
   c. If all weights > threshold, accept neighbor
   d. Merge `r_center` with `r_neighbor` using same algorithm as temporal
3. Output final merged reservoir

### Shading Evaluation

After spatial merge, evaluate the selected sample:
```hlsl
float3 radiance = DenoisedHDRTexture[pixel].rgb;  // Current pixel's radiance
float W = mergedReservoir.W;

// Since our sample y is the pixel itself, the "shading" is just:
// If winner is center: use center radiance
// If winner is neighbor: use neighbor radiance (need to sample from texture)
```

Wait — our reservoirs store `worldPos` as the sample, not radiance. For proper ReSTIR shading, we need to evaluate the selected sample's contribution. But since our sample IS the pixel position, the radiance is just the radiance at that pixel.

Simplified approach for Phase 8:
- The spatial merge selects a winner pixel (center or neighbor)
- We read the radiance from the winner's pixel position
- We scale by `W_merged / W_winner` to account for the unbiased weight

Actually, even simpler: since `p(y) = 1.0` and we're sampling pixels uniformly:
- `W_merged = w_sum_merged / M_merged`
- `w_sum_merged = sum of p_hat(y_i)` for all merged samples
- `p_hat(y_i) = luminance(radiance_i)`

The final shaded output should be proportional to `W_merged`. But we need RGB, not just luminance.

For Phase 8, use this approximation:
```hlsl
float3 centerRadiance = DenoisedHDRTexture[pixel].rgb;
float3 winnerRadiance = DenoisedHDRTexture[neighborPixel].rgb; // if winner is neighbor

// Since W already encodes the average luminance of merged samples,
// we can approximate the output as:
float3 output = centerRadiance * (W_merged / max(W_center, 1e-6));
```

This preserves the center pixel's color but scales it by the merged weight ratio. If the winner is a neighbor with different color, this isn't strictly correct, but it's a reasonable approximation for Phase 8.

A better approach: store the RGB radiance in the reservoir.
- Reservoir0: (radiance.R, radiance.G, radiance.B) — but we currently store worldPos!
- We'd need to change the reservoir format.

For Phase 8, let's keep the current reservoir format and use the approximation above. In Phase 9, we can redesign the reservoir to store radiance directly.

### Alternative: Store Radiance in Reservoir

Actually, for proper spatial reuse, we NEED to know the radiance of the selected sample. If we select a neighbor's sample, we need the neighbor's radiance.

Option: Add a third reservoir texture:
- `Reservoir2`: (radiance.R, radiance.G, radiance.B, unused)

Or repurpose `Reservoir0`:
- Phase 7a/7b store worldPos in RGB
- Phase 8 doesn't actually need worldPos for shading — it needs radiance
- We can store radiance instead of worldPos

But changing the reservoir format breaks backward compatibility with Phase 7a/7b debug vis.

Better option: Keep worldPos in Reservoir0 for now, and sample radiance from `DenoisedHDRTexture` using the winner's pixel coordinate. The spatial shader already knows which pixel (center or neighbor) won, so it can read the radiance directly.

### Final Shading Algorithm

```hlsl
Reservoir spatial = SpatialMerge(centerPixel, neighbors);

float3 outputRadiance;
if (winnerIsCenter) {
    outputRadiance = DenoisedHDRTexture[centerPixel].rgb;
} else {
    outputRadiance = DenoisedHDRTexture[neighborPixel].rgb;
}

// Scale by merged weight (optional — if W already incorporates this)
// For now, just output the winner's radiance
OutputRadiance[pixel] = float4(outputRadiance, 1.0);
```

Wait, but if we just output the winner's radiance, we're essentially doing a filtered copy. The real value of ReSTIR is that `W` encodes the combined importance of all samples. For diffuse surfaces, the output should be:
`L = W * (1/π) * albedo * ...`

But since our `p_hat` was luminance and `W = w_sum / M`, the output radiance should be approximately:
`output = winnerRadiance * (W_merged / W_winner)`

However, `W_winner = w_sum_winner / M_winner`, and for a single pixel `M_winner = 1`, `w_sum_winner = luminance(winnerRadiance)`. So `W_winner = luminance(winnerRadiance)`.

And `W_merged = (w_sum_center + w_sum_neighbors) / M_merged`.

The ratio `W_merged / W_winner` tells us how much "better" the merged reservoir is compared to the single sample. If winner is center and center had high luminance, ratio ≈ 1. If winner is a bright neighbor, ratio could be > 1.

For simplicity, Phase 8 will just output `winnerRadiance` without scaling. The spatial reuse still provides value by selecting the best sample from the neighborhood. In Phase 9, we can implement proper weight scaling.

## HLSL Implementation

### ReSTIR_Spatial_cs.hlsl

```hlsl
// Inputs:
//   t0: Merged Reservoir0 (worldPos.xyz, W)
//   t1: Merged Reservoir1 (w_sum, M, pdf, hitDist)
//   t2: GBuffer normals
//   t3: GBuffer depth
//   t4: Denoised HDR radiance (for winner evaluation)
// Outputs:
//   u0: Spatially stable radiance (RGBA16F)
//   u1: Debug output (M as grayscale, or normal diff)
```

**Constants**:
```hlsl
struct FReSTIRSpatialConstants {
    float OutputSize[2];
    float RcpOutputSize[2];
    float NormalSigma;
    float PlaneSigma;
    float DepthSigma;
    float MaxM;
    float DebugVis;
};
```

**Neighbor kernel**: 3x3 (8 neighbors)

**Geometric rejection**:
```hlsl
float normalAngle = acos(saturate(dot(centerNormal, neighborNormal)));
float nWeight = exp(-normalAngle * NormalSigma * 10.0);

float3 centerViewPos = ReconstructViewPos(centerUv, centerDepth);
float3 neighborViewPos = ReconstructViewPos(neighborUv, neighborDepth);
float planeDist = abs(dot(normalize(centerViewPos - neighborViewPos), centerNormal));
float pWeight = exp(-planeDist * PlaneSigma * 1000.0);

float depthDiff = abs(centerDepth - neighborDepth);
float dWeight = exp(-depthDiff * DepthSigma * 100.0);

float geomWeight = nWeight * pWeight * dWeight;
if (geomWeight < 0.01) continue; // Reject neighbor
```

**Merge**: Same as temporal merge, but accumulate all valid neighbors:
```hlsl
Reservoir spatial = center;
float totalW = center.w_sum;

for each valid neighbor:
    spatial.M = min(spatial.M + neighbor.M, MaxM);
    spatial.w_sum += neighbor.w_sum * geomWeight;
    
    // Select winner
    if (random() < (neighbor.w_sum * geomWeight) / spatial.w_sum) {
        spatial.y = neighbor.y;
        spatial.hitDist = neighbor.hitDist;
        winnerPixel = neighborPixel;
    }

spatial.W = spatial.w_sum / max(spatial.M, 1e-6);
```

**Output**:
```hlsl
float3 outputRadiance = DenoisedHDRTexture[winnerPixel].rgb;
OutputRadiance[pixel] = float4(outputRadiance, spatial.W);
```

## C++ Implementation

### Modified FReSTIRPass

Add `DispatchSpatial`:
```cpp
struct FSpatialDesc {
    nvrhi::TextureHandle MergedReservoir0;
    nvrhi::TextureHandle MergedReservoir1;
    nvrhi::TextureHandle NormalTexture;
    nvrhi::TextureHandle DepthTexture;
    nvrhi::TextureHandle RadianceTexture;  // Denoised HDR
    nvrhi::TextureHandle OutRadiance;       // Spatially stable output
    nvrhi::TextureHandle OutDebugTexture;
    uint32_t OutputWidth, OutputHeight;
};

struct FReSTIRSpatialConstants {
    TFP32 OutputSize[2];
    TFP32 RcpOutputSize[2];
    TFP32 NormalSigma;
    TFP32 PlaneSigma;
    TFP32 DepthSigma;
    TFP32 MaxM;
    TFP32 DebugVis;
    TFP32 Pad;
};

void DispatchSpatial(nvrhi::ICommandList*, const FSpatialDesc&, const FReSTIRSpatialConstants&);
```

### Integration into TestFewBounceGI.cpp

1. **Add spatial output texture**:
```cpp
nvrhi::TextureHandle SpatialRadianceTexture;  // RGBA16F
```

2. **Create texture** in `Initialize()`.

3. **In Render()**, after temporal pass:
```cpp
// ReSTIR Spatial Reuse Pass
CmdList->setTextureState(Reservoir0HistoryTexture, ..., ShaderResource);  // Merged from temporal
CmdList->setTextureState(Reservoir1HistoryTexture, ..., ShaderResource);
CmdList->setTextureState(GBufferNormalsTexture, ..., ShaderResource);
CmdList->setTextureState(GBufferDepthTexture, ..., ShaderResource);
CmdList->setTextureState(DenoisedHDRTexture, ..., ShaderResource);
CmdList->setTextureState(SpatialRadianceTexture, ..., UnorderedAccess);

ReSTIR::FReSTIRPass::FSpatialDesc SpatialDesc;
SpatialDesc.MergedReservoir0 = Reservoir0HistoryTexture;  // After swap, history holds merged
SpatialDesc.MergedReservoir1 = Reservoir1HistoryTexture;
SpatialDesc.NormalTexture = GBufferNormalsTexture;
SpatialDesc.DepthTexture = GBufferDepthTexture;
SpatialDesc.RadianceTexture = DenoisedHDRTexture;
SpatialDesc.OutRadiance = SpatialRadianceTexture;
SpatialDesc.OutDebugTexture = ReSTIRDebugTexture;
SpatialDesc.OutputWidth = CurrentFBInfo.width;
SpatialDesc.OutputHeight = CurrentFBInfo.height;

ReSTIR::FReSTIRSpatialConstants SpatialConstants;
SpatialConstants.NormalSigma = 0.1f;
SpatialConstants.PlaneSigma = 100.0f;
SpatialConstants.DepthSigma = 0.01f;
SpatialConstants.MaxM = 30.0f;
SpatialConstants.DebugVis = g_ReSTIRDebugVis ? 1.0f : 0.0f;

ReSTIRPass.DispatchSpatial(CmdList, SpatialDesc, SpatialConstants);
```

4. **Feed SpatialRadianceTexture into ReBLUR**:
```cpp
ReBLURDesc.CurrentRadianceTexture = SpatialRadianceTexture;
```

5. **Fallback**: If spatial reuse is disabled, keep using DenoisedHDRTexture.

## Register Map (Spatial Shader)

| Register | Name | Type | Purpose |
|----------|------|------|---------|
| b0 | `gConstants` | cbuffer | Spatial constants |
| t0 | `gMergedReservoir0` | Texture2D | Merged reservoir 0 |
| t1 | `gMergedReservoir1` | Texture2D | Merged reservoir 1 |
| t2 | `gNormals` | Texture2D | GBuffer normals |
| t3 | `gDepth` | Texture2D | Linear depth |
| t4 | `gRadiance` | Texture2D | Denoised HDR radiance |
| u0 | `gOutRadiance` | RWTexture2D | Spatially stable radiance |
| u1 | `gDebugOutput` | RWTexture2D | Debug visualization |

## Build Integration

- `ShaderMake.cfg`: Add `ReSTIR_Spatial_cs.hlsl -T cs`
- `CMakeLists.txt`: Already has `FReSTIRPass.cpp`

## Testing Strategy

1. **Compile**: `./Build.sh --Config=Debug --Target=TestFewBounceGI`
2. **Run**: `./Build.sh --Config=Debug --Target=TestFewBounceGI --Test`
3. **Visual check**: Output should be smoother than DenoisedHDRTexture alone
4. **Edge preservation**: Sharp geometry edges should not blur
5. **Fallback**: Ensure pipeline works with/without spatial reuse

## Success Criteria

- [ ] Compiles without errors
- [ ] TestFewBounceGI passes
- [ ] Spatial output is visually smoother than bilateral denoise alone
- [ ] No edge bleeding across geometry boundaries
- [ ] Performance acceptable (3x3 kernel = 9x sample increase, but still cheap)

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Spatial merge over-blurs | Tune NormalSigma/PlaneSigma; use 3x3 not 5x5 |
| Geometric weights wrong | Visualize rejection mask in debug output |
| Winner radiance lookup wrong | Verify winner pixel coords are correct |
| ReBLUR input changed → artifacts | Keep fallback to DenoisedHDRTexture |

## Future Work (Phase 9+)

- Store radiance directly in reservoir (avoid texture lookup)
- 5x5 kernel with adaptive radius
- Proper BRDF evaluation for selected samples
- Environment map sampling
