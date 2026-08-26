# ReSTIR GI Few-Bounce — CRITICAL DIAGNOSTIC (2026-06-06)

**Author**: System Diagnostic (post-2026-06-05 build)
**Subject**: `TestFewBounceGI` output is sparse "snow flower" noise, NOT GI
**Status**: REBUILD REQUIRED — root cause is the few-bounce GI itself, not ReSTIR
**Reference frame**: `Engine/Source/Runtime/Test/TestFewBounceGI_Data/dumps/2026-06-06_03-31-42_063_frame0004.png`

---

## 0. TL;DR

The previous critic (`claude.md` dated 2026-06-05) was **largely wrong** about ReSTIR. The current ReSTIR code is doing textbook streaming RIS / temporal merge / spatial merge. But it cannot fix the input.

**The real snow-flower source is the few-bounce GI shader** (`FewBounceGI.hlsl`):

1. **1 ray per pixel** (no integration) → extreme variance per pixel
2. **`SV_HitT` is misinterpreted as a 3-component normal** → all hit surfaces share a constant normal `(0.577, 0.577, 0.577)` after normalize → NdotL is uniform → no shading variation, just a coin-flip per pixel
3. **Constant gray albedo** → no material variation
4. **No access to InstanceID/PrimitiveID** → cannot fetch real material/normal
5. **ReSTIR resamples the noisy input within a 4×4 window** → snow-flower clusters
6. **Spatial pass uses neighbor's `y` (a random screen pixel from their local pool)** to read radiance → spreads noise to semantically unrelated pixels

The previous critic correctly identified the symptoms but mis-attributed them to ReSTIR. The fixes it prescribed (RIS, use y, alpha semantics) are **already in the code**.

---

## 1. The 4-Frame Evidence

Frame dumps: `2026-06-06_03-31-4{1,4,7,2}_frame{1..4}.png`

Observed:
- Image is 90%+ black with scattered bluish/grey speckles
- Speckle pattern is **high-frequency** and **shifts between frames** (flickering)
- The "sky" gap at the bottom is faintly visible (uniform dark)
- No coherent geometry visible, no shading, no shadows, no direct light source highlight

A "snow flower" pattern in GI terms means: noise dominates signal, the underlying illumination is below noise floor, the only visible features are the rare samples that happened to land on a bright source.

---

## 2. Root Cause #1 — `FewBounceGI.hlsl` is a coin-flip per pixel

### 2.1 One ray per pixel

```hlsl
// FewBounceGI.hlsl line 137-140
float3 rayDir = sampleHemisphereCosine(
    normal,
    random(pixelSeed, 0, 0),
    random(pixelSeed, 0, 1));
// Single TraceRay, no SPP loop
```

No `for (int s = 0; s < SPP; s++)` loop. Each pixel fires **exactly one** ray into the scene. There is no per-pixel integration over the hemisphere, no Russian roulette, no splitting. The output is fundamentally 1-SPP Monte Carlo.

**Expected behavior of 1-SPP GI**: extremely high variance, noise dominated, requires heavy denoising (which we don't have).

### 2.2 The "normal" is `SV_HitT`, not a real normal

```hlsl
// FewBounceGI.hlsl line 229
[shader("closesthit")]
void ClosestHit(inout GIPayload payload : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    ...
    // line 239
    float3 hitNormal = normalize(hitAttr);
```

`SV_HitT` is the **ray distance T at the hit**, declared by the system as a float scalar. HLSL will accept it being declared as a `float3`, but the value broadcast is `(T, T, T)`. After `normalize`, this is always `(0.577, 0.577, 0.577)` regardless of surface orientation.

**Consequence**: every surface in bounce 1, 2, 3 has identical shading. `NdotL` is constant. The hit position determines whether light arrives, but once you hit, the diffuse term is uniform.

**Why this is critical**: the second and third bounces have NO meaningful normal, so the cosine-weighted direction they emit is biased toward `(0.577, 0.577, 0.577)`. This collapses directional variation in the GI chain.

### 2.3 Constant gray albedo, no InstanceID

```hlsl
// line 242
float3 albedo = float3(0.7); // Gray material
```

Sponza has richly varying materials. The shader ignores `GBufferDiffuse`, `GBufferSpecular`, doesn't read InstanceID/PrimitiveID, and hardcodes `(0.7, 0.7, 0.7)`. Every hit returns the same color × NdotL.

### 2.4 Throughput is multiplied by 0.5 every bounce

```hlsl
// line 255
payload.throughput *= albedo * 0.5; // energy preservation
```

The `* 0.5` is an arbitrary energy multiplier that **darkens** the GI output as bounces accumulate. By bounce 3, throughput is `0.5^3 ≈ 0.125`. For a 3-bounce path to register, the path must hit **very bright** surfaces early. Most don't.

### 2.5 Miss path also drops throughput

```hlsl
// line 313-318
[shader("miss")]
void Miss(inout GIPayload payload : SV_RayPayload) {
    float3 skyRadiance = SampleSky(WorldRayDirection());
    payload.radiance += payload.throughput * skyRadiance;
    payload.flags &= ~0x01; // clear hit flag
}
```

Sky contribution is `throughput * skyColor`. With `throughput=1.0` at bounce 0, this returns the sky color directly. That part is fine. But because `flags` is cleared and the path terminates, the second bounce never happens for sky-bound paths.

### 2.6 Result of all five issues

Each pixel of `HDRTexture` is essentially:
- A coin flip on "did the ray hit something that faced the light?"
- Plus: if it hit, multiply by `albedo=0.7`, `NdotL = dot((0.577,0.577,0.577), lightDir)` (constant for all pixels!), `0.5^bounce`
- The result has near-zero correlation between neighbors → snow noise

**The bilateral denoise can't save it**: 5×5 Gaussian on 1-SPP noise just spreads the noise. There is no coherent signal to recover.

---

## 3. Root Cause #2 — ReSTIR is a screen-space resampler, not a generator

Once the input `DenoisedHDRTexture` is mostly black with sparse hits, the ReSTIR chain does the wrong thing.

### 3.1 Generation pass: tiny 4×4 neighborhood, no new rays

```hlsl
// ReSTIR_Generate_cs.hlsl line 70-71
int M = int(gConstants.NumCandidates);   // = 8
float radius = 2.0; // candidate search radius in pixels
```

`M=8` candidates, each in a 2-pixel tent-distributed offset → search window is **at most 4×4 pixels** around the center.

**What this does**:
- Reads `gRadiance.Load(int3(q, 0))` at up to 16 candidate positions
- Computes `p_hat = Luminance(candidate)`
- Selects one via streaming RIS weighted by `p_hat`
- Records `y = selected pixel position` and `W = w_sum / (M * p_hat(y))`

**What it does NOT do**:
- Cast new rays
- Sample new directions in world space
- Reuse any temporal data
- Re-evaluate the BRDF

So at frame N, every pixel selects **one of 16 nearby pixels** as its "best radiance". This is screen-space resampling of an already-noisy signal. No new information is created.

### 3.2 Spatial pass: neighbor's y is from a different surface

```hlsl
// ReSTIR_Spatial_cs.hlsl line 86-95
uint seed = uint(pixel.x * 65537u) + uint(pixel.y * 524287u) + ...;
float r = frac(float(seed) * 0.6180339887);
if (r * w_sum < nWsum) {
    y = nR0.xy;       // <-- adopt neighbor's y
    selectedPhat = nR1.y;
}
```

Each neighbor brings its own `y` (a pixel in the **neighbor's** local 4×4 window). When this spatial pass adopts the neighbor's `y`, then samples `gRadiance.Load(int3(samplePixel, 0))` at the OUTPUT, it reads a pixel that:
- Was selected by a different pixel's RIS
- Could be from a completely different surface (different normal, different depth, different material)
- Has no geometric relationship to the current pixel

For a 1-SPP noisy input, this is **spreading noise to semantically unrelated pixels**. It cannot produce coherent shading.

### 3.3 `W` weight is computed but never applied

```hlsl
// ReSTIR_Spatial_cs.hlsl line 117-130
float W = 0.0;
if (selectedPhat > 0.0 && M > 0.0) {
    W = w_sum / (M * selectedPhat);
}
W = min(W, 10.0);

// Comment: "Without MIS-aware spatial reuse, the unbiased W weight can introduce
//            high variance. For this test we output raw radiance[y] which is slightly biased
//            but stable. In production, use pairwise MIS for proper unbiased spatial reuse."
gOutput[pixel] = float4(outRadiance, 1.0);
```

**W is intentionally discarded**. The output is `outRadiance = gRadiance[y]`, unweighted. The author chose to drop the unbiased weight to "be stable". This is a deliberate bias that erases all the calibration work the reservoir was doing.

For sparse-noise input, this is catastrophic: a rare bright pixel can be picked by many neighbors and propagated, but the weighting that would normalize it is thrown out.

### 3.4 FrameIndex in the hash causes per-frame flicker

```hlsl
// ReSTIR_Generate_cs.hlsl line 76
float2 h = hash22(float2(pixel) + float2(i, gConstants.FrameIndex * 7.0 + i * 13.0));
```

The `FrameIndex` term in the hash means every frame, the candidates are different. So even with a static camera, the selected `y` changes frame-to-frame. The temporal pass's `M` clamping can prevent the w_sum from exploding, but each frame has a slightly different set of "winning" samples. **This is the literal source of the per-frame flicker observed in the dumps.**

For a static camera, the FrameIndex should NOT contribute to candidate positions. Standard practice: drive a sample-index counter (e.g. `i * 17`) and let temporal accumulation do the variance reduction.

---

## 4. Why the previous critic (2026-06-05) missed this

The previous critic claimed:
- "ReSTIR Generation doesn't actually sample — it just copies per-pixel data" → **WRONG**: it does streaming RIS with M=8 candidates
- "Spatial Reuse outputs the center pixel's radiance" → **WRONG**: line 110-113 samples at `y`, not at center
- "Alpha channel semantic mismatch" → **WRONG**: line 130 outputs `float4(outRadiance, 1.0)`, not reservoir W
- "No Q-sampling" → **WRONG**: tent distribution is Q-sampling

All four "critical bugs" have been fixed in the current code. The previous build must have been an earlier, more broken state. The current build is the result of those fixes — and now the **deeper** issue (the few-bounce GI itself) is exposed.

**Lesson logged**: A critic that only inspects the ReSTIR passes can never diagnose a problem that originates upstream. The few-bounce GI must be inspected together with the resampler to understand the end-to-end signal.

---

## 5. The signal-flow diagram

```
[GBuffer]                  clean, deterministic
   |
   v
[FewBounceGI RayGen]       <-- 1 ray/pixel, cosine sample, no SPP
   |   \-- TraceRay bounce 0
   |       \-- ClosestHit: NdotL with WRONG normal (SV_HitT, not real normal)
   |                 -> throughput *= 0.5
   |                 \-- TraceRay bounce 1 (same path)
   |                     \-- ClosestHit: same wrong normal
   |                               -> throughput *= 0.5
   |                               \-- TraceRay bounce 2
   v
[HDRTexture]               <-- SPARSE NOISE: 1-SPP, garbage normals, gray albedo
   |
   v
[BilateralDenoise 5x5]     <-- 5x5 Gaussian on 1-SPP noise -> smoothed noise
   |
   v
[DenoisedHDRTexture]       <-- smoothed sparse noise
   |
   v
[ReSTIR_Generate]          <-- 4x4 screen-space RIS, picks one of 16 neighbors
   |                           selected y flickers per frame (FrameIndex in hash)
   v
[Reservoir0/1]
   |
   v
[ReSTIR_Temporal]          <-- merges with reprojected history (static cam: same pixel)
   |                           but M is clamped to 30, so accumulation saturates fast
   v
[Reservoir0/1Merged]
   |
   v
[ReSTIR_Spatial]           <-- 3x3 neighbor merge, ADOPTS NEIGHBOR'S y
   |                           reads gRadiance at neighbor's y, which can be ANY pixel
   |                           W is computed but DISCARDED -> bias!
   v
[ReSTIROutput]             <-- sparse noise spread across more pixels
   |
   v
[Blit -> swapchain]        <-- user sees snow-flower flickering
```

---

## 6. Required fix order

### Priority 0 (must fix before ReSTIR can ever look right)

**A. Fix `FewBounceGI.hlsl` ClosestHit**:
- Drop `in float3 hitAttr : SV_HitT` (it's a scalar and is being misinterpreted)
- Use `WorldRayOrigin() + WorldRayDirection() * RayTCurrent()` for `hitPosition`
- Use the original surface's GBufferNormal as the hit normal (for first bounce); for subsequent bounces, sample GBuffer at reprojected pixel
- OR: extract `InstanceID()` / `PrimitiveIndex()` / `GeometryIndex()` to fetch per-mesh data from an SSBO
- Add SPP loop in RayGen: `for (int s = 0; s < SPP; s++)` accumulating into a per-pixel accumulator

**B. Read GBufferDiffuse / GBufferSpecular in ClosestHit**:
- The ClosestHit shader needs access to the GBuffer textures. Add bindings for `t0=Diffuse, t1=Specular, t2=Normal` (or carry them via payload lookup at hitPosition)
- For Sponza, sampling GBuffer at hit position will give a reasonable proxy for material color, but the right fix is per-mesh data

**C. Use light-source-based direct lighting**:
- The current `NdotL * albedo * g_GI.LightDir.w` is NdotL with a directional light. If the light has a position (point/area light), cast a shadow ray and attenuate. Sponza needs a proper light source.

### Priority 1 (improves ReSTIR quality on top of fixed GI)

**D. ReSTIR_Generate: increase search radius**:
- `radius = 2.0` is too small. Increase to `5.0-10.0` for short-range sharing.
- But more importantly: drive `q` from a per-pixel hash (no FrameIndex) so positions are temporally stable.

**E. ReSTIR_Generate: actually evaluate `p_hat` at world-space candidate directions**:
- Currently `p_hat` is "luminance of screen neighbor". Real ReSTIR GI evaluates `p_hat = BRDF * cos(theta) * G_term * lightSource` at a candidate DIRECTION, not at a candidate PIXEL.
- This requires the ReSTIR pass to do ray tracing. The current architecture treats the GI as an INPUT and re-spreads it; the real ReSTIR should be the GI generator.

**F. ReSTIR_Spatial: apply W**:
- Either output `outRadiance * W` (acceptable bias if W is calibrated) or implement pairwise MIS.
- Drop the `// NOTE: W weight is computed but not applied` comment — it should be applied.

**G. Use the FrameIndex only for unbiased path sampling, not for spatial jitter**:
- The hash inside the RIS loop can have FrameIndex for the selection step (that's fine — it's the "next" sample), but the candidate positions should be stable.

### Priority 2 (clean-up)

**H. Remove unused textures**:
- `TemporalRadianceTexture`, `RadianceHistoryTexture` are allocated but never written by a real temporal radiance pass (Temporal pass writes to `OutRadiance=TemporalRadianceTexture`, but Spatial then writes to `ReSTIROutputTexture`, ignoring the temporal radiance). Either use them or remove.

**I. Bilateral denoise sigma tuning**:
- `SpatialSigma=2.0` is heavy. `DepthSigma=0.01, NormalSigma=0.1` are tight. With 1-SPP noise and proper depth/normal guides, sigma might be too tight (allowing noisy pixels through). Increase depth/normal tolerance for first pass.

---

## 7. What the user should see vs. what is shown

Expected output of a working few-bounce GI test on Sponza:
- Coherent shading on walls, columns, ceiling
- Visible color bleeding (red curtain tints nearby surfaces)
- Soft shadows in corners and under arches
- Sky light gradient visible through openings
- ReSTIR's contribution: smoother gradients, less high-frequency noise

Actual output:
- 90% black
- Sparse high-frequency speckle
- Geometry barely visible
- No shading variation across surfaces
- Flickers per frame

The user is correct: "no real GI or direct rendering there". It is neither. It is a few-bounce GI with 1-SPP and broken ClosestHit, plus a ReSTIR resampler that cannot synthesize information it does not have.

---

## 8. Verification plan after fixes

After fixing `FewBounceGI.hlsl` ClosestHit and adding SPP:

1. **Disable ReSTIR entirely** (`bReSTIRInitialized = false`). Set `DumpTexture = DenoisedHDRTexture`. Expect to see a noisy-but-coherent GI image of Sponza with shading and color variation. If this still looks like snow, the bug is still in `FewBounceGI.hlsl`.

2. **Re-enable ReSTIR with `DebugVis = 1.0` to visualize the selected `y` per pixel**. Expect to see y locations that are clustered near bright samples (valid RIS) rather than uniformly random (broken RIS).

3. **Re-enable ReSTIR with `Radius = 8.0` and `MaxM = 100`**. Expect visible improvement in smoothness (more sharing) without washing out features (depth/normal guides still reject bad candidates).

4. **Add a debug visualize pass for `W`** (output `W` as grayscale). Expect to see W concentrated at bright sample locations, near zero elsewhere. If W is uniformly 0 or 1, the reservoir math is wrong.

5. **Final visual check**: 4-frame dump at 1s intervals. The static camera and static lighting should produce nearly identical frames. Any visible flicker is a sign that FrameIndex is corrupting candidate positions (per the fix in priority 1.D).

---

## 9. OpenWolf logging (per protocol)

Bug added to `.wolf/buglog.json`:
```json
{
  "id": "bug-restir-snow-flower-2026-06-06",
  "timestamp": "2026-06-06T03:32:00Z",
  "error_message": "Few-bounce GI output is sparse snow-flower noise, not GI",
  "file": "Engine/Source/Runtime/Test/TestFewBounceGI_Data/FewBounceGI.hlsl",
  "root_cause": "1-SPP + ClosestHit uses SV_HitT (scalar ray distance) as if it were a 3-component normal; constant gray albedo; no GBuffer sampling at hit. ReSTIR resamples an already-sparse 1-SPP input.",
  "fix": "Fix ClosestHit to use WorldRayOrigin/WorldRayDirection/RayTCurrent for hit position, sample GBuffer textures for material/normal, add SPP loop in RayGen. Then tune ReSTIR radius, remove FrameIndex from candidate positions, apply W in spatial output.",
  "tags": ["restir", "gi", "raytracing", "sponza", "noise"],
  "related_bugs": ["restir-2026-06-05-prior-critic-was-wrong"],
  "occurrences": 1,
  "last_seen": "2026-06-06T03:32:00Z"
}
```

Cerebrum entry: prior critic's failure mode — symptom-level diagnosis without inspecting upstream data flow. Future critics on ReSTIR must include a frame-dump histogram + GBuffer / input-radiance correlation check.
