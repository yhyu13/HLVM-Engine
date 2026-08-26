# ReSTIR GI Few-Bounce — CRITIC v2 (2026-06-06, post normal-fix)

**Author**: System Diagnostic
**Subject**: `TestFewBounceGI` still shows "snow flower" flickering after SV_HitT / barycentric normal fix
**Status**: Geometry is now visible, but the image is dominated by high-frequency noise that flickers per frame
**Reference frames**: `Engine/Source/Runtime/Test/TestFewBounceGI_Data/dumps/2026-06-06_07-58-5{8,9}_*_frame{0001..0004}.png`

---

## 0. TL;DR

The previous critic (`critic_2026-06-06_snowflower.md`) was **correct in its structural analysis** of the upstream bug (SV_HitT as 3-component normal, throughput ×0.5, miss=black). Those fixes have landed in the working tree:

- ✅ ClosestHit now does barycentric interpolation of vertex normals + `mul(ObjectToWorld3x4(), float4(localNormal, 0.0))` → real per-fragment normals
- ✅ C++ creates `RTVertexBuffer`, `RTIndexBuffer`, `RTInstanceInfoBuffer` and binds them at t5/t6/t7
- ✅ `payload.throughput *= albedo` (no more arbitrary `*0.5` darkening)
- ✅ Miss shader now returns `SampleSky(WorldRayDirection())` instead of black

But the prior critic's **visual symptom description was wrong** about the current state. The current frames are **not** "90% black with sparse speckle". They show:

- Sponza's arches, columns, dome, central void — all **clearly visible** (silhouette and even rough shading)
- A **heavy high-frequency noise** covering every surface (like salt-and-pepper)
- The noise pattern **shifts every frame** (per-frame flicker)

The user is correct: this is "snow flower flickering". The geometry is there, but there is no real GI, no real direct light. The shading is dominated by 1-SPP variance + ReSTIR's screen-space resampling of that variance. The flicker is a separate, traceable bug (FrameIndex inside the candidate hash).

---

## 1. The 4-Frame Evidence (re-inspected)

Frame dumps (4 frames, ~0.3 s apart, static camera, static lighting):

| Frame | What you see |
|-------|--------------|
| 0001 | Sponza visible (arches, columns, central void), heavy noise, dark center, "sky" gap at bottom |
| 0002 | Same geometry, **noise pattern has shifted** — different pixels lit |
| 0003 | Same geometry, **noise pattern has shifted again** |
| 0004 | Same geometry, **noise pattern has shifted again** |

The architecture is stable. The geometry, the depth, the GBuffer is identical across frames. Only the noise over the geometry changes.

**Visual signature**: looks like TV static overlaid on a dim, low-contrast image of Sponza. The center of the scene (dome area) is so dark it reads as black; the walls and columns are visible but uniformly gray-tinted with white-noise speckle.

**This is exactly what the user describes** as "snow flower flickering" + "no real GI or direct rendering there". The geometry is a faint outline; the signal is noise.

---

## 2. What was fixed (good)

The diff vs. `47bacc8` shows the following were fixed in the working tree:

1. **SV_HitT → real vertex normal** ([FewBounceGI.hlsl:228-255](Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl#L228-L255))
   - `Attributes { float2 barycentrics; }` declared as `SV_IntersectionAttributes`
   - `RTIndices[info.IndexOffset + primitiveIdx*3 + 0..2]` → vertex IDs
   - `RTVertices[i0/i1/i2].Normal` → 3 vertex normals
   - `bary` weights → interpolated `localNormal`
   - `mul(ObjectToWorld3x4(), float4(localNormal, 0.0))` → world-space normal
   - This is correct. **Why the prior critic was right that this fix matters**: a wrong constant normal `(0.577, 0.577, 0.577)` makes `NdotL` constant → no shading variation. Now normal is real, NdotL varies per pixel. The fact that we can see any shading on the walls in the current frames is evidence this fix worked.

2. **Hit position / next-bounce origin** ([FewBounceGI.hlsl:229-233](Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl#L229-L233))
   - `hitPosition = worldPos + worldDir * hitT` — this is the standard formula, correct.
   - `payload.origin = hitPosition + hitNormal * 0.01` — normal-biased re-spawn, standard.

3. **No more `*0.5` throughput darkening** ([FewBounceGI.hlsl:267](Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl#L267))
   - Was `payload.throughput *= albedo * 0.5;` (arbitrary 50% energy loss per bounce)
   - Now `payload.throughput *= albedo;` (Lambertian BRDF weight, principled)
   - With albedo=0.7, bounce 3 throughput is now `0.7^3 = 0.343` instead of `0.35^3 = 0.043`. ~8× brighter at depth-3.

4. **Miss shader returns sky radiance** ([FewBounceGI.hlsl:314-318](Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl#L314-L318))
   - Was effectively `payload.radiance += 0;` (only flag clear mattered)
   - Now `payload.radiance += payload.throughput * SampleSky(WorldRayDirection());`
   - With `skyIntensity=0.3` and `zenithColor=(0.4, 0.6, 1.0)` → escape paths now have visible blue/dim sky contribution.

5. **C++ RT mesh data plumbing** ([TestFewBounceGI.cpp:266-348](Engine/Source/Runtime/Test/TestFewBounceGI.cpp#L266-L348))
   - New section: "Create unified vertex/index buffers for ray tracing hit shaders"
   - Loops over `StaticMeshes`, accumulates into `AllVertices/AllIndices/InstanceInfos`
   - Creates three structured buffers, writes with `InitCmdList->writeBuffer`
   - Binding layout extended at lines 698-705: t5=StructuredBuffer_SRV, t6/t7 same
   - Binding set extended at line 1251-1253

These are real, correct fixes. The frames confirm they help — the silhouette is visible, walls have rough shading, you can tell Sponza apart from a black square.

---

## 3. What is STILL broken (the actual cause of the current "snow flower" look)

The fixes above brought us from "all-black with sparse hits" to "geometry visible under heavy noise". The remaining failures:

### 3.1 1-SPP per pixel (no SPP loop)

```hlsl
// FewBounceGI.hlsl:118-217 RayGen
// One ray, one path, no for-loop:
float3 rayDir = sampleHemisphereCosine(normal, random(pixelSeed, 0, 0), random(pixelSeed, 0, 1));
RayDesc ray = { rayOrigin, rayDir, 0.001, 1000.0 };
TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
```

Each pixel fires **exactly one** ray into the scene. There is no `for (int s = 0; s < SPP; s++)` accumulator. The only way to reduce variance is to crank `TemporalFrameCount` up via temporal accumulation — but the temporal pass **only merges reservoirs, not radiance** (see 3.7 below), so accumulation does not help.

**Variance impact**: A single ray's BRDF evaluation has variance proportional to `1/N` for N samples. For 1 sample, the variance equals the squared mean. Combined with the fact that `albedo=0.7` (uniform), `NdotL` is the only term that varies, and on a 1-SPP path the noise dominates the signal-to-noise ratio by ~order of magnitude.

### 3.2 Hardcoded gray albedo

```hlsl
// FewBounceGI.hlsl:258
// TODO: sample per-instance material albedo/texture. Using gray default for now.
float3 albedo = float3(0.7);
```

This is the **single biggest reason the image looks like noise over a faint outline**. Sponza has dramatic material variation:
- Curtain = saturated red (1.0, 0.0, 0.0 ish)
- Marble floor/columns = white
- Brick walls = tan
- Wood beams = dark brown

None of that can show up because every hit returns `(0.7, 0.7, 0.7)`. The "faint outline of geometry" you see is the variation in NdotL across surface normals — not the variation in albedo.

**Fix**: Use `GBufferDiffuse` at `hitPosition` (project to screen space, sample the diffuse texture). Even if projection is approximate (sphere-tracing the hit position is also possible), the result will be 10× more recognizable as Sponza.

### 3.3 No shadow ray / no proper light source

```hlsl
// FewBounceGI.hlsl:263-264
payload.radiance += payload.throughput * albedo * NdotL * g_GI.LightDir.w;
payload.radiance += payload.throughput * albedo * g_GI.AmbientColor.rgb * 0.1;
```

The "direct light" is a single `NdotL` term with `LightDir.w = 1.0` intensity. No shadow ray. NdotL of a back-facing surface is zero (good), but the ambient term is `AmbientColor * 0.1 = 0.03` (tiny). For a back-facing surface, the only contribution is the ambient term, which is 30× dimmer than the front-facing term.

This is why the dome in the center is so dark: arches curve away from `LightDir = normalize(sin(0), 0.6, cos(0)) = (0, 0.6, 0.94)`. The inside of the dome is in shadow (NdotL=0), ambient is `0.7*0.03=0.021`. That's ~5/255 in 8-bit. Reads as black.

**Fix**: At minimum, sample the sky in the hit normal's hemisphere as ambient (sky-occlusion ambient). For a real shadow, cast a ray from `hitPosition` toward `lightDir` and only contribute if it escapes.

### 3.4 ReSTIR Generation: FrameIndex inside the candidate hash → per-frame flicker

```hlsl
// ReSTIR_Generate_cs.hlsl:73-95
for (int i = 0; i < M; ++i) {
    float2 h = hash22(float2(pixel) + float2(i, gConstants.FrameIndex * 7.0 + i * 13.0));
    float2 offset = SampleTent(h) * radius;
    int2 q = clamp(pixel + int2(offset + float2(0.5, 0.5)), ...);
    ...
    if (h2.x * w_sum < w) { y = float2(q); }
}
```

The candidate position `q` is a function of `FrameIndex`. For a static camera and static scene, the input `DenoisedHDRTexture` is also static-ish (variance is different but the mean is the same). But because the candidate positions are different each frame, the selected `y` for each pixel is different.

Then in the spatial pass:
```hlsl
// ReSTIR_Spatial_cs.hlsl:113
float3 outRadiance = gRadiance.Load(int3(samplePixel, 0)).rgb;
```
where `samplePixel = int2(y + 0.5)`. The output for a pixel is the radiance at the selected `y`. If `y` changes frame to frame, the output changes frame to frame.

**This is the direct, traceable cause of the per-frame flicker.** The temporal pass tries to merge with the previous frame's reservoir (line 122-138 of ReSTIR_Temporal_cs.hlsl), but it can only merge weights — it cannot stabilize a per-pixel output that depends on a per-frame random selection.

**Fix**: Remove `gConstants.FrameIndex` from the candidate-position hash. Use it only for the *selection* of which candidate wins (which is fine — it's standard "next-sample" jitter). The candidate *positions* should be temporally stable.

Example fix:
```hlsl
// BEFORE
float2 h = hash22(float2(pixel) + float2(i, gConstants.FrameIndex * 7.0 + i * 13.0));
// AFTER
float2 h = hash22(float2(pixel) + float2(i * 7.0, i * 13.0));
```

### 3.5 ReSTIR Spatial: W weight is computed and discarded

```hlsl
// ReSTIR_Spatial_cs.hlsl:117-130
float W = 0.0;
if (selectedPhat > 0.0 && M > 0.0) { W = w_sum / (M * selectedPhat); }
W = min(W, 10.0);
gOutput[pixel] = float4(outRadiance, 1.0);  // <-- W is NOT used
```

The comment on line 127-129 says "Without MIS-aware spatial reuse, the unbiased W weight can introduce high variance. For this test we output raw radiance[y] which is slightly biased but stable."

**This is incorrect for 1-SPP input.** With proper ReSTIR DI, the input is 1-SPP from a spatial-neighbors generator, and W is calibrated. With a 1-SPP input that has many near-zero values and rare bright hits, the average luminance of a 4×4 window is tiny, and `p_hat` is often 0. When the chosen `y` is the rare bright pixel, `W = w_sum / (M * selectedPhat)` would be ~huge. With `W` discarded, that bright pixel is replicated 9 times (center + 8 spatial neighbors) with no normalization. Result: a bright speckle can dominate a 3×3 region.

But for the current state, the bigger issue is that the unweighted output of a 1-SPP input is exactly 1-SPP noise. Discarding W doesn't help; it just hides the problem.

**Fix**: Apply `W`. Yes, it can be high-variance on 1-SPP input, but that's the correct output. Or implement pairwise MIS for proper unbiased output. Or, simplest: output `gRadiance[y] * W` and clamp W to a small range (e.g. 0..2).

### 3.6 ReSTIR Spatial: adopts neighbor's `y` (the noise-spread bug)

```hlsl
// ReSTIR_Spatial_cs.hlsl:91-95
if (r * w_sum < nWsum) {
    y = nR0.xy;       // <-- adopt neighbor's y
    selectedPhat = nR1.y;
}
```

When a neighbor's reservoir wins the spatial merge, the current pixel adopts the neighbor's `y` (a screen pixel that is the neighbor's chosen sample). The depth/normal test (line 73-76) gates this:
```hlsl
if (dot(centerNormal, nNormal) < 0.5) continue;
if (abs(nDepth - centerDepth) > 0.05) continue;
```

For Sponza, neighboring pixels of a wall have similar normals (e.g. all facing +Z), so they pass the normal test. They have similar depth (within 0.05), so they pass the depth test. Result: a noisy bright pixel at position (x, y) can be shared with (x±1, y±1) easily. The 3×3 spatial window turns a single noisy bright sample into 9 adjacent pixels all reading the same noisy value.

This is the "snow flower" effect: not a fractal pattern, but a clustered noise where one bright sample spreads to its 8 neighbors.

**Fix**: The 3×3 window + low normal/depth thresholds is correct for the original ReSTIR DI use case (where each pixel has SPP-many samples and the merge averages them). For a 1-SPP screen-space resampler, this is too aggressive. Either:
- Tighten the normal threshold to `0.95` (only pixels with nearly identical normal can share)
- Reduce the spatial radius to a single neighbor (just the center pixel, or one random neighbor)

But really, the root fix is to make the input less noisy (3.1) so that the spatial pass doesn't have a noisy input to spread.

### 3.7 Temporal pass does not blend radiance

```hlsl
// ReSTIR_Temporal_cs.hlsl:1-2
// NOTE: Does NOT do TAA-style radiance blending. Temporal accumulation is via reservoir M/w_sum only.
```

The C++ test allocates `TemporalRadianceTexture` and `RadianceHistoryTexture`, binds `CurrentRadiance = DenoisedHDRTexture` and `HistoryRadiance = ReSTIROutputTexture` at t8/t9, and `OutRadiance = TemporalRadianceTexture` at u386. But the temporal shader does **not** have these as inputs (or outputs) — its t0..t7 are reservoirs, depths, normals. The OutRadiance u386 binding has no matching `RWTexture2D<float4>` write in the shader.

Result: the temporal pass writes a merged reservoir to `Reservoir0/1Merged`, then swap with history. Next frame, the spatial pass reads the merged reservoir. The temporal pass contributes no radiance blending.

**Consequence**: ReSTIR on a static scene with no radiance blending still flickers per frame, because each frame's output is `gRadiance[selected_y]` and `selected_y` changes per frame (see 3.4). Even with `M` clamping and reservoir merging, the *output pixel* reads from a *different input pixel* each frame.

**Fix options**:
- Option A (minimal): Don't flicker. Fix 3.4 (remove FrameIndex from candidate hash). Then the per-pixel output is stable across frames. No temporal radiance blending needed.
- Option B (proper): TAA-style radiance accumulation. The temporal pass writes `gRadiance[y] * combinedW` (or some blend) to `OutRadiance = TemporalRadianceTexture`. The spatial pass reads `TemporalRadianceTexture` instead of `DenoisedHDRTexture` for the radiance lookup. This is the "ReSTIR with temporal radiance" form (Bitterli et al. 2020 §5).
- Option C (drop the unused binding): remove `TemporalRadianceTexture`, `RadianceHistoryTexture`, and the t8/t9/u386 bindings. Acknowledge that temporal is reservoir-only.

### 3.8 Generation radius is too small

```hlsl
// ReSTIR_Generate_cs.hlsl:71
float radius = 2.0; // candidate search radius in pixels
```

With `M=8` candidates, each in a 2-pixel tent offset, the search window is at most 4×4 pixels around the center. For 1-SPP noise, this is way too small to find a "good" bright sample by RIS. The reservoir always picks from a tiny noisy pool.

**Fix**: Increase radius to `5.0` or `8.0`. But with a larger radius, the depth/normal test must be tighter, otherwise bright pixels from unrelated surfaces are adopted (snow-flower spreads further).

### 3.9 Bilateral denoise can't fix 1-SPP noise

```cpp
// TestFewBounceGI.cpp:1285-1287
Desc.DepthSigma = 0.01f;      // Sharp depth edges
Desc.NormalSigma = 0.1f;      // Sharp normal edges
Desc.SpatialSigma = 2.0f;     // 5x5 kernel effective radius
```

A 5×5 kernel with `SpatialSigma=2.0` and tight depth/normal gates is a good **edge-preserving** denoiser, but it requires an input that has *some* signal. For 1-SPP GI noise, the input is dominated by variance. The denoise will produce a smoother image but it cannot recover the signal that wasn't there. In flat regions it works (averages 25 samples → 25× lower variance in expectation). In textured regions, the tight depth/normal gates prevent it from averaging across surface boundaries, so noise on a single wall surface is averaged → smoother but still noisy.

**Fix**: This is downstream of 3.1. If we add SPP=8 to the few-bounce GI, the denoise becomes effective. Or use a better denoiser (BMFR, ReBLUR, OptiX denoiser).

### 3.10 `DenoisedHDRTexture` is the ReSTIR input

```cpp
// TestFewBounceGI.cpp:1317, 1396
GenDesc.RadianceTexture = DenoisedHDRTexture;
...
SpatDesc.RadianceTexture = DenoisedHDRTexture;
```

ReSTIR reads from the denoised output, not the noisy raw `HDRTexture`. The bilateral denoise smooths the noise a bit, but for 1-SPP input, it doesn't reduce variance enough. ReSTIR resamples this already-denoised input. So the snow-flower pattern is a 1-SPP signal smoothed by 5×5 bilateral then resampled by 4×4 RIS then 3×3 spatial. Each step is meant for a low-variance input.

**Fix**: Either (a) skip the denoise for the GI pass — feed noisy `HDRTexture` directly to ReSTIR (which is designed to handle noisy input); (b) add SPP to the GI pass so the denoise actually works; (c) use a better denoiser.

---

## 4. End-to-end signal flow (current state)

```
[GBuffer]                  clean, deterministic, 5 MRTs
   |
   v
[FewBounceGI RayGen]       1 ray/pixel, cosine sample, 3 bounces
   |   \-- TraceRay bounce 0
   |       \-- ClosestHit: REAL vertex normal (barycentric + ObjectToWorld3x4)
   |                       NdotL is per-fragment, but:
   |                       - albedo = 0.7 (gray, hardcoded)
   |                       - no shadow ray
   |                       - throughput *= 0.7 (Lambertian, no *0.5)
   |                       -> throughput after 3 bounces: 0.7^3 = 0.343
   |       \-- Miss: returns SampleSky (now blue/dim, was black)
   v
[HDRTexture]               <-- 1-SPP, gray albedo, sky in escape, NdotL shading
   |                            variance is dominated by 1/N
   v
[BilateralDenoise 5x5]     SpatialSigma=2.0, DepthSigma=0.01, NormalSigma=0.1
   |                        Tight gates + small kernel = cannot average out 1-SPP noise
   v
[DenoisedHDRTexture]       <-- smoothed but still noisy
   |
   v
[ReSTIR_Generate]          4x4 RIS (M=8, radius=2), p_hat = Luminance(neighbor)
   |                        FrameIndex IN candidate-position hash -> y flickers per frame
   v
[Reservoir0/1]
   |
   v
[ReSTIR_Temporal]          Merges reservoir metadata with history reservoir
   |                        Does NOT blend radiance (no TAA)
   |                        M clamped to 30 (saturates quickly)
   v
[Reservoir0/1Merged]       <-- swap with history
   |
   v
[ReSTIR_Spatial]           3x3 neighbor merge, depth/normal gate
   |                        ADOPTS NEIGHBOR'S y (snow-flower spread)
   |                        W is computed but DISCARDED -> bias
   |                        Output = gRadiance[y] (raw, no W)
   v
[ReSTIROutputTexture]      <-- smoothed 1-SPP noise spread by spatial merge
   |                        <-- per-pixel y changes per frame -> output flickers
   v
[Blit -> swapchain]        <-- user sees geometry outline + snow-flower flicker
```

---

## 5. Required fix order

### Priority 0 — make the GI image recognizable as Sponza

**A. Per-instance albedo (FewBounceGI.hlsl:258)**:
- Project `hitPosition` to screen space: `int2 screenXY = hitPosition.xy * projection.xy + projection.zw`
- Sample `GBufferDiffuse` at `screenXY` (use point sampler)
- Use that as `albedo`
- If the project lands off-screen, fall back to `(0.7, 0.7, 0.7)`
- This alone will make the image 10× more recognizable

**B. Add SPP loop in RayGen**:
- Wrap the TraceRay section in `for (int s = 0; s < SPP; s++)`
- Accumulate `payload.radiance` across SPP samples
- Divide by SPP at the end (or use Russian roulette)
- Start with SPP=4 for visibility, SPP=8 for production
- This is the single biggest variance reducer

**C. Sky-occlusion ambient**:
- In ClosestHit, sample sky in the direction `hitNormal` (the highest hemisphere)
- Add `payload.throughput * SampleSky(hitNormal) * 0.3` to radiance
- This gives back-facing surfaces a meaningful ambient term

### Priority 1 — kill the flicker

**D. Remove FrameIndex from ReSTIR_Generate candidate positions** ([ReSTIR_Generate_cs.hlsl:76](Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Generate_cs.hlsl#L76)):
- Change `hash22(float2(pixel) + float2(i, gConstants.FrameIndex * 7.0 + i * 13.0))` to `hash22(float2(pixel) + float2(i * 7.0, i * 13.0))`
- The `FrameIndex` should be used for the *selection* of which candidate wins (line 91), not the candidate's *position*
- After this fix, on a static scene, the per-pixel `y` is the same every frame

**E. Apply W in spatial output** ([ReSTIR_Spatial_cs.hlsl:130](Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Spatial_cs.hlsl#L130)):
- Change `gOutput[pixel] = float4(outRadiance, 1.0);` to `gOutput[pixel] = float4(outRadiance * W, 1.0);`
- Clamp W to `[0, 5]` to prevent outliers
- This calibrates the resampled radiance correctly

### Priority 2 — reduce ReSTIR's noise spreading

**F. Tighten spatial thresholds**:
- `NormalThreshold` from 0.5 → 0.95
- `DepthThreshold` from 0.05 → 0.01
- Or reduce `SpatialRadius` from 1.0 (3×3) to a single neighbor

**G. Increase generation radius**:
- After (B), `radius = 4.0` to give the reservoir a wider search window
- With SPP=4 input, the per-pixel noise is 4× lower, and a larger radius finds more consistent bright samples

### Priority 3 — clean up

**H. Remove or wire up the temporal radiance textures**:
- `TemporalRadianceTexture` and `RadianceHistoryTexture` are bound but the temporal shader doesn't read or write them
- Either remove the bindings, or implement TAA-style radiance accumulation in the temporal pass (Option B in 3.7)

**I. Bilateral denoise sigma tuning** (after B):
- With SPP≥4, `SpatialSigma=2.0` becomes effective
- For a 1-SPP input, increase to `SpatialSigma=4.0` to do more averaging (but expect edge halos)

---

## 6. What the user should see after Priority 0 + 1

After fixes A, B, C, D, E:

- Walls are visibly colored (red curtain, white marble, tan brick)
- Back-facing surfaces have sky-colored ambient (not pure black)
- 1-SPP noise is 4× lower (from SPP=4)
- Per-pixel output is stable across frames (FrameIndex removed)
- W calibration prevents bright speckles from dominating 9-pixel regions
- Image looks like a noisy but coherent low-light Sponza, not a snow-flower pattern

This is the minimum to validate that the pipeline is end-to-end working. After Priority 2, the image should look like a reasonable Sponza GI render with some residual noise (denoiser-grade, not sub-pixel noise).

---

## 7. What "no real GI or direct rendering" means in this state

The user is correct on both counts:

**No real direct rendering**: The "direct light" contribution is `albedo * NdotL * 1.0 * throughput`. With:
- albedo = 0.7 (uniform, no material variation)
- NdotL = max(dot(normalized_vertex_normal, (0, 0.6, 0.94)), 0)
- throughput after 1 bounce = 0.7
- intensity = 1.0

Front-facing surfaces get `(0.7 * 1.0 * 1.0) = 0.7` from the first bounce, then attenuated. Back-facing surfaces get `0`. Ambient is `(0.7 * 0.1 * 0.3) = 0.021`. So the dynamic range is `[0.021, 0.7]`. In 8-bit, that's `[5, 178]`. The dark center reads as black, the lit walls read as mid-gray. **This is uniform gray, not a real light**. A real light would have:
- A direct highlight (NdotL peak)
- A shadow (back-facing or occluded)
- A color tint (albedo variation)
- An ambient occlusion gradient

**No real GI**: Bounces 1, 2, 3 have:
- Same gray albedo (no color bleeding)
- Same NdotL with same wrong-direction light (no incoming energy from a real source)
- Throughput dropping to `0.7^3 = 0.343` by bounce 3
- The "GI" contribution is uniform gray × 0.343 — barely visible

Color bleeding (red curtain tinting nearby surfaces) is the most recognizable GI signature. It's impossible to produce when `albedo=0.7` for every surface.

So the user's statement "no real GI or direct rendering" is precisely correct. The geometry outline visible in the dumps is a byproduct of:
- The ray's hit/miss pattern (visible silhouette because ray directions follow GBuffer normals)
- The bilateral denoise blurring the per-pixel noise (which is what makes the silhouette readable)
- A small amount of NdotL shading (the walls have *some* variation)

But there is no actual shading content — no color, no shadows, no light highlights, no color bleeding. The image is, functionally, denoised 1-SPP Monte Carlo noise over a faint outline of geometry.

---

## 8. Verification plan

After Priority 0+1 fixes:

1. **Disable ReSTIR entirely** (set `bReSTIRInitialized = false`). Set `DumpTexture = DenoisedHDRTexture`. Expect to see a noisy-but-coherent GI image of Sponza with **color variation** (red curtain, white columns) and **shading variation** (lit walls vs shadowed dome). If this still looks like snow, the bug is still in `FewBounceGI.hlsl`.

2. **Re-enable ReSTIR with `DebugVis = 1.0`** (assuming the C++ passes DebugVis through to the spatial output). Expect to see y locations that are clustered near bright samples. If uniformly random, RIS is broken.

3. **Re-enable ReSTIR with the FrameIndex fix**. Capture 4 frames at 0.3 s intervals on a static camera. The output should be **identical across frames** (modulo numerical noise). If it still flickers, the temporal pass is mutating something per frame.

4. **Add a debug visualize pass for `W`** (output `W` as grayscale). Expect to see W concentrated at bright sample locations. If W is uniformly 0 or 1, the reservoir math is wrong.

5. **Add a debug visualize pass for `albedo`** (output `GBufferDiffuse` instead of `gRadiance[y]`). Expect to see Sponza's actual color. If still gray, the GBuffer is not being read.

---

## 9. OpenWolf logging (per protocol)

Bug to add to `.wolf/buglog.json`:
```json
{
  "id": "bug-restir-snow-flower-v2-2026-06-06",
  "timestamp": "2026-06-06T07:59:00Z",
  "error_message": "Post SV_HitT fix: Sponza geometry is visible but covered in snow-flower noise that flickers per frame. No color, no shadows, no light highlights.",
  "file": "Engine/Source/Runtime/Test/TestFewBounceGI_Data/ReSTIR_Generate_cs.hlsl + FewBounceGI.hlsl",
  "root_cause": "(1) 1-SPP with uniform gray albedo = no color information, (2) FrameIndex in ReSTIR_Generate candidate hash makes y flicker per frame, (3) W weight discarded in spatial output, (4) temporal pass does not blend radiance so flicker is not smoothed.",
  "fix": "(A) Sample GBufferDiffuse at hit position for albedo, (B) add SPP loop in RayGen, (C) sky-occlusion ambient in ClosestHit, (D) remove FrameIndex from candidate-position hash, (E) apply W in spatial output.",
  "tags": ["restir", "gi", "raytracing", "sponza", "noise", "flicker", "albedo"],
  "related_bugs": ["bug-restir-snow-flower-2026-06-06"],
  "occurrences": 1,
  "last_seen": "2026-06-06T07:59:00Z"
}
```

Cerebrum entries to add:
- **Key Learning**: ReSTIR's screen-space resampling can never recover a signal that the GI generator didn't produce. For 1-SPP input, the resampler just spreads the noise. SPP must be at the source.
- **Do-Not-Repeat**: When a "ReSTIR" output flickers, check `FrameIndex` inside the candidate-position hash. The standard fix is to use `FrameIndex` only for the selection step, never for candidate positions.
- **User Preference** (inferred from this task): The user prefers critic documents that distinguish "what was fixed" from "what is still broken" and list fixes in priority order. Prior critic mixed these into a single narrative, which made it harder to track progress.

---

## 10. Summary score card

| Symptom | Prior critic claim | Actual state | Status |
|---------|-------------------|--------------|--------|
| 90% black, sparse speckle | yes | no, geometry visible | **WRONG** (describes pre-fix state) |
| No real GI | yes | yes | correct |
| No real direct rendering | yes | yes | correct |
| SV_HitT as normal | yes | **fixed** | correct (was a real bug) |
| 1-SPP per pixel | yes | yes | correct (still true) |
| Gray albedo | yes | yes | correct (still true) |
| ReSTIR spreads noise | yes | yes (less aggressively now) | correct |
| FrameIndex in candidate hash | yes (called out as flicker source) | yes (still there) | correct |
| W discarded in spatial | yes | yes | correct |
| Temporal radiance not blended | no | yes | **MISSED** (this is also a flicker contributor) |

**Net assessment**: The prior critic correctly identified the structural issues. The new symptom (visible-but-flickering) was correctly predicted by some of those issues. The prior critic's visual description was wrong because the SV_HitT fix had landed before that critic was written — wait, no, the SV_HitT fix is the thing that made the geometry visible in the first place. Re-reading the prior critic: it was written *before* the fix. So the visual description was correct at the time of writing but is no longer accurate.

The current state is a **new symptom class** post-fix: the structural problems the prior critic identified (1-SPP, gray albedo, FrameIndex in hash, W discarded) are now the dominant causes of the snow-flower look. Fixing SV_HitT exposed them.
