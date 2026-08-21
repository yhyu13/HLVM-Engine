# PLAN — Port ZetaRay's ReSTIR GI as Ground Truth (2026-08-21)

Objective (user): learn from
`/home/hangyu5/Documents/Gitrepo-My/ZetaRay/Source/ZetaRenderPass/IndirectLighting/ReSTIR_GI`
as ground truth and fix our ReSTIR implementation; fully test until it works
perfectly.

## 1. Ground truth (ZetaRay ReSTIR_GI) — what it actually does

Single compute pass (DXR ray queries), per pixel:

1. **Candidate generation — `RIS_InitialCandidates`**:
   - Sample the primary surface's BSDF -> `wi`, `pdf`.
   - Trace to second path vertex `x2` (position, normal, `ID`).
   - Path-trace `Lo` = incident radiance at `x2` towards the primary
     (`PathTrace`: NEE at each vertex, `throughput *= f/pdf`, Russian
     roulette).
   - `target = Lo * f(x1, wi)` where `f` **includes `|cos|`** (rendering
     equation convention, `BSDF::Unified().f`).
   - Reservoir: `pos=x2`, `normal=x2`, `ID`, `Lo`, `M=1`, `w_sum = targetLum/pdf`,
     `W = 1/pdf`, `target_z = target`.
2. **Temporal resampling** (`TemporalResample1/2`, pairwise MIS):
   - Reproject via motion vectors; validate candidates with **plane
     heuristic** (`|dot(n, x_i - x)| <= eps * viewZ`), normal similarity,
     roughness similarity.
   - `m_curr = p_curr / (p_curr + Σ M_prev·targetLum_prev·J)` ; `w_sum *= m_curr`.
   - For each prev candidate: recompute `target_curr = Lo_prev * f(curr surface)`,
     **segment visibility test**, balance-heuristic `m_prev`, `w_prev = m_prev * targetLum_curr * W_prev`,
     stream into reservoir.
   - `W = w_sum / targetLum(selected)`, `M = M_curr + Σ M_prev`.
3. **Spatial resampling** (`SpatialResample`, pairwise MIS):
   - Hammersley sample positions in radius, plane heuristic + emissive +
     normal/roughness heuristics.
   - `Compute_m_i` / `Update_m_c` / `Stream` / `End`:
     `W = w_sum / (targetLum * (1 + k))`, `M_s` accumulates.
4. **Final**: `li = target_z * W` (indirect only). Direct lighting is a
   separate pass. Outlier suppression (`w_sum > 25 * waveAvg` -> M=1).

Reservoir storage (3 textures): A = `float4(pos, asfloat(ID))`,
B = `half4(Lo, M)`, C = `float3(w_sum, W, octa-normal)`.

## 2. Our implementation — what it does today (measured 2026-08-14+)

```
GBuffer -> FGIPass (RT pipeline): 1 spp, N bounces, NEE sun + sky,
  radiance = primaryDirect + primaryAmbient + indirect  (all-in-one)
  -> Output(float4(result, avgHitT)) + OutputDirection(u2)
  -> ReSTIR Generate: reservoir0={radiance,hitT}, reservoir1={M=1,W=1,dir}
  -> Temporal: matrix reprojection (camera + turntable yaw), depth/normal
     reject, weighted merge w = target*W*M, W = sum/(M*targetSel)
  -> Spatial: 3x3 merge + weighted-average resolve (biased)
  -> Resolve (half->full) -> ReBLUR -> Accumulate(ACES) -> display
```

Gaps vs ground truth (each maps to a phase):

| # | Gap | ZetaRay | Ours | Phase |
|---|---|---|---|---|
| G1 | Candidate pdf | BSDF sample with pdf, `W=1/pdf` | cosine-lerp sample, **no pdf**, M=1/W=1 | 1 |
| G2 | Sample vertex | reservoir holds `x2 pos/normal/ID` | holds radiance+hitT+dir only | 1 |
| G3 | Lo semantics | reservoir stores **indirect** Lo at x2 | stores full pixel radiance (incl. direct+ambient) | 1 |
| G4 | Reservoir layout | 3 textures (pos/ID, Lo/M, w_sum/W/normal) | 2 textures | 1 |
| G5 | Temporal MIS | pairwise MIS m_curr/m_prev + Jacobian + plane heuristic | plain weighted merge, no Jacobian | 2 |
| G6 | Spatial MIS | pairwise MIS, `W=w_sum/(target*(1+k))` | 3x3 weighted average (biased) | 3 |
| G7 | Visibility | segment ray query in reuse | none (leak risk) | 4 |
| G8 | Final | `li = target_z * W` + separate direct | accumulated full radiance | 1+4 |
| G9 | Outliers | `w_sum > 25*waveAvg -> M=1` | none | 4 |

## 3. Phased implementation (each ends runnable + verified)

### Phase 1 — Reservoir sample model (this turn)

* FGIPass/GIPathTracing (ReSTIR copy): cosine-weighted BSDF sample with
  **pdf = cos/π**; payload carries `x2 normal + ID` (80 B); outputs:
  `Output = float4(Lo, hitT)`, `OutputDirection = float4(x2pos, asfloat(ID))`,
  `SampleInfo(u3) = float4(x2normal, pdf)`,
  `DirectTexture(u4) = primaryDirect + primaryAmbient + sky-on-primary-miss`.
  Sky pixels: invalid reservoir + sky in DirectTexture.
* Reservoir: 3 half-res RGBA32F textures per slot.
* ReSTIR Generate = RIS packaging: `f = albedo·max(NdotL,0)/π`,
  `target = Lo·f`, `w_sum = targetLum/pdf`, `W = 1/pdf`, `M = 1`.
* ReSTIR Temporal/Spatial: write/read 3-texture reservoirs (math unchanged
  this phase — Phase 2/3 replace it).
* Accumulate (ReSTIR copy): `sample = indirect + DirectTexture`.

Gate: build + run + dumps; display = direct + ReSTIR indirect; reservoirs
carry real sample state (validator extended).

### Phase 2 — Temporal resampling (pairwise MIS)

Port `TemporalResample1/2` + `FindTemporalCandidate` + `JacobianReconnectionShift`
into `ReSTIR_Temporal_cs.hlsl` (matrix + turntable reprojection kept).
Gate: M accumulates under turntable; static-camera reference converges.

### Phase 3 — Spatial resampling (pairwise MIS)

Port `SpatialResample`/`PairwiseMIS` (Hammersley, plane heuristic).
Gate: stills converge to path-traced reference; no fireflies.

### Phase 4 — Visibility + outliers

Ray query (`VK_KHR_ray_query`) in Temporal/Spatial compute passes
(TLAS + vertex/index/instance SRVs added to FReSTIRPass bindings);
`SuppressOutlierReservoirs`.
Gate: no light leaks through walls; validator 6/6.

### Phase 5 — Full validation + docs

Validator update (reservoir M/W semantics, indirect+direct split, frame
time), screenshots per phase, FIX_LOG, commit.

## 4. Reuse (don't redo)

GBuffer + per-texel materials, sun/sky model, turntable, ReBLUR, Resolve,
Accumulate/tonemap, dump/validator scaffolding, half-res dispatch.
