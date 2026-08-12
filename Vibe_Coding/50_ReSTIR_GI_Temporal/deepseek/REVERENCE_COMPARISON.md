# HLVM ReSTIR vs RealEngine Reference — Field-by-field comparison

Companion to `PLAN.md`. Source of truth:
`/home/hangyu5/Documents/Gitrepo-Other/Graphics/raytracing/RealEngine/shaders/restir_gi/`
and `source/renderer/lighting/restir_gi.{h,cpp}`.

## Reservoir (RealEngine `reservoir.hlsli`) vs HLVM

RealEngine `Sample` = `{radiance (float3), hitT (float), rayDirection (uint/enc16x2)}`.
RealEngine `Reservoir` = `{Sample sample, float sumWeight, float M, float W}`.

- `Update(s, w, rng)`: `sumWeight+=w; M+=1; if (rng < w/sumWeight) sample=s;`
- `Merge(r, target_p, rng)`: `Update(r.sample, target_p * r.W * r.M, rng); M += r.M;`
- `W = sumWeight / max(eps, M * target_function(sample.radiance))`
- `target_function(radiance) = Luminance(radiance)` (they use luminance too, but the
  *sample is a real ray sample*, so the estimator is a legitimate RIS/ReSTIR estimator).

HLVM "reservoir" (ReSTIR_Generate_cs.hlsl) stores `{y (screen pixel coord), w_sum, M, W}`
where `y` points into the denoised radiance texture and `p_hat = Luminance(that pixel)`.
It never stores a direction/hitT, so it cannot be reprojected meaningfully or merged with
visibility/jacobian. That is the core divergence.

## Pipeline stage mapping (RealEngine `restir_gi.cpp` → HLVM `FReSTIRPass`)

| Stage | RealEngine | HLVM | Gap |
|---|---|---|---|
| Initial sampling | half-res, trace 1 hemisphere ray/pixel, store sample | sample denoised radiance at neighbor pixels | domain + resolution |
| Temporal | reproject via `mtxPrevViewProjection(Inverse)` + velocity, depth/normal validate, `R.Update` | identity matrices, depth/normal compare only | reprojection dead |
| Spatial | 2 passes, `Rs.Merge(Rn, target_p, rng)`, search radius 16→8 | one 3x3 pass, `Σ(w_i r_i)/Σ(M_i p_i)` | merge is a box blur |
| Resolve | bilinear upscale half→full with depth/normal weights, optional SH output | none (ReSTIR writes full-res RGBA directly) | missing |
| Denoiser | NRD / Custom / ReBLUR on resolve output | ReBLUR present but skipped in driver | wiring |

## Key constants / storage formats (RealEngine)

- Half-res working size: `half = (full+1)/2`; reservoirs stored at half-res.
- `sampleRadiance`: RGBA16F (a=hitT); `rayDirection`: R32UI (enc16x2); `reservoir`: RG16F (M,W).
- `depthNormal`: RG32UI (asuint(depth), encNormal16x2).
- M clamp: `min(M, 30.0)`.
- Temporal validity: `length(worldPos - prevWorldPos) > 0.1*linDepth` OR
  `saturate(dot(n,prevN)) < 0.8` ⇒ invalidate (fall back to current sample).
- Spatial rejection: `length(worldPos-worldPos_qn) > 0.05*linDepth` OR `saturate(dot)<0.9`.

## What to copy verbatim

- `reservoir.hlsli` `Reservoir::Update/Merge`, `TargetFunction` — directly usable.
- Temporal/Spatial reprojection + validation logic.
- Resolve bilinear + depth/normal weights (adapt `bilinear.hlsli` to explicit SRVs).

## What to keep from HLVM

- `GIPathTracing.hlsl` GI tracer (64B payload, NEE-in-ClosestHit, compact payload rules).
- GBuffer PT raster pass.
- `FGIPass` dispatch and binding infrastructure (fix the SRV read issue, keep the layout).
- ReBLUR pass (wire it after resolve).
