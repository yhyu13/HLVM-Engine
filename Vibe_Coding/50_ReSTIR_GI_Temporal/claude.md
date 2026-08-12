# ReSTIR GI Temporal Implementation — CRITICAL DIAGNOSTIC REPORT

**Date**: 2026-06-05  
**Reviewer**: Senior Graphics Engineer  
**Verdict**: **REBUILD FROM ASH** — The implementation has fundamental algorithmic failures that cannot be patched.

---

## Executive Summary

The ReSTIR GI implementation is **non-functional**. It produces no meaningful GI because:

1. **ReSTIR Generation doesn't actually sample** — it just copies per-pixel data into a "reservoir" format
2. **Spatial Reuse outputs the center pixel's radiance** — the reservoir's selected sample (y) is completely ignored
3. **Alpha channel semantic mismatch** — reservoir weight (luminance) is used where hit distance is expected
4. **Missing the core ReSTIR GI algorithm** — no Q-sampling, no candidate generation, no importance resampling

---

## Root Cause Analysis

### CRITICAL BUG #1: ReSTIR Generation is a Pass-Through

**File**: `ReSTIR_Generate_cs.hlsl` (lines 59-116)

**Problem**: Each pixel reads its own radiance and stores it as the reservoir. There is **no sampling** — the code just re-formats existing data.

```hlsl
// Line 76-77: Read own radiance
float4 radianceHitDist = gRadiance.Load(int3(pixel, 0));
float3 radiance = radianceHitDist.rgb;

// Line 115-116: Store own data as "reservoir"
gReservoir0[pixel] = float4(worldPos, W);  // W = luminance(radiance)
gReservoir1[pixel] = float4(w_sum, M, pdf, hitDist);
```

**What ReSTIR should do**: Generate M candidate samples (light paths) at each pixel using Q-sampling, evaluate their radiance, select among them via RIS.

**What this code does**: Copies each pixel's data into a struct and calls it a reservoir.

**Impact**: The reservoir contains zero new information. Temporal and spatial reuse cannot improve what doesn't exist.

---

### CRITICAL BUG #2: Spatial Reuse Ignores Reservoir Selection

**File**: `ReSTIR_Spatial_cs.hlsl` (lines 203-205)

**Problem**: Despite doing geometric merging, the output is **always the center pixel's radiance**:

```hlsl
// Line 204: Ignore reservoir's y (selected sample position)
float3 centerRadiance = gRadiance.Load(int3(pixel, 0)).rgb;

// Line 205: Output center radiance, NOT reservoir-selected radiance
gOutRadiance[pixel] = float4(centerRadiance, spatial.W);
```

**What it should output**: `gRadiance.Load(int3(spatial.y, 0)).rgb` — the radiance at the reservoir's selected world position.

**Impact**: Even if temporal reuse correctly merged reservoirs with different y positions, spatial reuse throws away that information and returns the center pixel.

---

### CRITICAL BUG #3: Alpha Channel Semantic Mismatch

**Data Flow**:
1. `FewBounceGI.hlsl` outputs: `float4(radiance, 1.0)` — hitDist hardcoded to 1.0
2. `ReSTIR_Generate_cs.hlsl` stores: `hitDist = 1.0` in Reservoir1.w
3. `ReSTIR_Spatial_cs.hlsl` outputs: `SpatialRadiance.A = spatial.W` (reservoir weight)
4. `ReBLUR_cs.hlsl` reads: `hitDist = SpatialRadiance.a = spatial.W`

In ReBLUR (lines 46-54):
```hlsl
float GetNormHitDist(float hitDist, float viewZ, float roughness) {
    float A = gConstants.HitDistParams[0];  // 3.0
    float B = gConstants.HitDistParams[1];  // 0.1
    float D = gConstants.HitDistParams[3];  // -25.0
    float roughnessScale = exp2(D * roughness * roughness);
    float f = (A + abs(viewZ) * B) * roughnessScale;
    return saturate(hitDist / f);
}
```

**Problem**: `spatial.W` is luminance (0 to ~100+ for bright pixels), NOT hit distance (0 to ~10 for most scenes).

With `HitDistParams = [3.0, 0.1, 20.0, -25.0]` and `viewZ = 5.0`:
- Expected `normHitDist = 1.0 / ((3.0 + 0.5) * small) ≈ 1.0` (valid)
- Actual `normHitDist = 50.0 / ((3.0 + 0.5) * small) ≈ 10+` (clamped, breaks confidence)

**Impact**: ReBLUR's confidence estimation and anti-lag are completely broken.

---

### CRITICAL BUG #4: No Importance Sampling (RIS)

**What ReSTIR requires** (from Bitterli et al. and Ouyang et al.):

1. **Q-sampling**: Generate candidates using a proposal distribution (e.g., tent function over screen)
2. **Evaluate target**: Compute `p_hat(y) = luminance(radiance(y))` for each candidate
3. **RIS selection**: Choose among M candidates with probability proportional to `p_hat(y) / q(y)`
4. **Store result**: `y` = selected sample position, `W` = unbiased weight

**What this implementation does**:
- No Q-sampling (no secondary ray casting)
- No candidate generation (just uses self)
- No RIS selection (M=1, no choice)
- `W = w_sum / M = p_hat / 1 = luminance` — not unbiased, not useful

---

## Semantic Confusion Summary

| Stage | What It Should Be | What It Actually Is |
|-------|-------------------|---------------------|
| Reservoir0.xyz | Selected sample world position | Self world position |
| Reservoir0.w | Unbiased weight W = p_hat(y) / p(y) / M | Luminance (not unbiased) |
| Reservoir1.x | Weight sum w_sum = Sigma p_hat | Self luminance |
| Reservoir1.y | Sample count M | Always 1 |
| Reservoir1.w | Hit distance for ReBLUR | Hardcoded 1.0 |
| Spatial output A | Hit distance for ReBLUR | Reservoir weight W |
| Final output RGB | Radiance at selected y | Center pixel radiance |

---

## Why "Works on 3x3 Grid but Fails with Poisson" is Misleading

The self-review mentions the implementation passed with 3x3 grid but fails with Poisson disk. This is **not** a regression — both are broken. The 3x3 grid appeared to "work" because:

1. It was effectively just a 3x3 bilateral filter
2. The center pixel dominated the blend due to weight issues
3. No actual reservoir-based selection was happening

---

## Required Rebuild

### Phase 1: True ReSTIR Generation (RIS-based)

```hlsl
// Pseudocode for correct generation
for each pixel p:
    for i in 0..M-1:
        // Q-sample: generate candidate in screen space
        float2 q = SampleTent(p, random());
        float3 worldPos_q = gWorldPos.Load(q).rgb;
        
        // Evaluate target at candidate
        float3 radiance_q = gRadiance.Load(q).rgb;
        float p_hat = Luminance(radiance_q);
        
        // Store in reservoir R
        R.y = worldPos_q;
        R.w_sum += p_hat;
        if (rand() < p_hat / R.w_sum):
            R.selected = i;
    
    R.W = R.w_sum / M;  // Unbiased weight
```

### Phase 2: Spatial Reuse Must Use y

```hlsl
// Output radiance at reservoir's selected position
float3 selectedRadiance = gRadiance.Load(int3(spatial.y, 0)).rgb;
gOutRadiance[pixel] = float4(selectedRadiance, spatial.hitDist);
```

### Phase 3: Fix Alpha Channel

- Store actual hit distance from GI ray tracing (not hardcoded 1.0)
- Pass hit distance through the pipeline unchanged
- OR: Add a separate texture for hit distance if spatial pass can't preserve it

---

## Severity Assessment

| Bug | Severity | Fixable? |
|-----|----------|----------|
| No actual sampling | CRITICAL | No — requires rewrite |
| Spatial ignores y | CRITICAL | No — requires rewrite |
| Alpha mismatch | CRITICAL | Partially — needs semantic change |
| No RIS | CRITICAL | No — requires algorithm rewrite |

**Conclusion**: This is not a bug fix. This is a missing feature that requires architectural redesign.
